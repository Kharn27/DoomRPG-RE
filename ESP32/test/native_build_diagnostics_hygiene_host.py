#!/usr/bin/env python3
"""GCC and source contract for clean CYD build diagnostics.

Menu C must use the real C ABI declaration (never implicit int). TFT_eSPI
owns the LCD only: SoftXpt2046 owns XPT2046. The documented upstream library
warning opt-out is scoped to TFT_eSPI and does not disable compiler warnings.
Retired desktop/SDL guards keep running but are quiet by default.
"""
import ast
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OPTIONS = (ROOT / "src/native_main_menu_options_back.c").read_text()
BRIDGE = (ROOT / "include/platform_video_c_bridge.h").read_text()
BRIDGE_CPP = (ROOT / "src/platform_video_c_bridge.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()
VIDEO = (ROOT / "src/platform_video.cpp").read_text()
TOUCH = (ROOT / "src/soft_xpt2046.cpp").read_text()
CONFIG = (ROOT / "platformio.ini").read_text()
BUILD = (ROOT / "scripts/build_engine.py").read_text()

def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)

def compile_probe(header: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as scratch:
        name = Path(scratch) / "native_present.h"
        name.write_text(header)
        code = Path(scratch) / "probe.c"
        code.write_text(
            '#include "native_present.h"\n'
            'int invoke_native_present(void) {\n'
            '    return Esp32PlatformVideo_present();\n'
            '}\n'
        )
        return subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-fsyntax-only", str(code)],
            text=True, capture_output=True, check=False
        )

require(BRIDGE.count("int Esp32PlatformVideo_present(void);") == 1,
        "native C bridge public declaration drift")
require('extern "C" int Esp32PlatformVideo_present(void)' in BRIDGE_CPP,
        "actual C-linkage bridge implementation missing")
require(OPTIONS.count('#include "platform_video_c_bridge.h"') == 1,
        "options owner must include the C bridge exactly once")
require(OPTIONS.index('#include "platform_video_c_bridge.h"')
        < OPTIONS.index("#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY"),
        "native present prototype must not depend on hitbox diagnostics")
require(OPTIONS.count("Esp32PlatformVideo_present();") >= 5,
        "menu options native present call sites missing")
good = compile_probe(BRIDGE)
require(good.returncode == 0, "native bridge header invalid:\n" + good.stderr)
bad = compile_probe(BRIDGE.replace("int Esp32PlatformVideo_present(void);", ""))
require(bad.returncode != 0 and "implicit" in bad.stderr.lower(),
        "missing prototype mutant was not rejected by -Werror")

base_flags = CONFIG.split("[env:esp32-cyd]", 1)[1].split(
    "[env:esp-cyd-inverted]", 1
)[0]
require(base_flags.count("-D DISABLE_ALL_LIBRARY_WARNINGS=1") == 1,
        "official TFT_eSPI warning opt-out missing")
require("TOUCH_CS=" not in base_flags and "-D TOUCH_CS" not in base_flags,
        "fake TFT_eSPI touch chip-select must never be configured")
require("$" + "{env:esp32-cyd.build_flags}" in CONFIG.split(
    "[env:esp-cyd-inverted]", 1
)[1].split("[env:esp32-cyd-bringup]", 1)[0],
        "inverted env must inherit the same LCD/touch policy")
require("SoftXpt2046 touchscreen" in MAIN and "SoftXpt2046::begin" in TOUCH,
        "native XPT2046 owner missing")
require("#include <TFT_eSPI.h>" in MAIN and
        "#include <TFT_eSPI.h>" in VIDEO,
        "TFT_eSPI display ownership changed")
require("-Wno-cpp" not in CONFIG and
        all(line.strip() != "-w" for line in CONFIG.splitlines()),
        "compiler warnings cannot be globally hidden")

tree = ast.parse(BUILD)
details = [n for n in ast.walk(tree)
           if isinstance(n, ast.Call) and
           isinstance(n.func, ast.Name) and n.func.id == "audit_detail"]
require(len(details) == 16, "retired-ABI audit guards must stay observable")
require('os.environ.get("DOOMRPG_BUILD_AUDIT_VERBOSE") == "1"' in BUILD,
        "opt-in retirement diagnostics missing")
require("Native source audit PASS" in BUILD and "raise RuntimeError(" in BUILD,
        "build audit conclusion or fail-closed checks removed")

print("BUILD-DIAGNOSTICS HOST PASS C present=declared mutant=reject "
      "TFT_eSPI=LCD-only nativeTouch=SoftXpt2046 "
      "retirementAudit=quiet-by-default inverted=inherited GCC=warnings-on")
