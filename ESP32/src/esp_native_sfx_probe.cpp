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

    /* AUDIO-SFX-P3 / issue #261: 83 remaining original WAVs (including
     * player hurt and monster pain/death IDs). Keep original P0 unchanged.
     * One-shot RIFF metadata only: no SFX playback, SD, ZIP, ISR or heap. */
    static constexpr uint16_t kP3Ids[]={
        5047U, 5048U, 5049U, 5050U, 5051U, 5052U, 5053U, 5054U, 5055U, 5057U, 5058U,
        5059U, 5063U, 5064U, 5066U, 5067U, 5068U, 5069U, 5070U, 5071U, 5072U, 5073U,
        5074U, 5076U, 5077U, 5078U, 5079U, 5080U, 5081U, 5082U, 5083U, 5084U, 5085U,
        5086U, 5087U, 5088U, 5089U, 5090U, 5091U, 5092U, 5093U, 5094U, 5095U, 5096U,
        5097U, 5098U, 5099U, 5100U, 5101U, 5102U, 5103U, 5104U, 5105U, 5106U, 5107U,
        5108U, 5109U, 5110U, 5111U, 5112U, 5113U, 5114U, 5115U, 5116U, 5117U, 5118U,
        5119U, 5120U, 5121U, 5122U, 5123U, 5124U, 5125U, 5126U, 5127U, 5128U, 5129U,
        5130U, 5131U, 5133U, 5136U, 5137U, 5138U
    };
    static_assert(sizeof(kP3Ids)/sizeof(kP3Ids[0])==83U,
                  "AUDIO-SFX-P3 original catalog drift");
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[SFXP3] END status=pak-open-failed failClosed=yes\n");
        return;
    }
    uint32_t p3Present=0U,p3Pcm=0U,p3Ima=0U,p3Other=0U,p3Invalid=0U;
    printf("[SFXP3] BEGIN remaining=83 p0=9 totalOriginalWav=92 "
           "music=untouched playback=no source=verified-map-flash-pak\n");
    for (unsigned i=0U;i<83U;i++) {
        const unsigned id=(unsigned)kP3Ids[i];
        char name[16];
        (void)snprintf(name,sizeof(name),"%04u.wav",id);
        EspAssetPackEntry entry={};
        EspNativeSfxWavInfo meta={};
        EspNativeSfxWavResult result=ESP_NATIVE_SFX_WAV_BAD;
        if (EspAssetPack_findEntry(name,&entry) &&
            !(entry.flags&ESP_ASSET_PACK_FLAG_DIRECTORY) &&
            entry.size>=44U) {
            uint8_t header[kMaxHeader];
            const uint32_t len=entry.size<kMaxHeader?
                entry.size:(uint32_t)kMaxHeader;
            if (EspAssetPack_readRange(&entry,0U,header,len))
                result=EspNativeSfxWav_probe(header,len,entry.size,&meta);
            ++p3Present;
        }
        if (result==ESP_NATIVE_SFX_WAV_PCM) ++p3Pcm;
        else if (result==ESP_NATIVE_SFX_WAV_IMA_MONO) ++p3Ima;
        else if (result==ESP_NATIVE_SFX_WAV_OTHER) ++p3Other;
        else ++p3Invalid;
        const uint32_t durationMs=meta.sampleRate!=0U?
            (uint32_t)(((uint64_t)meta.sampleFrames*1000ULL)/
                       meta.sampleRate):0U;
        printf("[SFXP3] ENTRY id=%u name=%s bytes=%u "
               "format=0x%04x channels=%u hz=%u bits=%u "
               "dataOffset=%u dataBytes=%u frames=%u durationMs=%u "
               "status=%s playback=no\n",
               id,name,(unsigned)entry.size,(unsigned)meta.formatTag,
               (unsigned)meta.channels,(unsigned)meta.sampleRate,
               (unsigned)meta.bitsPerSample,
               (unsigned)meta.dataOffset,(unsigned)meta.dataBytes,
               (unsigned)meta.sampleFrames,(unsigned)durationMs,
               EspNativeSfxWav_status(result));
    }
    EspAssetPack_close();
    const uint32_t p3Heap=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("[SFXP3] END scanned=83 p0=9 totalOriginalWav=92 "
           "present=%u pcm=%u ima=%u other=%u invalid=%u "
           "heap8=%u->%u delta=%d packLease=closed "
           "music=untouched playback=no\n",
           (unsigned)p3Present,(unsigned)p3Pcm,(unsigned)p3Ima,
           (unsigned)p3Other,(unsigned)p3Invalid,(unsigned)after,
           (unsigned)p3Heap,(int32_t)p3Heap-(int32_t)after);
}
