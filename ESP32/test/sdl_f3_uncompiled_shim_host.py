#!/usr/bin/env python3
"""SDL-F3: the dormant SDL shim remains an explicit host-only pixel oracle,
but no firmware source depends on it and PlatformIO excludes its TU.
ELF/compiledb checks run separately for every firmware profile.
"""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
ini = (ROOT / "ESP32/platformio.ini").read_text()
main = (ROOT / "ESP32/src/main.cpp").read_text()
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
assert not (ROOT / "ESP32/src/esp32_sdl.cpp").exists(), (
    "retired SDL shim returned to production sources")
shim_path = ROOT / "ESP32/test/oracles/esp32_sdl.cpp"
oracle_data = shim_path.read_bytes()
actual_sha = hashlib.sha1(
    b"blob " + str(len(oracle_data)).encode("ascii") + b"\0" + oracle_data
).hexdigest()
assert actual_sha == "b8385250706b8b5396c3f3808e5084ad1f318425", (
    "SDL-F33 host shim oracle bytes changed", actual_sha)
shim = oracle_data.decode("utf-8")
# F34: archived display declaration may only be imported by host oracles.
assert not (ROOT / "ESP32/include/esp32_sdl_platform.h").exists()
header_path = ROOT / "ESP32/test/oracles/esp32_sdl_platform.h"
header_bytes = header_path.read_bytes()
header_blob = hashlib.sha1(
    b"blob " + str(len(header_bytes)).encode("ascii") + b"\0" + header_bytes
).hexdigest()
assert header_blob == "6d89897ebbcccaba4dd7e041520fc7776bae6142", (
    "SDL-F34 archived display header changed", header_blob)
assert 'void Esp32Sdl_attachDisplay(TFT_eSPI* display);' in header_bytes.decode("utf-8")
assert '#include "esp32_sdl_platform.h"' in shim
for directory in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for source in directory.rglob("*"):
        if source.suffix not in (".c", ".cpp", ".h", ".hpp"):
            continue
        code = source.read_text(encoding="utf-8", errors="replace")
        assert not re.search(
            r'(?m)^\s*#\s*include\s*[<"]esp32_sdl_platform\.h[>"]',
            code), f"retired SDL display header imported by {source}"
fixture = (ROOT / "ESP32/test/sdl_shim_permanent_host.py").read_text()
bridge = (ROOT / "ESP32/src/platform_video_c_bridge.cpp").read_text()

block = re.search(r"(?m)^build_src_filter\s*=\s*\n(?P<body>(?:[ \t]+[^\n]*\n)+)", ini)
assert block, "build_src_filter missing"
filters = block.group("body").splitlines()
# SDL-F22 additionally retires the never-linked SDL_RWops BMP decoder TU.
# Require both exclusions precisely; the historical F3 shim still exists as oracle.
assert [s.strip() for s in filters] == [
    "+<*>", "-<esp32_sdl.cpp>", "-<esp32_bmp.cpp>"
], filters
# Production + TRACE/bringup + the display-only alternate CYD release profile.
# Never permit an unaccounted environment or an SDL source registration.
assert ini.count("extends = env:esp32-cyd") == 3
assert "[env:esp-cyd-inverted]" in ini
assert "Esp32Sdl_attachDisplay" not in main
assert '#include "esp32_sdl_platform.h"' not in main
assert "videoReady = PlatformVideo_begin(&display);" in main
assert main.index("display.begin();") < main.index("initializePlatformVideo();")
assert "SDL shim host-only; compiledFirmware=no" in builder
assert 'if os.path.isfile(join(project_src_dir, "esp32_sdl.cpp")):' in builder
assert 'sdl_host_oracle = join(project_dir, "test", "oracles", "esp32_sdl.cpp")' in builder
assert 'oracle=ESP32/test/oracles/esp32_sdl.cpp' in builder
assert 'Retired SDL display header returned to production includes' in builder
assert 'Missing host-only SDL display header oracle' in builder
assert "env.BuildSources(" not in builder
assert "void Esp32Sdl_attachDisplay(TFT_eSPI* display)" in shim
assert '#include "ESP32/test/oracles/esp32_sdl.cpp"' in fixture
assert "return PlatformVideo_present() ? 1 : 0;" in bridge
print("SDL-F33 PASS shim moved to exact host-only oracle; excluded from all firmware profiles;"
      " bootstrap no longer attaches SDL display; host oracle retained")
