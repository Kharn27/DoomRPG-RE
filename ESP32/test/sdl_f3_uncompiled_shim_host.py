#!/usr/bin/env python3
"""SDL-F3: the dormant SDL shim remains an explicit host-only pixel oracle,
but no firmware source depends on it and PlatformIO excludes its TU.
ELF/compiledb checks run separately for every firmware profile.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ini = (ROOT / "ESP32/platformio.ini").read_text()
main = (ROOT / "ESP32/src/main.cpp").read_text()
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text()
fixture = (ROOT / "ESP32/test/sdl_shim_permanent_host.py").read_text()
bridge = (ROOT / "ESP32/src/platform_video_c_bridge.cpp").read_text()

block = re.search(r"(?m)^build_src_filter\s*=\s*\n(?P<body>(?:[ \t]+[^\n]*\n)+)", ini)
assert block, "build_src_filter missing"
filters = block.group("body").splitlines()
assert [s.strip() for s in filters] == ["+<*>", "-<esp32_sdl.cpp>"], filters
# SDL-F3 contract is the two production-derived diagnostic environments.
# BT-1 adds a distinct draft-only profile; it does not change the production
# filter or SDL exclusion and must not invalidate the historical oracle.
assert ini.count("extends = env:esp32-cyd") == (
    2 + int("[env:esp32-cyd-bt-test]" in ini)
)
assert "Esp32Sdl_attachDisplay" not in main
assert '#include "esp32_sdl_platform.h"' not in main
assert "videoReady = PlatformVideo_begin(&display);" in main
assert main.index("display.begin();") < main.index("initializePlatformVideo();")
assert "SDL shim host-only; compiledFirmware=no" in builder
assert 'if not os.path.isfile(join(project_src_dir, "esp32_sdl.cpp")):' in builder
assert "env.BuildSources(" not in builder
assert "void Esp32Sdl_attachDisplay(TFT_eSPI* display)" in shim
assert '#include "ESP32/src/esp32_sdl.cpp"' in fixture
assert "return PlatformVideo_present() ? 1 : 0;" in bridge
print("SDL-F3 PASS shim TU excluded from all firmware profiles;"
      " bootstrap no longer attaches SDL display; host oracle retained")
