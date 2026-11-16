#!/usr/bin/env python3
"""AUDIO-I1: original intro WAV using bounded SD ADPCM reader and handoff."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
music=(src/"esp_audio_entrance_music.cpp").read_text()
api=(src/"esp_audio_entrance_music.h").read_text()
intro=(src/"native_intro_input.c").read_text()
clock=(root/"ESP32/include/native_intro_clock.h").read_text()
main=(src/"main.cpp").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()
assert '/doomrpg/music/opl/M_IN_CIT.wav' in music
assert '/M_IN_CIT.wav' in music
assert '/doomrpg/music/opl/M_STALKS.wav' in music
assert 'activeIntroTrack=Esp32IntroClock_isActive()!=0||introPrestartWindow;' in music
assert 'requested=activeIntroTrack?kIntroPrimary:kPrimary' in music
assert 'fallback=activeIntroTrack?kIntroFallback:kRootFallback' in music
assert 'musicFile=SD.open(requested,FILE_READ)' in music
assert 'musicFile=SD.open(fallback,FILE_READ)' in music
assert 'EspEntranceAdpcm_parse(header,n,fileBytes,&wav)' in music
assert 'expected=IMA-ADPCM-mono-22050' in music
assert 'if (!activeIntroTrack)' in music and 'mixPendingSfx();' in music
assert 'const bool intro=Esp32IntroClock_isActive()!=0||introPrestartWindow;' in music
assert 'if ((!entrance && !intro) || !soundOn)' in music
assert 'EspEntranceMusic_stopIntroAtHandoff(void)' in music
assert 'closeMusic("INTRO_EXIT_BEFORE_MAP")' in music
assert 'attempted=false; /* Entrance will own the next independent track. */' in music
assert 'EspEntranceMusic_stopIntroAtHandoff(void);' in api
assert 'EspEntranceMusic_stopIntroAtHandoff();' not in intro
assert 'Esp32IntroClock_isActive' in clock
assert 'Esp32IntroClock_park("intro-exit-ready")' in intro
assert 'EspEntranceMusic_stopIntroAtHandoff();' in main
assert main.index("EspEntranceMusic_stopIntroAtHandoff();") < main.index("Esp32IntroClock_service();")
assert 'if (!activeIntroTrack || Esp32IntroClock_isActive()) return;' in music
assert "EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);" in main
assert "kRingCapacity=16384U" in music
assert "xTaskCreate" not in music
assert "DoomRPG.zip" not in music
assert "Test AUDIO-I1" in ci
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):music.index("static uint32_t queued()")]
assert "SD." not in isr and "EspAssetPack" not in isr
assert "Esp32IntroClock" not in isr
print("PASS AUDIO-I1: INTRO M_IN_CIT SD IMA stream, fail closed, release before BSP")
