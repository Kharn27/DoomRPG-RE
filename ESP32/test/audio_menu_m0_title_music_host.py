#!/usr/bin/env python3
"""MENU-M1: sound before first title present, one-shot across child menus."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
src = ROOT / "src"
music = (src / "esp_audio_entrance_music.cpp").read_text()
header = (src / "esp_audio_entrance_music.h").read_text()
boot = (src / "main.cpp").read_text()
resident = (src / "esp_native_resident_gameplay.c").read_text()
actions = (src / "native_main_menu_actions.c").read_text()
history = (ROOT / "test/audio_menu_m1_history.py").read_text()
workflow = (ROOT.parent / ".github/workflows/esp32-cyd.yml").read_text()

assert 'kMenuPrimary[]="/doomrpg/music/opl/M_DM2TTL.wav"' in music
assert 'kMenuFallback[]="/M_DM2TTL.wav"' in music
assert 'kMenuGainDiv=33U' in music
assert "kMenuTailStopFrames=256U" in music
assert 'static bool menuSessionLatched;' in music
assert "const bool menu=sdReady&&!gameplayActive&&!intro&&menuSessionLatched;" in music
assert "activeMenuTrack=!intro&&menu;" in music
assert "if (activeMenuTrack) return true;" in music  # Never rewind title
assert 'closeMusic("MENU_END_ONCE");' in music
assert 'if (!menu) menuSessionLatched=false;' in music
assert "if (EspAssetPack_isOpen()) return;" in music
assert "if (phase!=kPlay || !fileOpen || ring==nullptr) return;" in music
assert "if (activeMenuTrack) return;" in music
assert 'if (!activeIntroTrack && !activeMenuTrack)' in music
assert "int EspEntranceMusic_prestartMenuBeforeFirstFrame(void)" in music
assert "int EspEntranceMusic_prestartMenuBeforeFirstFrame(void);" in header

# Boot/startup only paints AFTER audio starts. Exit-to-main does the same.
a = boot.index("DoomRPG_esp32MainMenuModelBuildMain(doomRpg) != 0")
b = boot.index("EspEntranceMusic_prestartMenuBeforeFirstFrame();", a)
c = boot.index("DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV)", b)
assert a < b < c
a = resident.index("DoomRPG_esp32MainMenuModelEnter(doomRpg, MENU_MAIN)")
b = resident.index("EspEntranceMusic_prestartMenuBeforeFirstFrame();", a)
c = resident.index("DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalFNV)", b)
assert a < b < c

# No STOP on HELP, OPTIONS or slot picker. STOP only when actual LOAD begins.
a = actions.index("DoomRPG_esp32MainMenuDispatchConfirmed(")
b = actions.index("switch (action)", a)
part = actions[a:b]
assert "if (action == DOOMRPG_ESP32_MAIN_MENU_ACTION_START)" in part
assert part.count("EspEntranceMusic_stopMenuBeforeAction();") == 1
for key in ("LOAD", "OPTIONS", "HELP"):
    assert "action == DOOMRPG_ESP32_MAIN_MENU_ACTION_"+key not in part
c = actions.index('if (!EspNativeGameplaySave_hasReadableCheckpoint())', b)
d = actions.index('EspEntranceMusic_stopMenuBeforeAction();', c)
e = actions.index('DoomRPG_esp32ReleaseMainMenuMemory(doomRpg)', d)
assert c < d < e
assert "MENU-M1 action drift" in history

assert music.count('static File musicFile;') == 1
assert music.count('static uint8_t* ring;') == 1
assert 'kRingCapacity=16384U' in music
assert 'EspEntranceMusic_prestartIntroBeforeFirstFrame' in (
    src / "native_main_menu_start_action.c").read_text()
assert 'EspEntranceMusic_prestartGameplayBeforeFirstFrame' in (
    src / "esp_native_gameplay_session.c").read_text()
assert "Test AUDIO-MENU-M0 title music route and one-reader handoff" in workflow
print("[MENU-M1] PASS pre-display, one-shot EOF, menu/submenu continuity, confirmed LOAD handoff")
