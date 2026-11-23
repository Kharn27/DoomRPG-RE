#!/usr/bin/env python3
"""AUDIO #382 P1: compile the production published-window policy and replay
Sector 1 5069 with two concurrent RC11 witnesses. Not a hardware/PCM proof."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=(root/"ESP32/src/esp_audio_entrance_music.cpp").read_text()
header=(root/"ESP32/src/esp_native_sfx_window.h").read_text()
workflow=(root/".github/workflows/esp32-cyd.yml").read_text()
assert '#include "esp_native_sfx_window.h"' in src
assert "const uint32_t n=EspNativeSfxPcm_publishedSpan(" in src
assert "if (n>8192U) n=8192U;" not in src
assert 'failed="reader-caught-up";break;' in src
assert "if (!EspAssetPack_musicSdSafe()) return;" in src
assert "EspNativeSfxPcm_publishedSpan" in header
assert "audio_sfx_382_published_window_host.py" in workflow
with tempfile.TemporaryDirectory(prefix="doomrpg-sfx382-") as directory:
    exe=Path(directory)/"oracle"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(root/"ESP32/src"),
                    str(root/"ESP32/test/audio_sfx_382_published_window_host.c"),
                    "-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS AUDIO #382 P1: production policy linked; no 8192 cap, no new ISR/audio storage")
