#!/usr/bin/env python3
"""Build-contract and exact production C++ LCD INVON/INVOFF parity oracle.

Runs the real applyCydInvertedGammaProfile() with a tiny mocked TFT controller,
compiled twice with the actual environment macro. Never edits the RGB565
framebuffer or PAK. Does not claim optical calibration of a different LCD.
"""
from __future__ import annotations

import configparser
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INI = ROOT / "ESP32/platformio.ini"
VIDEO = ROOT / "ESP32/src/platform_video.cpp"

def exact_func(source: str, signature: str) -> str:
    assert source.count(signature) == 1, signature
    start = source.index(signature)
    opener = source.index("{", start)
    depth = 0
    for i in range(opener, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError("unterminated real controller function")

def main() -> None:
    ini = configparser.RawConfigParser()
    assert ini.read(INI), "missing ESP32/platformio.ini"
    base = "env:esp32-cyd"
    variant = "env:esp-cyd-inverted"
    assert ini.has_section(base) and ini.has_section(variant)
    assert set(ini.options(variant)) == {"extends", "build_flags"}, (
        "alternate env MUST inherit all production settings except its flag")
    assert ini.get(variant, "extends") == base
    base_flags = ini.get(base, "build_flags")
    variant_flags = [l.strip() for l in
                     ini.get(variant, "build_flags").splitlines() if l.strip()]
    assert variant_flags == [
        "\${env:esp32-cyd.build_flags}",
        "-D DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1",
        "-I.pio/libdeps/esp-cyd-inverted/TFT_eSPI",
    ], "alternate env changes more than LCD inversion/build include"
    assert "-D TFT_INVERSION_ON" in base_flags
    assert "-D TFT_RGB_ORDER=TFT_BGR" in base_flags
    assert "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" not in base_flags
    assert ini.get(base, "lib_deps") == "bodmer/TFT_eSPI @ ^2.5.43"

    cpp = VIDEO.read_text()
    fn = exact_func(cpp, "void applyCydInvertedGammaProfile()")
    palette_match = re.search(
        r"constexpr uint8_t kCydInvertedGamma\[15\] = \{.*?\};",
        cpp, re.DOTALL)
    assert palette_match, "real ILI9341 gamma LUT missing"
    assert fn.count("invertDisplay(false)") == 1
    assert fn.count("invertDisplay(true)") == 1
    assert fn.count("setSwapBytes(true)") == 1
    assert "#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" in fn
    assert "invertDisplay(" not in cpp[cpp.index("bool PlatformVideo_present()"):
                                        cpp.index("void PlatformVideo_showTestPattern()")]

    harness = """
#include <cassert>
#include <cstdint>
struct Panel {
    int commands[4] = {0};
    int ncommands = 0, ndata = 0, invertCalls = 0, swapCalls = 0;
    bool inv = false, swap = false;
    void writecommand(int v) { assert(ncommands < 4); commands[ncommands++] = v; }
    void writedata(int) { ++ndata; }
    void invertDisplay(bool v) { inv = v; ++invertCalls; }
    void setSwapBytes(bool v) { swap = v; ++swapCalls; }
};
static Panel mock;
static Panel* platformDisplay = &mock;
""" + palette_match.group(0) + "\n" + fn + """
int main() {
    applyCydInvertedGammaProfile();
    assert(mock.ncommands == 4 &&
           mock.commands[0] == 0xf2 && mock.commands[1] == 0x26 &&
           mock.commands[2] == 0xe0 && mock.commands[3] == 0xe1);
    assert(mock.ndata == 32);  // F2/26 values + 15+15 real gamma bytes
    assert(mock.invertCalls == 1 && mock.swapCalls == 1 && mock.swap);
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    assert(!mock.inv);  // INV OFF, only physical controller difference
#else
    assert(mock.inv);   // INV ON, existing calibrated normal CYD
#endif
    return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="cyd-panel-inv-") as tmp:
        root = Path(tmp)
        src = root / "oracle.cpp"
        exe = root / "oracle"
        src.write_text(harness)
        for label, flags in (
            ("esp32-cyd", []),
            ("esp-cyd-inverted",
             ["-DDOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1"]),
        ):
            subprocess.run(
                ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 *flags, str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
            print(f"PASS {label}: real video function gamma=identical "
                  "swapBytes=true framebuffer=untouched "
                  f"LCD_INV={'OFF' if flags else 'ON'}")
    print("PASS CYD panel env: same sources, PAK, UI, TFT pins, BGR, "
          "gamma, touch, framebuffer; only LCD INV polarity differs")

if __name__ == "__main__":
    main()
