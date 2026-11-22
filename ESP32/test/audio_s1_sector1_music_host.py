#!/usr/bin/env python3
"""AUDIO-S1-P1: real C11 65,536-pair routing/ADPCM oracle, source owner audit.

The original M_E1M3.wav is user-supplied on microSD; never copy it into
Git/PAK. This is a host proof, not CYD acoustic validation.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
INC=ROOT/"include"
SRC=ROOT/"src"
music=(SRC/"esp_audio_entrance_music.cpp").read_text()
session=(SRC/"esp_native_gameplay_session.c").read_text()
transition=(SRC/"esp_native_transition_presentation.c").read_text()
workflow=(ROOT.parent/".github/workflows/esp32-cyd.yml").read_text()
c=r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_loading_route.h"
#include "esp_audio_track_volume.h"
#include "esp_audio_entrance_adpcm.h"

static void u16(uint8_t* p, uint16_t n) {
    p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8);
}
static void u32(uint8_t* p, uint32_t n) {
    p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16); p[3]=(uint8_t)(n>>24);
}
static void wavFixture(uint8_t* w) {
    memset(w,0,68);
    memcpy(w,"RIFF",4);u32(w+4,60);memcpy(w+8,"WAVE",4);
    memcpy(w+12,"fmt ",4);u32(w+16,20);
    u16(w+20,0x11);u16(w+22,1);u32(w+24,22050);
    u32(w+28,11025);u16(w+32,8);u16(w+34,4);
    u16(w+36,2);u16(w+38,9);
    memcpy(w+40,"fact",4);u32(w+44,4);u32(w+48,9);
    memcpy(w+52,"data",4);u32(w+56,8);
}
int main(void) {
    assert(ESP_MAP_ID_SECTOR_1==2U);
    assert(ESP_AUDIO_LOADING_TRACK_M_E1M3!=ESP_AUDIO_LOADING_TRACK_M_STALKS);
    assert(ESP_AUDIO_LOADING_TRACK_M_E1M3!=ESP_AUDIO_LOADING_TRACK_M_DEAD);
    assert(EspAudioLoadingRoute_forMap(2U)==ESP_AUDIO_LOADING_TRACK_M_E1M3);
    assert(strcmp(EspAudioLoadingRoute_trackName(
        ESP_AUDIO_LOADING_TRACK_M_E1M3),"M_E1M3.wav")==0);
    assert(strcmp(EspAudioLoadingRoute_trackName(
        (EspAudioLoadingTrack)255),"none")==0);
    for(unsigned map=0;map<256;map++) {
        const EspAudioLoadingTrack plan=
            map==1 ? ESP_AUDIO_LOADING_TRACK_M_STALKS :
            map==2 ? ESP_AUDIO_LOADING_TRACK_M_E1M3 :
            map==9 ? ESP_AUDIO_LOADING_TRACK_M_DEAD :
            ESP_AUDIO_LOADING_TRACK_DEFERRED;
        assert(EspAudioLoadingRoute_forMap((uint8_t)map)==plan);
        for(unsigned bsp=0;bsp<256;bsp++) {
            const EspAudioLoadingTrack route=
                map==1 && bsp==1 ? ESP_AUDIO_LOADING_TRACK_M_STALKS :
                map==2 && bsp==3 ? ESP_AUDIO_LOADING_TRACK_M_E1M3 :
                map==9 && bsp==2 ? ESP_AUDIO_LOADING_TRACK_M_DEAD :
                ESP_AUDIO_LOADING_TRACK_DEFERRED;
            assert(EspAudioLoadingRoute_forView(
                (uint8_t)map,(uint8_t)bsp)==route);
        }
    }
    assert(EspAudioLoadingRoute_forView(2,2)==ESP_AUDIO_LOADING_TRACK_DEFERRED);
    assert(EspAudioLoadingRoute_forView(2,1)==ESP_AUDIO_LOADING_TRACK_DEFERRED);
    assert(EspAudioLoadingRoute_forView(2,9)==ESP_AUDIO_LOADING_TRACK_DEFERRED);
    assert(EspAudioLoadingRoute_forView(9,2)==ESP_AUDIO_LOADING_TRACK_M_DEAD);
    assert(EspAudioLoadingRoute_forView(1,1)==ESP_AUDIO_LOADING_TRACK_M_STALKS);
    assert(EspAudioLoadingRoute_forView(10,3)==ESP_AUDIO_LOADING_TRACK_DEFERRED);
    for(unsigned v=0;v<=100;v++) {
        assert(EspAudioTrackVolume_effectivePercent(
            ESP_AUDIO_LOADING_TRACK_M_E1M3,(uint8_t)v)==v);
        assert(EspAudioTrackVolume_effectivePercent(
            ESP_AUDIO_LOADING_TRACK_M_DEAD,(uint8_t)v)==(v*75)/100);
    }
    assert(EspAudioTrackVolume_effectivePercent(
        ESP_AUDIO_LOADING_TRACK_M_E1M3,101)==0);
    /* Exercise the exact IMA parser used after SD.open, not a mock format. */
    uint8_t w[68];EspEntranceAdpcmWav info={0};
    wavFixture(w);
    assert(EspEntranceAdpcm_parse(w,68,68,&info));
    assert(info.sampleFrames==9 && info.blockAlign==8);
    u32(w+24,16000);
    assert(!EspEntranceAdpcm_parse(w,68,68,&info));
    wavFixture(w);u16(w+22,2);
    assert(!EspEntranceAdpcm_parse(w,68,68,&info));
    wavFixture(w);u16(w+20,1);
    assert(!EspEntranceAdpcm_parse(w,68,68,&info));
    wavFixture(w);u32(w+56,7);
    assert(!EspEntranceAdpcm_parse(w,68,68,&info));
    wavFixture(w);u32(w+48,0);
    assert(!EspEntranceAdpcm_parse(w,68,68,&info));
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="audio-s1-p1-") as td:
    source=Path(td)/"s1.c"
    binary=Path(td)/"s1"
    source.write_text(c)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-pedantic","-I",str(INC),str(source),"-o",str(binary)],
                   cwd=ROOT.parent,check=True)
    subprocess.run([str(binary)],cwd=ROOT.parent,check=True)

# C++ music is compiled by PlatformIO builds; these guards check integration
# and known fail-closed branches rather than claiming a mocked SD playback.
assert 'kSector1Primary[]="/doomrpg/music/opl/M_E1M3.wav"' in music
assert 'kSector1Fallback[]="/M_E1M3.wav"' in music
assert "activeGameplayTrack==ESP_AUDIO_LOADING_TRACK_M_E1M3" in music
assert "requested=kSector1Primary;" in music and "fallback=kSector1Fallback;" in music
assert "const char* requested=activeIntroTrack?kIntroPrimary:" in music
assert "const char* fallback=activeIntroTrack?kIntroFallback:" in music
assert "musicFile=SD.open(requested,FILE_READ);" in music
assert "musicFile=SD.open(fallback,FILE_READ);" in music
assert 'EspEntranceAdpcm_parse(header,n,fileBytes,&wav)' in music
assert 'SILENT reason=missing_file' in music
assert 'SILENT reason=invalid_wav' in music
assert "if (!EspNativeAudioIntent_isSoundEnabled())" in music
assert "reason=SOUND_OFF beforeWorldPresent=1" in music
assert "activeGameplayTrack!=gameplayTrack" in music
assert "activeGameplayTrack=ESP_AUDIO_LOADING_TRACK_DEFERRED;" in music
assert 'closeMusic("LOAD_START_BEFORE_BSP");' in music
assert 'closeMusic("MENU_ACTION_BEFORE_PAK");' in music
assert 'closeMusic("MENU_END_ONCE");' in music
assert "ESP_NATIVE_AUDIO_STOP_ALL_FLAG" in music
assert "EspNativeAudioIntent_discardThrough(sfxSeenSequence);" in music
assert "EspNativeAudioIntent_notePlaybackDrop();" in music
assert "mixPendingSfx(); /* SFX only over real gameplay music. */" in music
assert "if (phase!=kPlay || ring==nullptr) return;" in music
assert music.count("static File musicFile;")==1
assert music.count("static uint8_t* ring;")==1
assert music.count("ring=(uint8_t*)heap_caps_malloc(")==1
assert "kRingCapacity=16384U" in music
assert "DAC_CHANNEL_2" in music and "DAC_CHANNEL_1" not in music
assert "EspAudioLoadingRoute_forMap(view->targetMapId)" in session
assert "EspAudioLoadingRoute_forMap(targetMapId)" in transition
assert 'EspAudioLoadingRoute_forView(view->targetMapId,' in music
assert "EspEntranceMusic_prestartGameplayBeforeFirstFrame(view->targetMapId)" in session
assert "Test AUDIO-S1-P1 Sector 1 native track" in workflow
print("PASS AUDIO-S1-P1: 65536 C11 map/BSP pairs, IMA format refusals, "
      "single SD music owner, prestart/LOAD/EXIT, SFX/STOP_ALL contract")
