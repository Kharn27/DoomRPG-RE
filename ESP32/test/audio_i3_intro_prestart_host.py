#!/usr/bin/env python3
"""AUDIO-I3: START owns a bounded audio prestart before ST_INTRO first present."""
from pathlib import Path

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
start=(src/"native_main_menu_start_action.c").read_text()
first=(src/"native_intro_first_frame.c").read_text()
audio=(src/"esp_audio_entrance_music.cpp").read_text()
header=(src/"esp_audio_entrance_music.h").read_text()
clock=(src/"native_intro_clock.c").read_text()
workflow=(root/".github/workflows/esp32-cyd.yml").read_text()

assert 'EspEntranceMusic_prestartIntroBeforeFirstFrame(void);' in header
assert '#include "esp_audio_entrance_music.h" /* I3:' in start
assert 'Esp32StoryFit_prepare(doomCanvas, doomRpg)' in start
assert '(void)EspEntranceMusic_prestartIntroBeforeFirstFrame();' in start
assert 'DoomRPG_esp32RenderFirstIntroFrame(doomRpg)' in start
assert (start.index('Esp32StoryFit_prepare(doomCanvas, doomRpg)') <
        start.index('(void)EspEntranceMusic_prestartIntroBeforeFirstFrame();') <
        start.index('DoomRPG_esp32RenderFirstIntroFrame(doomRpg)'))
assert 'DoomRPG_flushGraphics(doomRpg)' in first
assert 'Esp32IntroClock_arm(doomRpg' in first
assert 'EspEntranceMusic_prestartIntroBeforeFirstFrame' not in first
assert 'EspEntranceMusic_prestartIntroBeforeFirstFrame' not in clock
assert 'static bool introPrestartWindow;' in audio
assert 'activeIntroTrack=intro;' in audio
assert 'activeMenuTrack=!intro&&menu;' in audio
assert 'const bool intro=Esp32IntroClock_isActive()!=0||introPrestartWindow;' in audio
signature='int EspEntranceMusic_prestartIntroBeforeFirstFrame(void) {'
assert audio.count(signature)==1
pre=audio[audio.index(signature):audio.index('void EspEntranceMusic_syncIdleOutput(void)')]
assert 'if (!EspNativeAudioIntent_isSoundEnabled())' in pre
assert 'if (EspAssetPack_isOpen() || Esp32IntroClock_isActive() ||' in pre
assert 'introPrestartWindow=true;' in pre
assert 'introPrestartWindow=false;' in pre
assert 'while ((uint32_t)(millis()-startMs)<1800U)' in pre
assert 'EspEntranceMusic_service(false,true);' in pre
assert 'if (phase==kPlay) break;' in pre
assert 'if (phase==kSilent && attempted) break;' in pre
assert 'delay(1);' in pre
assert 'closeMusic("INTRO_PRESTART_TIMEOUT")' in pre
assert 'if (!ready) attempted=true;' in pre
assert 'firstFrame=NOT_PRESENTED' in pre
assert 'return ready;' in pre
assert 'kRingCapacity=16384U' in audio
assert 'kIntroGainDiv=33U' in audio and 'kEntranceGainDiv=15U' in audio
assert 'int EspEntranceMusic_prestartIntroBeforeFirstFrame' not in (
    src/"native_intro_input.c").read_text()
assert 'Test AUDIO-I3' in workflow
isr=audio[audio.index("static bool IRAM_ATTR onAudioTick"):
          audio.index("static uint32_t queued()")]
assert 'introPrestartWindow' not in isr
assert 'EspEntranceMusic_service' not in isr
assert 'SD.' not in isr
assert 'malloc' not in pre
print("PASS AUDIO-I3: bounded native START sound-before-first-picture, same ring/ISR, no F8 intro source mutation")
