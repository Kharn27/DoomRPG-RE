#!/usr/bin/env python3
"""Junction M_DEAD: real C11 map-route oracle and single SD audio-owner guards.

Never bundle original music in firmware, test fixtures or PAK. User supplies
22.05kHz mono IMA-ADPCM WAV on microSD; absent/invalid WAV fails closed.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
INC=ROOT/"include"
SRC=ROOT/"src"
music=(SRC/"esp_audio_entrance_music.cpp").read_text()
session=(SRC/"esp_native_gameplay_session.c").read_text()
loading=(SRC/"esp_native_transition_presentation.c").read_text()
ci=(ROOT.parent/".github/workflows/esp32-cyd.yml").read_text()
code=r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_loading_route.h"
int main(void) {
    assert(ESP_MAP_ID_INTRO==1U && ESP_MAP_ID_JUNCTION==9U);
    assert(strcmp(EspMapCatalog_nameForId(9U),"/junction.bsp")==0);
    assert(strcmp(EspMapCatalog_nameForId(10U),"/junction_destroyed.bsp")==0);
    assert(EspAudioLoadingRoute_forMap(1U)==ESP_AUDIO_LOADING_TRACK_M_STALKS);
    assert(EspAudioLoadingRoute_forMap(9U)==ESP_AUDIO_LOADING_TRACK_M_DEAD);
    assert(strcmp(EspAudioLoadingRoute_trackName(
        ESP_AUDIO_LOADING_TRACK_M_DEAD),"M_DEAD.wav")==0);
    for (unsigned i=0U;i<256U;i++) {
        if (i==1U || i==9U) continue;
        assert(EspAudioLoadingRoute_forMap((uint8_t)i)==
               ESP_AUDIO_LOADING_TRACK_DEFERRED);
    }
    assert(strcmp(EspAudioLoadingRoute_trackName(
        (EspAudioLoadingTrack)255),"none")==0);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="junction-m-dead-") as d:
    file=Path(d)/"route.c"
    exe=Path(d)/"route"
    file.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-pedantic","-I",str(INC),str(file),
                    str(SRC/"esp_map_catalog.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
assert 'kJunctionPrimary[]="/doomrpg/music/opl/M_DEAD.wav"' in music
assert 'kJunctionFallback[]="/M_DEAD.wav"' in music
assert 'kPrimary[]="/doomrpg/music/opl/M_STALKS.wav"' in music
assert 'kIntroPrimary[]="/doomrpg/music/opl/M_IN_CIT.wav"' in music
assert 'kMenuPrimary[]="/doomrpg/music/opl/M_DM2TTL.wav"' in music
assert "static EspAudioLoadingTrack activeGameplayTrack=" in music
assert "activeGameplayTrack!=gameplayTrack" in music
assert "view->targetMapId==view->gameplayLoadMapId" in music
assert "activeGameplayTrack=track;" in music
assert "LOAD_PRESTART_OWNER_NOT_READY" in music
assert "activeIntroTrack=false;\n        activeMenuTrack=false;\n        activeGameplayTrack=track;" in music
assert "!activeMenuTrack && activeGameplayTrack==track" in music
assert "route=%s ready=" in music
assert "EspEntranceMusic_service(true,true);" in music
assert "while ((uint32_t)(millis()-startMs)<1800U)" in music
assert "closeMusic(\"LOAD_START_BEFORE_BSP\")" in music
assert "activeGameplayTrack=ESP_AUDIO_LOADING_TRACK_DEFERRED;" in music
assert music.count("static File musicFile;")==1
assert music.count("static uint8_t* ring;")==1
assert "kRingCapacity=16384U" in music
assert "DAC_CHANNEL_2" in music
assert "EspAudioLoadingRoute_forMap(view->targetMapId)" in session
assert "ESP_AUDIO_LOADING_TRACK_DEFERRED &&" in session
at=session.index("EspNativeSfxProbe_service(true);")
assert at<session.index("EspEntranceMusic_prestartGameplayBeforeFirstFrame(",at)
assert at<session.index("if (!Esp32PlatformVideo_present())",at)
assert "EspAudioLoadingRoute_forMap(targetMapId)" in loading
assert "playback=deferred" in loading
assert "SD.open" not in loading
assert "Test Junction M_DEAD SD track route" in ci
print("JUNCTION-M1 HOST PASS map9=M_DEAD map1=M_STALKS "
      "254 deferred incl map10, 1 SD reader, 16KiB ring, pre-world audio")
