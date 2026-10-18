#!/usr/bin/env python3
"""Exercise native config record codec and explicit desktop Config retirement.

Physical SD rename and PWM are checked on the classic CYD; host checks wire
format, strict decode validation, and startup/owner invariants.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
INC = ROOT / "ESP32/include"
start = (SRC / "main.cpp").read_text()
owner = (SRC / "esp_native_config.cpp").read_text()
bridge = (SRC / "esp_legacy_game_bridge.c").read_text()
startup = (SRC / "esp_legacy_config_mappings_startup.c").read_text()
menu = (SRC / "native_main_menu_options_back.c").read_text()
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
old_tu = SRC / "esp_doomrpg_config_read.c"

assert not old_tu.exists()
assert "Game_loadConfig(" not in bridge
assert "Game_loadConfig(" not in startup
assert 'SDL_RWFromFile("Config"' not in startup
assert "retired_config_readers" in builder
assert "File_readByte(SDL_RWops* rw)" in builder
assert "File_readInt(SDL_RWops* rw)" in builder
assert 'EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);' in start
assert start.index("initializePlatformVideo();") < start.index("initializeSdCard();")
assert start.index("initializeSdCard();") < start.index("EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);")
assert start.index("EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);") < start.index("initializeGameAssets();")
assert 'constexpr char kConfigPath[] = "/DoomRPG-ESP32.cfg";' in owner
assert 'constexpr char kTempPath[] = "/DoomRPG-ESP32.cfg.tmp";' in owner
assert 'constexpr char kBackupPath[] = "/DoomRPG-ESP32.cfg.bak";' in owner
assert "reason=first-run-missing percent=100 write=no" in owner
assert "readExact(kConfigPath, &loaded)" in owner
assert "readExact(kBackupPath, &loaded)" in owner
assert "verify(kTempPath, percent)" in owner
assert "verify(kConfigPath, percent)" in owner
assert "SD.rename(kConfigPath, kBackupPath)" in owner
assert "SD.rename(kTempPath, kConfigPath)" in owner
assert "SD.rename(kBackupPath, kConfigPath)" in owner
assert "if (!EspNativeConfig_saveBrightness(after))" in menu
assert menu.index("EspNativeConfig_saveBrightness(after)") < menu.index("Esp32Backlight_setPercent(after)")
assert "SDL_RenderPresent(NULL)" in menu

with tempfile.TemporaryDirectory(prefix="native-cyd-config-") as d:
    exe = Path(d) / "codec"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-I", str(INC),
                    str(SRC / "esp_native_config_codec.c"),
                    str(ROOT / "ESP32/test/native_config_codec_host.c"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print("PASS native CYD config: desktop reader disconnected, first-run defaults, SD commit before PWM")
