#!/usr/bin/env python3
"""Compile and run the production BOTTOM HUD HELP mini-painter, not a mock
of the rendering math. Only the PAK-backed HUD source, wall clock and the
single cached miniature are replaced by bounded deterministic host fakes.
No ESP32 framebuffer, SDL, ZIP or audio owner is needed on the host.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ACTIONS = (ROOT / "ESP32/src/native_main_menu_actions.c").read_text()
HUD = (ROOT / "ESP32/src/esp_native_gameplay_hud.c").read_text()

def extract(signature):
    start = ACTIONS.index(signature)
    opened = ACTIONS.index("{", start)
    depth = 0
    for pos in range(opened, len(ACTIONS)):
        if ACTIONS[pos] == "{":
            depth += 1
        elif ACTIONS[pos] == "}":
            depth -= 1
            if depth == 0:
                return ACTIONS[start:pos + 1]
    raise AssertionError("unclosed native painter: " + signature)

tint = extract("static uint16_t helpTintBottomHudWeaponBlue(")
paint = extract("static int helpPaintBottomHudIllustration(")

# Native HUD corners must remain in gameplay, not in the HELP-only capture.
assert "if (status == ESP_NATIVE_GAMEPLAY_HUD_OK && !bottomOnly) {" in HUD
assert "drawWeaponTouchCorners(framebuffer, stats);" in HUD
assert "paintPrepared(&prepared, framebuffer, NULL, 1)" in HUD
assert "paintPrepared(&prepared, framebuffer, outStats, 0)" in HUD
assert "const uint32_t keep = 256U - mix;" in tint
assert "31U * mix" in tint
assert "const uint32_t mix = ((uint32_t)power * 160U) / 255U;" in tint
assert "helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x]" in paint
assert "helpTintBottomHudWeaponBlue(*pixel, power)" in paint
assert "amber" not in paint and "corner" not in paint.lower().split(
    "whole button", 1)[-1].split("if (power != 0U)", 1)[-1]

preamble = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define MAIN_HELP_HUD_BAR_X 16
#define MAIN_HELP_HUD_BAR_Y 43
#define MAIN_HELP_HUD_BAR_WIDTH 128
#define MAIN_HELP_HUD_BAR_HEIGHT 16
#define ESP_NATIVE_HUD_WEAPON_LEFT 100
#define ESP_NATIVE_HUD_WEAPON_RIGHT 159

static uint16_t helpBottomHudMini[MAIN_HELP_HUD_BAR_WIDTH *
                                  MAIN_HELP_HUD_BAR_HEIGHT];
static uint8_t helpBottomHudReady;
static int helpHudBreathStep = -1;
static uint32_t mockClock;
static int paintCalls;

static uint32_t DoomRPG_GetUpTimeMS(void) { return mockClock; }
static int helpBreathStepAt(uint32_t now) { return (int)now; }
static uint8_t helpBreathLevel(int step) { return (uint8_t)step; }
static uint8_t EspNativeHelpScene_breathPower(uint8_t level) { return level; }

/* The real preview only paints the native bottom HUD once and then
 * reduces it to 128x16. This deterministic fake represents a dim gray
 * PAK HUD with visible contrast: no amber touch notches. */
static int EspNativeGameplayHud_paintHelpBottomBand(uint16_t* fb) {
    int x, y;
    ++paintCalls;
    for (y = 100; y < 120; ++y) {
        for (x = 0; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
            const unsigned int shade = 7U + ((unsigned int)(x * 3 + y) & 15U);
            fb[y * DOOMRPG_LOGICAL_WIDTH + x] =
                (uint16_t)(((shade >> 1) << 11) | (shade << 5) |
                           (shade >> 1));
        }
    }
    return 1;
}
"""

test = r"""
static uint16_t frame[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static uint16_t reference[MAIN_HELP_HUD_BAR_WIDTH * MAIN_HELP_HUD_BAR_HEIGHT];
static uint16_t peak[MAIN_HELP_HUD_BAR_WIDTH * MAIN_HELP_HUD_BAR_HEIGHT];
static uint16_t middle[MAIN_HELP_HUD_BAR_WIDTH * MAIN_HELP_HUD_BAR_HEIGHT];
static uint16_t readPixel(int x, int y) {
    return frame[y * DOOMRPG_LOGICAL_WIDTH + x];
}
static void paintPower(uint8_t p) {
    memset(frame, 0x55, sizeof(frame));
    mockClock = (uint32_t)p;
    assert(helpPaintBottomHudIllustration(frame));
    assert(helpHudBreathStep == (int)p);
}
static int red(uint16_t p) { return (int)(p >> 11); }
static int blue(uint16_t p) { return (int)(p & 31U); }

int main(void) {
    int x, y, painted = 0, unchanged = 0;
    paintPower(0U);
    assert(paintCalls == 1 && helpBottomHudReady);
    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {
        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {
            const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;
            const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;
            const unsigned int shade = 7U +
                ((unsigned int)(sx * 3 + sy) & 15U);
            const uint16_t source = (uint16_t)(
                ((shade >> 1) << 11) | (shade << 5) | (shade >> 1));
            reference[y * 128 + x] =
                readPixel(MAIN_HELP_HUD_BAR_X + x, MAIN_HELP_HUD_BAR_Y + y);
            assert(reference[y * 128 + x] == source);
        }
    }

    paintPower(128U);
    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y)
        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x)
            middle[y * 128 + x] = readPixel(16 + x, 43 + y);

    paintPower(255U);
    assert(paintCalls == 1);
    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {
        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {
            const uint16_t original = reference[y * 128 + x];
            const uint16_t current = readPixel(16 + x, 43 + y);
            const uint16_t half = middle[y * 128 + x];
            const int ammo = x >= 80 && x <= 127;
            peak[y * 128 + x] = current;
            if (ammo) {
                assert(current == helpTintBottomHudWeaponBlue(original, 255U));
                assert(half == helpTintBottomHudWeaponBlue(original, 128U));
                assert(red(current) < red(original));
                assert(blue(current) > blue(original));
                assert(red(current) <= red(half));
                assert(blue(current) >= blue(half));
                ++painted;
            } else {
                assert(current == original && half == original);
                ++unchanged;
            }
        }
    }
    assert(painted == 48 * 16 && unchanged == 80 * 16);
    /* Repaints must never accumulate blue or re-read the HUD backing. */
    paintPower(255U);
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 128; ++x)
            assert(peak[y * 128 + x] == readPixel(16 + x, 43 + y));
    paintPower(0U);
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 128; ++x)
            assert(reference[y * 128 + x] == readPixel(16 + x, 43 + y));
    assert(paintCalls == 1);
    puts("PASS real native HELP bottom painter: full 48x16 gray-blue breath, preserved neighbor HUD, no amber overlay, no accumulation");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="doom-help-hud-blue-") as td:
    src = Path(td) / "native_help_bottom_blue.c"
    exe = Path(td) / "native_help_bottom_blue"
    src.write_text(preamble + "\n" + tint + "\n" + paint + "\n" + test,
                   encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
