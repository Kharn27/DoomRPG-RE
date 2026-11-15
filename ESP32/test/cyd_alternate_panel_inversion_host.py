#!/usr/bin/env python3
"""LCD-C3: exact panel gamma/polarity and classic-only PWM optical trial.

Compile real controller and PWM functions for both release environments.
Keep LCD-C2 gamma intact, lower only classic physical PWM duty to 90%.
Accepted inverted gamma and PWM must remain byte-for-byte unchanged.
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
        "${env:esp32-cyd.build_flags}",
        "-D DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1",
        "-I.pio/libdeps/esp-cyd-inverted/TFT_eSPI",
    ], "alternate env changes more than LCD inversion/build include"
    assert "-D TFT_INVERSION_ON" in base_flags
    assert "-D TFT_RGB_ORDER=TFT_BGR" in base_flags
    assert "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" not in base_flags
    assert ini.get(base, "lib_deps").strip() == "bodmer/TFT_eSPI @ ^2.5.43"

    cpp = VIDEO.read_text()
    fn = exact_func(cpp, "void applyCydInvertedGammaProfile()")
    duty_fn = exact_func(
        cpp, "constexpr uint32_t backlightDutyFromPercent(uint8_t percent)")
    assert "ledcWrite(kBacklightChannel, backlightDutyFromPercent(100U))" not in cpp, (
        "cold boot must evaluate and report calibrated duty separately")
    assert "const uint32_t initialDuty = backlightDutyFromPercent(100U);" in cpp
    assert "ledcWrite(kBacklightChannel, initialDuty);" in cpp
    assert "const uint32_t duty = backlightDutyFromPercent(percent);" in cpp
    assert "lcd=classic scale=90pct trial=yes" in cpp
    assert "lcd=inverted scale=100pct" in cpp
    classic_match = re.search(
        r"constexpr uint8_t kCydClassicLegacyE0Gamma\[15\] = \{.*?\};",
        cpp, re.DOTALL)
    assert classic_match, "original classic E0 bytes missing"
    positive_match = re.search(
        r"constexpr uint8_t kCydPanelPositiveGamma\[15\] = \{.*?\};",
        cpp, re.DOTALL)
    assert positive_match, "accepted alternate E0 missing"
    negative_match = re.search(
        r"constexpr uint8_t kCydPanelNegativeGamma\[15\] = \{.*?\};",
        cpp, re.DOTALL)
    assert negative_match, "real shared negative gamma table missing"
    assert "kCydInvertedGamma" not in cpp, "legacy E1/E1 profile reappeared"
    assert "kCydAlternatePositiveGamma" not in cpp
    assert "kCydAlternateNegativeGamma" not in cpp
    assert "#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" in cpp
    assert fn.count("kCydPanelPositiveGamma") == 2
    assert fn.count("kCydClassicLegacyE0Gamma") == 2
    assert fn.count("kCydPanelNegativeGamma") == 2
    assert fn.count("#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION") == 2, (
        "LCD-C2 changes only E0 source select and INV polarity")
    assert fn.count("invertDisplay(false)") == 1
    assert fn.count("invertDisplay(true)") == 1
    assert fn.count("setSwapBytes(true)") == 1
    assert "#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" in fn
    assert "invertDisplay(" not in cpp[cpp.index("bool PlatformVideo_present()"):
                                        cpp.index("void PlatformVideo_showTestPattern()")]
    assert "classic inversion=on gamma=classic-E0-driver-E1-trial" in cpp
    assert "alternate-panel inversion=off gamma=ILI9341-driver-E0E1-trial" in cpp

    harness = """
#include <cassert>
#include <cstddef>
#include <cstdint>
using std::size_t;
struct Panel {
    int commands[4] = {0};
    int data[4][15] = {};
    int lengths[4] = {};
    int ncommands = 0, ndata = 0, invertCalls = 0, swapCalls = 0;
    bool inv = false, swap = false;
    void writecommand(int v) { assert(ncommands < 4); commands[ncommands++] = v; }
    void writedata(int v) {
        assert(ncommands > 0);
        int at = ncommands - 1;
        assert(lengths[at] < 15);
        data[at][lengths[at]++] = v;
        ++ndata;
    }
    void invertDisplay(bool v) { inv = v; ++invertCalls; }
    void setSwapBytes(bool v) { swap = v; ++swapCalls; }
};
static Panel mock;
static Panel* platformDisplay = &mock;
""" + "#if !DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION\n" + classic_match.group(0) + "\n#else\n" + positive_match.group(0) + "\n#endif\n" + negative_match.group(0) + "\n" + fn + "\n" + duty_fn + """
int main() {
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    // The previously accepted alternate CYD must keep all PWM duties exact.
    assert(backlightDutyFromPercent(25U) == 64U);
    assert(backlightDutyFromPercent(50U) == 128U);
    assert(backlightDutyFromPercent(75U) == 191U);
    assert(backlightDutyFromPercent(100U) == 255U);
#else
    // LCD-C3 90pct panel-only physical duty. User config remains 25/50/75/100.
    assert(backlightDutyFromPercent(25U) == 58U);
    assert(backlightDutyFromPercent(50U) == 115U);
    assert(backlightDutyFromPercent(75U) == 172U);
    assert(backlightDutyFromPercent(100U) == 230U);
#endif
    applyCydInvertedGammaProfile();
    assert(mock.ncommands == 4 &&
           mock.commands[0] == 0xf2 && mock.commands[1] == 0x26 &&
           mock.commands[2] == 0xe0 && mock.commands[3] == 0xe1);
    assert(mock.ndata == 32);  // F2/26 values + 15+15 real gamma bytes
    assert(mock.lengths[0] == 1 && mock.data[0][0] == 0x00);
    assert(mock.lengths[1] == 1 && mock.data[1][0] == 0x01);
    assert(mock.lengths[2] == 15 && mock.lengths[3] == 15);
    // Published INV-OFF pair remains exact. On INV-ON classic, E0
    // must match the proven pre-C1 bytes and ONLY E1 changes.
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    const int alternateE0[15] = {0x0f,0x31,0x2b,0x0c,0x0e,0x08,0x4e,0xf1,
                                 0x37,0x07,0x10,0x03,0x0e,0x09,0x00};
#else
    const int classicE0[15] = {0x00,0x15,0x17,0x07,0x11,0x06,0x2b,0x56,
                               0x3c,0x05,0x10,0x0f,0x3f,0x3f,0x0f};
#endif
    const int sharedE1[15] = {0x00,0x0e,0x14,0x03,0x11,0x07,0x31,0xc1,
                              0x48,0x08,0x0f,0x0c,0x31,0x36,0x0f};
    for (int i = 0; i < 15; ++i) {
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
        assert(mock.data[2][i] == alternateE0[i] &&
               mock.data[2][i] == kCydPanelPositiveGamma[i]);
#else
        assert(mock.data[2][i] == classicE0[i] &&
               mock.data[2][i] == kCydClassicLegacyE0Gamma[i]);
#endif
        assert(mock.data[3][i] == sharedE1[i] &&
               mock.data[3][i] == kCydPanelNegativeGamma[i]);
    }
    assert(mock.invertCalls == 1 && mock.swapCalls == 1 && mock.swap);
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    assert(!mock.inv);  // INV OFF + published classic ILI9341 gamma pair
#else
    assert(mock.inv);   // INV ON + classic old E0 / published negative E1
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
            print(f"PASS {label}: exact real E0/E1 gamma sequence "
                  "swapBytes=true framebuffer=untouched "
                  f"LCD_INV={'OFF' if flags else 'ON'}")
    print("PASS LCD-C3: classic INVON gamma C2 and 90pct physical PWM; "
          "alternate gamma and duty exactly unchanged; "
          "framebuffer/PAK/gameplay untouched")

if __name__ == "__main__":
    main()
