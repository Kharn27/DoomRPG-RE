#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_asset_pack.h"
#include "esp_native_sfx_probe.h"
#include "esp_native_sfx_wav.h"

/* SFX-P0 temporary, fail-closed inventory from the verified PAK flash slot.
 * Never touch a live PAK lease, never touch the SD music File, ISR or DAC.
 * Each asset contributes at most 512 stack bytes; no malloc, no ZIP. */
namespace {
static bool scanned=false;
static const uint16_t kProbeIds[]={
    5042U,5044U,5045U,5046U,5060U,5061U,5062U,5065U,5134U
};
static constexpr size_t kMaxHeader=512U;
}
extern "C" void EspNativeSfxProbe_service(bool gameplayActive) {
    if (!gameplayActive) {
        scanned=false;
        return;
    }
    if (scanned||!EspAssetPack_isMapFlashActive()||
        !EspAssetPack_isResident()||EspAssetPack_isOpen())
        return;
    scanned=true;
    const unsigned count=(unsigned)(sizeof(kProbeIds)/sizeof(kProbeIds[0]));
    const uint32_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("[SFXP0] BEGIN assets=%u source=verified-map-flash-pak "
           "pakLease=closed music=untouched playback=no heap8=%u\n",
           count,(unsigned)before);
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[SFXP0] END status=pack-open-failed failClosed=yes\n");
        return;
    }
    uint32_t found=0U,pcm=0U,ima=0U,unsupported=0U,invalid=0U;
    for (size_t i=0U;i<sizeof(kProbeIds)/sizeof(kProbeIds[0]);i++) {
        const unsigned id=(unsigned)kProbeIds[i];
        char name[16];
        (void)snprintf(name,sizeof(name),"%04u.wav",id);
        EspAssetPackEntry entry={};
        if (!EspAssetPack_findEntry(name,&entry)||
            entry.size<12U||(entry.flags&ESP_ASSET_PACK_FLAG_DIRECTORY)) {
            printf("[SFXP0] ENTRY id=%u name=%s status=absent-or-short\n",
                   id,name);
            ++invalid;
            continue;
        }
        ++found;
        uint8_t header[kMaxHeader];
        const size_t length=entry.size<kMaxHeader?
                            (size_t)entry.size:kMaxHeader;
        if (!EspAssetPack_readRange(&entry,0U,header,length)) {
            printf("[SFXP0] ENTRY id=%u name=%s bytes=%u status=pak-read-failed\n",
                   id,name,(unsigned)entry.size);
            ++invalid;
            continue;
        }
        EspNativeSfxWavInfo meta={};
        const EspNativeSfxWavResult result=EspNativeSfxWav_probe(
            header,(uint32_t)length,entry.size,&meta);
        if (result==ESP_NATIVE_SFX_WAV_PCM) ++pcm;
        else if (result==ESP_NATIVE_SFX_WAV_IMA_MONO) ++ima;
        else if (result==ESP_NATIVE_SFX_WAV_OTHER) ++unsupported;
        else ++invalid;
        printf("[SFXP0] ENTRY id=%u name=%s bytes=%u format=0x%04x "
               "channels=%u hz=%u bits=%u blockAlign=%u samplesBlock=%u "
               "dataOffset=%u dataBytes=%u fact=%u frames=%u "
               "status=%s playback=no\n",
               id,name,(unsigned)entry.size,
               (unsigned)meta.formatTag,(unsigned)meta.channels,
               (unsigned)meta.sampleRate,(unsigned)meta.bitsPerSample,
               (unsigned)meta.blockAlign,(unsigned)meta.samplesPerBlock,
               (unsigned)meta.dataOffset,(unsigned)meta.dataBytes,
               (unsigned)meta.hasFact,(unsigned)meta.sampleFrames,
               EspNativeSfxWav_status(result));
    }
    EspAssetPack_close();
    const uint32_t after=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("[SFXP0] END scanned=%u present=%u pcm=%u ima=%u "
           "unsupported=%u invalid=%u packLease=closed "
           "music=untouched playback=no heap8=%u->%u delta=%d\n",
           count,(unsigned)found,(unsigned)pcm,(unsigned)ima,
           (unsigned)unsupported,(unsigned)invalid,
           (unsigned)before,(unsigned)after,(int32_t)after-(int32_t)before);
}
