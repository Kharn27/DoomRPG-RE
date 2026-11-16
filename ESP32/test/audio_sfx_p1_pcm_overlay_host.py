#!/usr/bin/env python3
"""AUDIO-SFX-P1: first actual native 5046/5060 PCM SFX inside buffered music."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
music=(src/"esp_audio_entrance_music.cpp").read_text()
header=src/"esp_native_sfx_pcm_mix.h"
gameplay=(src/"esp_native_gameplay_action_engine.c").read_text()
pickup=(src/"esp_native_gameplay_player_resources.c").read_text()
p0=(src/"esp_native_sfx_wav.h").read_text()
assert '#include "esp_native_sfx_pcm_mix.h"' in music
assert '#include "esp_native_sfx_wav.h"' in music
assert "EspAssetPack_isMapFlashActive()" in music
assert "EspAssetPack_isResident()" in music
assert "EspAssetPack_isOpen()" in music
assert 'EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)' in music
assert 'EspAssetPack_findEntry(filename,&entry)' in music
assert 'EspAssetPack_readRange(&entry,' in music
assert 'EspAssetPack_close();' in music
assert 'source=verified-flash-pak' in music
assert 'format=PCM16-mono-8000' in music
assert 'id!=5046U && id!=5060U' in music
assert 'output+kSfxLeadFrames>kRingCapacity' in music
assert 'writer-reader<output+kSfxLeadFrames' in music
assert 'kSfxLeadFrames=1024U' in music
assert 'ring[slot]=EspNativeSfxPcm_mix(ring[slot],pcm);' in music
assert 'mixPendingSfx(); /* one native real-action intent' in music
assert music.index('if (phase==kPlay) {\n        mixPendingSfx();') > music.index('if (phase==kClock)')
assert 'if (phase!=kPlay || !fileOpen || ring==nullptr) return;' in music
assert 'phase=kPlay;' in music
assert 'RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC,code' in music
isr=music[music.index('static bool IRAM_ATTR onAudioTick'):music.index('static uint32_t queued()')]
assert 'sfx' not in isr.lower() and 'EspAssetPack' not in isr
assert 'EspEntranceMusic_pumpGameplay' in music
assert "DoomRPG.zip" not in music and "Mix_" not in music
assert 'EspNativeAudioIntent_publish(5046U,0U,3U)' in gameplay
assert gameplay.index('[CRATE] COMMIT') < gameplay.index(
    'EspNativeAudioIntent_publish(5046U,0U,3U)')
assert 'crateOutcome != ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID' in gameplay
assert 'EspNativeAudioIntent_publish(5060U,0U,3U)' in pickup
assert pickup.index('[PLAYERRES] COMMIT') < pickup.index(
    'EspNativeAudioIntent_publish(5060U,0U,3U)')
assert 'PCM' in p0 and 'ESP_NATIVE_SFX_WAV_PCM' in music
assert 'sampleMode=uncapped-interval' in music # R2f stays intact
code=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_native_sfx_pcm_mix.h"
static uint32_t countSfxIndex(uint32_t outFrames) {
    uint32_t phase=0,idx=0;
    for (uint32_t i=0;i<outFrames;i++) {
        phase+=8000;
        if (phase>=22050) {phase-=22050;idx++;}
    }
    return idx;
}
int main(void) {
    assert(EspNativeSfxPcm_outputFrames(4160,22050)==11466);
    assert(EspNativeSfxPcm_outputFrames(3520,22050)==9702);
    assert(EspNativeSfxPcm_outputFrames(13600,22050)==37485);
    assert(EspNativeSfxPcm_outputFrames(0,22050)==0);
    assert(EspNativeSfxPcm_outputFrames(4160,0)==0);
    assert(EspNativeSfxPcm_outputFrames(UINT32_MAX,22050)==0);
    assert(11466U+1024U<16384U);
    assert(9702U+1024U<16384U);
    assert(37485U+1024U>16384U);
    assert(countSfxIndex(11466)==4160);
    assert(countSfxIndex(9702)==3520);
    const uint8_t a[]={0,0,0,128,255,127,255,255};
    assert(EspNativeSfxPcm_le16(a)==0);
    assert(EspNativeSfxPcm_le16(a+2)==-32768);
    assert(EspNativeSfxPcm_le16(a+4)==32767);
    assert(EspNativeSfxPcm_le16(a+6)==-1);
    assert(EspNativeSfxPcm_mix(128,0)==128);
    assert(EspNativeSfxPcm_mix(128,512)==129);
    assert(EspNativeSfxPcm_mix(128,-512)==127);
    assert(EspNativeSfxPcm_mix(230,32767)==240);
    assert(EspNativeSfxPcm_mix(25,-32768)==16);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-sfx-p1-") as td:
    s=Path(td)/"mix.c"
    binary=Path(td)/"mix"
    s.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(s),"-o",str(binary)],check=True,cwd=root)
    subprocess.run([str(binary)],check=True,cwd=root)
print("PASS AUDIO-SFX-P1: two real native gameplay cues, PCM16/8k mix over R2e ring within 46ms")
