#!/usr/bin/env python3
"""Compile the real HELP-only 4x3 PAK sprite illumination, sweeping neon
frame and HUD touch cue into a host frame with strict pixel/clock guards.

The game/HUB sprite painter stays untouched. All 12 frames must light in
sequence and close on four sides. The PAK miniature remains immutable, every
nonfocused slot is dark, and captions/sky/footer are pixel-exact unchanged.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/native_main_menu_actions.c").read_text()

def extract(signature: str) -> str:
    assert source.count(signature) == 1, signature
    begin = source.index(signature)
    brace = source.index("{", begin)
    level = 0
    for n in range(brace, len(source)):
        if source[n] == "{":
            level += 1
        elif source[n] == "}":
            level -= 1
            if not level:
                return source[begin:n + 1]
    raise AssertionError("unclosed production function " + signature)

functions = "\n\n".join(extract(s) for s in (
    "static uint16_t helpTintBottomHudWeaponBlue(",
    "static uint16_t helpWeaponPreviewLitPixel(",
    "static void helpIlluminateWeaponGridMini(void) {",
    "static uint8_t helpWeaponSweepSlot(",
    "static void helpDrawWeaponGridMini(",
    "static void helpDrawWeaponButtonCue(",
))
assert "demo.weapons = 0x0fffU;" in source
assert "helpIlluminateWeaponGridMini();" in source
assert "EspNativeGameplayHubWeaponGrid_paint(framebuffer, &demo, 2U)" in source
assert "scroll == MAIN_HELP_WEAPON_PAGE_SCROLL;" in source
assert "helpHudBreathStep = step;" in functions
assert "helpWeaponSweepEpoch = DoomRPG_GetUpTimeMS();" in source
assert "helpWeaponSweepSlot(" in source
assert "static uint16_t helpWeaponMini[" in source
assert "MAIN_HELP_WEAPON_SWEEP_MS 320U" in source
assert "const int dstX = 108, dstY = 12, dstW = 46, dstH = 15;" in functions

preamble = r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define MAIN_HELP_WEAPON_MINI_W 144
#define MAIN_HELP_WEAPON_MINI_H 60
#define MAIN_HELP_WEAPON_MINI_X 8
#define MAIN_HELP_WEAPON_MINI_Y 39
#define MAIN_HELP_WEAPON_SWEEP_MS 320U
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT 12U
#define MAIN_HELP_HUD_BAR_WIDTH 128
#define MAIN_HELP_HUD_BAR_HEIGHT 16
#define ESP_NATIVE_HUD_WEAPON_LEFT 100
#define ESP_NATIVE_HUD_WEAPON_RIGHT 159
static uint16_t helpWeaponMini[MAIN_HELP_WEAPON_MINI_W *
                               MAIN_HELP_WEAPON_MINI_H];
static uint8_t helpWeaponMiniReady = 1U;
static uint16_t helpBottomHudMini[MAIN_HELP_HUD_BAR_WIDTH *
                                  MAIN_HELP_HUD_BAR_HEIGHT];
static uint8_t helpBottomHudReady = 1U;
static int helpHudBreathStep = -1;
static uint32_t fakeNow;
static uint32_t DoomRPG_GetUpTimeMS(void) { return fakeNow; }
static int helpBreathStepAt(uint32_t now) { return (int)now; }
static uint8_t helpBreathLevel(int step) { return (uint8_t)step; }
static uint8_t EspNativeHelpScene_breathPower(uint8_t level) { return level; }
"""
main = r"""
static uint16_t frame[160 * 120];
static uint16_t before[144 * 60];
static uint16_t cached[128 * 16];

static void previewGridTest(void) {
    int row, col, x, y, count = 0;
    for (y = 0; y < 60; ++y) {
        for (x = 0; x < 144; ++x) {
            const int dx = x % 36;
            const int dy = y % 20;
            helpWeaponMini[y * 144 + x] =
                (dx == 0 || dx == 35 || dy == 0 || dy == 19)
                    ? 0x6b4dU : 0x0841U;
        }
    }
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 4; ++col) {
            const int base = (row * 20 + 9) * 144 + col * 36 + 17;
            /* Representative source-gray and natural-color PAK sprite
             * texels in EVERY weapon slot (including 3 dog familiars). */
            helpWeaponMini[base] = 0x8410U;
            helpWeaponMini[base + 1] = 0xf800U;
            helpWeaponMini[base + 2] = 0x0000U;
        }
    }
    memcpy(before, helpWeaponMini, sizeof(before));
    helpIlluminateWeaponGridMini();
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 4; ++col) {
            const int base = (row * 20 + 9) * 144 + col * 36 + 17;
            const uint16_t litGray = helpWeaponMini[base];
            const uint16_t litRed = helpWeaponMini[base + 1];
            assert(litGray != 0x8410U && litRed != 0xf800U);
            assert(((litGray >> 5) & 63U) > ((0x8410U >> 5) & 63U));
            assert((litGray & 31U) > (0x8410U & 31U));
            assert((litRed & 31U) > 0U);
            assert(helpWeaponMini[base + 2] == 0U);
            ++count;
        }
    }
    for (y = 0; y < 60; ++y) {
        for (x = 0; x < 144; ++x) {
            const uint16_t old = before[y * 144 + x];
            if (old == 0x0841U || old == 0x0000U ||
                x % 36 == 0 || x % 36 == 35 ||
                y % 20 == 0 || y % 20 == 19)
                assert(helpWeaponMini[y * 144 + x] == old);
        }
    }
    assert(count == 12);
}


static void sweepTest(void) {
    /* Source miniature was already illuminated by previewGridTest(). */
    static uint16_t immutable[144 * 60];
    const uint16_t sky = 0x2345U;
    uint32_t t;
    int slot, x, y;
    memcpy(immutable, helpWeaponMini, sizeof(immutable));
    for (slot = 0; slot < 12; ++slot) {
        /* 319ms hold, then exactly one next slot; slot 11 wraps to 0. */
        t = (uint32_t)slot * MAIN_HELP_WEAPON_SWEEP_MS;
        assert(helpWeaponSweepSlot(t) == (uint8_t)slot);
        assert(helpWeaponSweepSlot(t + MAIN_HELP_WEAPON_SWEEP_MS - 1U)
               == (uint8_t)slot);
        for (y = 0; y < 120; ++y)
            for (x = 0; x < 160; ++x)
                frame[y * 160 + x] = sky;
        helpDrawWeaponGridMini(frame, (uint8_t)slot);
        for (y = 0; y < 120; ++y) {
            for (x = 0; x < 160; ++x) {
                uint16_t expected = sky;
                if (x >= 8 && x <= 151 && y >= 39 && y <= 98) {
                    const int dx = x - 8;
                    const int dy = y - 39;
                    const int cx = dx % 36, cy = dy % 20;
                    const int cell = dy / 20 * 4 + dx / 36;
                    const int frameEdge = (cx <= 1 || cx >= 34 ||
                                           cy <= 1 || cy >= 18);
                    expected = frameEdge
                        ? (uint16_t)(cell == slot ? 0x07e0U : 0x4208U)
                        : immutable[dy * 144 + dx];
                }
                assert(frame[y * 160 + x] == expected);
            }
        }
        assert(memcmp(immutable, helpWeaponMini, sizeof(immutable)) == 0);
    }
    assert(helpWeaponSweepSlot(12U * MAIN_HELP_WEAPON_SWEEP_MS) == 0U);
    assert(helpWeaponSweepSlot(13U * MAIN_HELP_WEAPON_SWEEP_MS) == 1U);
    /* Invalid slot must not touch a pixel (fail closed). */
    for (y = 0; y < 120; ++y)
        for (x = 0; x < 160; ++x) frame[y * 160 + x] = sky;
    helpDrawWeaponGridMini(frame, 12U);
    helpWeaponMiniReady = 0U;
    helpDrawWeaponGridMini(frame, 0U);
    helpWeaponMiniReady = 1U;
    helpDrawWeaponGridMini(NULL, 0U);
    for (y = 0; y < 120; ++y)
        for (x = 0; x < 160; ++x) assert(frame[y * 160 + x] == sky);
    assert(memcmp(immutable, helpWeaponMini, sizeof(immutable)) == 0);
}

static void cueTest(void) {
    int x, y, phase, changed;
    const uint16_t untouched = 0x39e7U;
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 128; ++x) {
            const uint16_t p = (uint16_t)(
                (((x + y) & 15) << 11) |
                (((x * 3 + y) & 31) << 5) | ((x * 5 + y) & 15));
            helpBottomHudMini[y * 128 + x] = p;
        }
    }
    memcpy(cached, helpBottomHudMini, sizeof(cached));
    for (phase = 0; phase < 3; ++phase) {
        const uint8_t power = phase == 0 ? 0U : phase == 1 ? 128U : 255U;
        memset(frame, 0, sizeof(frame));
        for (y = 0; y < 120; ++y)
            for (x = 0; x < 160; ++x)
                frame[y * 160 + x] = untouched;
        fakeNow = power;
        helpDrawWeaponButtonCue(frame);
        assert(helpHudBreathStep == (int)power);
        changed = 0;
        for (y = 0; y < 120; ++y) {
            for (x = 0; x < 160; ++x) {
                if (x >= 108 && x < 154 && y >= 12 && y < 27) {
                    const int sx = 80 + (x - 108) * 48 / 46;
                    const int sy = (y - 12) * 16 / 15;
                    const uint16_t original = cached[sy * 128 + sx];
                    assert(frame[y * 160 + x] ==
                           helpTintBottomHudWeaponBlue(original, power));
                    ++changed;
                } else {
                    assert(frame[y * 160 + x] == untouched);
                }
            }
        }
        assert(changed == 46 * 15);
        assert(memcmp(cached, helpBottomHudMini, sizeof(cached)) == 0);
    }
    /* Both endpoints and the middle derive from the SAME gray source. */
    assert(helpTintBottomHudWeaponBlue(0x8410U, 0U) == 0x8410U);
    assert(helpTintBottomHudWeaponBlue(0x8410U, 255U) != 0x8410U);
    assert(helpBottomHudReady == 1U);
}

int main(void) {
    previewGridTest();
    sweepTest();
    cueTest();
    puts("PASS HELP 12/12 PAK weapon grid, 320ms wrap sweep, full green borders, 46x15 HUD cue, no overdraw/cache mutation");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="doom-help-weapons-") as tmp:
    code = Path(tmp) / "native_help_weapon_preview.c"
    binary = Path(tmp) / "native_help_weapon_preview"
    code.write_text(preamble + "\n" + functions + "\n" + main,
                    encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(code), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
