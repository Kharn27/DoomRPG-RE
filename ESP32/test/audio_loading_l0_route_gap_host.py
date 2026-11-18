#!/usr/bin/env python3
"""LOAD-AUDIO-L0: native map soundtrack plan + bounded observational loading timing."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
inc=root/"ESP32/include"
header=(inc/"esp_audio_loading_route.h").read_text()
presentation=(src/"esp_native_transition_presentation.c").read_text()
audio=(src/"esp_audio_entrance_music.cpp").read_text()
main=(src/"main.cpp").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()
assert "ESP_MAP_ID_INTRO" in header
assert "ESP_AUDIO_LOADING_TRACK_DEFERRED" in header
assert "ESP_AUDIO_LOADING_TRACK_M_STALKS" in header
assert 'case ESP_MAP_ID_INTRO:' in header
assert 'case ESP_MAP_ID_JUNCTION:' in header
assert 'ESP_AUDIO_LOADING_TRACK_M_DEAD' in header
assert 'EspAudioLoadingRoute_forMap(targetMapId)' in presentation
assert "[LOADAUDIO-L0] ROUTE" in presentation
assert "[LOADAUDIO-L0] BUDGET" in presentation
assert "LOAD_AUDIO_L0_RING_MS 743U" in presentation
assert "transport=deferred reason=await-safe-SD-ownership" in presentation
assert "playback=deferred" in presentation
assert "audioPakClosedCheckpoints" in presentation
assert "audioMaxFlashCallbackGapMs" in presentation
assert 'loadAudioL0_report("ABORT")' in presentation
assert 'loadAudioL0_report("RELEASE")' in presentation
assert 'loadAudioL0_report("END")' in presentation
assert 'EspAssetPack_isOpen()' in presentation
assert 'EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady)' in main
assert 'EspEntranceMusic_prestartIntroBeforeFirstFrame' not in presentation
assert 'EspEntranceMusic_service' not in presentation
assert 'EspEntranceMusic_pumpGameplay' not in presentation
assert 'SD.open' not in presentation
assert "kRingCapacity=16384U" in audio
assert 'Test LOAD-AUDIO-L0' in ci
code=r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_loading_route.h"
int main(void) {
    assert(ESP_MAP_ID_INTRO==1U);
    assert(EspAudioLoadingRoute_forMap(1U)==ESP_AUDIO_LOADING_TRACK_M_STALKS);
    assert(strcmp(EspAudioLoadingRoute_trackName(
        EspAudioLoadingRoute_forMap(1U)),"M_STALKS.wav")==0);
    assert(ESP_MAP_ID_JUNCTION==9U);
    assert(EspAudioLoadingRoute_forMap(9U)==ESP_AUDIO_LOADING_TRACK_M_DEAD);
    assert(strcmp(EspAudioLoadingRoute_trackName(
        EspAudioLoadingRoute_forMap(9U)),"M_DEAD.wav")==0);
    for (unsigned i=0;i<256;i++) {
        if (i==1 || i==9) continue;
        assert(EspAudioLoadingRoute_forMap((uint8_t)i)==
               ESP_AUDIO_LOADING_TRACK_DEFERRED);
        assert(strcmp(EspAudioLoadingRoute_trackName(
            EspAudioLoadingRoute_forMap((uint8_t)i)),"none")==0);
    }
    assert(EspAudioLoadingRoute_forMap(10U)==ESP_AUDIO_LOADING_TRACK_DEFERRED);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-loading-l0-") as d:
    f=Path(d)/"route.c"
    e=Path(d)/"route"
    f.write_text(code)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-pedantic","-I",str(inc),str(f),"-o",str(e)],
                   cwd=root,check=True)
    subprocess.run([str(e)],cwd=root,check=True)
print("PASS LOAD-AUDIO-L0: Entrance M_STALKS + Junction M_DEAD, 254 other IDs deferred, loader remains silent")
