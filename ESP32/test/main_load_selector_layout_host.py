#!/usr/bin/env python3
"""Compile and execute real standalone MENU_MAIN LOAD selector C++ with host stubs.

Test the exact painted rectangles and input dispatch after compacting the
heading, lifting all five slots and granting BACK a full 22-pixel chassis.
The in-game HUB save/load overlay keeps its independent baseline geometry.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "ESP32/src/esp_native_gameplay_save.cpp").read_text(encoding="utf-8")


def extract_function(marker: str) -> str:
    start = source.index(marker)
    first = source.index("{", start)
    depth = 0
    for pos in range(first, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("unbalanced C++ function: " + marker)


names = (
    "kMainLoadFirstSlotTop", "kMainLoadSlotPitch",
    "kMainLoadPreviousTop", "kMainLoadPreviousBottom",
    "kMainLoadNextTop", "kMainLoadNextBottom",
    "kMainLoadBackLeft", "kMainLoadBackRight",
    "kMainLoadBackTop", "kMainLoadBackBottom",
)
constants = {}
for name in names:
    matches = re.findall(r"^constexpr int " + name + r" = (\d+);$", source, re.M)
    assert len(matches) == 1, (name, matches)
    constants[name] = int(matches[0])

assert constants == {
    "kMainLoadFirstSlotTop": 24, "kMainLoadSlotPitch": 14,
    "kMainLoadPreviousTop": 26, "kMainLoadPreviousBottom": 54,
    "kMainLoadNextTop": 61, "kMainLoadNextBottom": 90,
    "kMainLoadBackLeft": 34, "kMainLoadBackRight": 111,
    "kMainLoadBackTop": 97, "kMainLoadBackBottom": 118,
}, constants

paint = extract_function("static void paintMainSlots(void) {")
begin = extract_function('extern "C" int EspNativeGameplaySave_mainSelectorBegin(void) {')
finish = extract_function('extern "C" void EspNativeGameplaySave_mainSelectorFinish(void) {')
tap = extract_function('extern "C" int EspNativeGameplaySave_mainSelectorTap(int x, int y) {')
for snippet in (
    "const int y = kMainLoadFirstSlotTop + row * kMainLoadSlotPitch;",
    "drawSlotNav(fb, kMainLoadPreviousTop, kMainLoadPreviousBottom,",
    "drawSlotNav(fb, kMainLoadNextTop, kMainLoadNextBottom,",
    'DOOMRPG_LOGICAL_HEIGHT, 28, 2, 131, 18,',
    '"LOAD GAME", 0, 1)',
    'fb, text, 137, 99, EspMainMenuSkin_uiAccent());',
):
    assert snippet in paint, snippet

# The HUB selector is deliberately NOT retimed or shifted.
hub_start = source.index("bool paintSaveOverlay(void) {")
hub_end = source.index("}  // namespace", hub_start)
hub = source[hub_start:hub_end]
assert "const int top = 47 + row * 14;" in hub
assert "drawSlotNav(fb, 49, 78, false, first != 1U);" in hub
assert "drawSlotNav(fb, 83, 112, true, first == 1U);" in hub
assert 'drawCenteredWord(fb, 60, 35, label,' in hub
assert "if (slotMode != 0U)" in hub

prelude = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
enum { ESP_MAIN_MENU_SKIN_IDLE = 0 };
constexpr uint16_t ESP_HUB_COLOR_BG = 0x0000;
constexpr uint16_t ESP_HUB_COLOR_PANEL = 0x2222;
constexpr uint16_t ESP_HUB_COLOR_PANEL_ALT = 0x3333;
constexpr uint16_t ESP_HUB_COLOR_STEEL_DARK = 0x4444;
constexpr uint16_t ESP_HUB_COLOR_GREEN = 0x5555;
constexpr uint16_t ESP_HUB_COLOR_IVORY = 0x6666;
static uint8_t slotMode = 0, slotFocus = 1, slotArmed = 0, mainSlotReady = 0;
static uint8_t selectedSaveSlot = 1;
static uint16_t frame[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static int slotY[5], slotCount = 0, navTop[2], navBottom[2], navCount = 0;
static int backTop = -1, backBottom = -1, titleTop = -1, titleBottom = -1;
static int labelY = -1, presentations = 0;

void* Esp32PlatformVideo_framebuffer() { return frame; }
size_t Esp32PlatformVideo_framebufferSizeBytes() { return sizeof(frame); }
int Esp32PlatformVideo_present() { ++presentations; return 1; }
uint16_t EspMainMenuSkin_uiAccent() { return 0xabcd; }
uint16_t EspMainMenuSkin_uiAccentDim() { return 0x1234; }
void fillRect(uint16_t*, int l, int t, int r, int b, uint16_t) {
    if (l == 0 && t == 0 && r == 159 && b == 119) {
        slotCount = 0;
        navCount = 0;
    }
    if (l == 7 && r == 113 && b == t + 12) {
        assert(slotCount < 5);
        slotY[slotCount++] = t;
    }
    assert(l >= 0 && r < 160 && t >= 0 && b < 120);
}
void drawRect(uint16_t*, int, int, int, int, uint16_t) {}
void drawCenteredWord(uint16_t*, int, int, const char*, uint16_t) {}
void drawSlotNav(uint16_t*, int t, int b, bool, bool) {
    assert(navCount < 2);
    navTop[navCount] = t;
    navBottom[navCount++] = b;
}
bool formatSlotCaption(uint8_t slot, bool, char* out, size_t capacity) {
    std::snprintf(out, capacity, "SLOT %u", unsigned(slot));
    return slot == 4 || slot == 7;
}
void selectSaveSlot(uint8_t slot) { selectedSaveSlot = slot; }
bool readableSaveExists(void) { return slotFocus == 4 || slotFocus == 7; }
int EspMainMenuSkin_paintPlate(uint16_t*, int, int, int,
                               int l, int t, int r, int b, int state) {
    assert(l == 34 && r == 111 && state == ESP_MAIN_MENU_SKIN_IDLE);
    backTop = t;
    backBottom = b;
    return 1;
}
int EspMainMenuSkin_paintText(uint16_t*, int, int, int,
                              int l, int t, int r, int b,
                              const char* text, int, int) {
    if (std::strcmp(text, "LOAD GAME") == 0) {
        assert(l == 28 && r == 131);
        titleTop = t;
        titleBottom = b;
    } else if (std::strcmp(text, "BACK") == 0) {
        assert(l == 34 && r == 111 && t == backTop && b == backBottom);
    } else {
        assert(false && "unexpected title");
    }
    return 1;
}
struct EspNativeGameplayHubTouchUi {
    static void drawMiniText(uint16_t*, const char* text, int x, int y,
                             uint16_t) {
        assert(x == 137 && (!std::strcmp(text, "1/2") ||
                            !std::strcmp(text, "2/2")));
        labelY = y;
    }
};
"""

cases = r"""
int main() {
    assert(EspNativeGameplaySave_mainSelectorBegin() == 1);
    assert(slotMode == 3 && slotFocus == 1 && slotArmed == 0);
    assert(selectedSaveSlot == 1);
    assert(titleTop == 2 && titleBottom == 18);
    assert(labelY == 99 && backTop == 97 && backBottom == 118);
    assert(backBottom - backTop + 1 == 22);
    assert(slotCount == 5 && navCount == 2);
    for (int row = 0; row < 5; ++row)
        assert(slotY[row] == 24 + row * 14);
    assert(slotY[4] + 12 == 92 && slotY[4] + 12 < backTop - 1);
    assert(navTop[0] == 26 && navBottom[0] == 54);
    assert(navTop[1] == 61 && navBottom[1] == 90);
    assert(navBottom[1] < labelY);

    // Empty slot and dead zone between final slot and BACK.
    assert(EspNativeGameplaySave_mainSelectorTap(60, 24) == 0);
    assert(slotFocus == 1 && slotArmed == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 94) == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 96) == 0);
    assert(slotFocus == 1);

    // Live save: first tap arms, second confirms, no page/border collision.
    assert(EspNativeGameplaySave_mainSelectorTap(60, 66) == 0);
    assert(slotFocus == 4 && slotArmed == 4 && mainSlotReady == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 66) == 1);
    assert(mainSlotReady == 1);

    // Reopen, PAGE DOWN -> 6..10, occupied slot 7 and PAGE UP.
    assert(EspNativeGameplaySave_mainSelectorBegin() == 1);
    assert(EspNativeGameplaySave_mainSelectorTap(137, 70) == 0);
    assert(slotFocus == 6 && selectedSaveSlot == 6);
    assert(EspNativeGameplaySave_mainSelectorTap(137, 70) == 0);
    assert(slotFocus == 6);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 38) == 0);
    assert(slotFocus == 7 && slotArmed == 7);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 38) == 1);
    assert(mainSlotReady == 1);
    assert(EspNativeGameplaySave_mainSelectorTap(137, 26) == 0);
    assert(slotFocus == 1);
    assert(EspNativeGameplaySave_mainSelectorTap(137, 60) == 0);
    assert(slotFocus == 1); // Gap between nav chevrons does nothing.

    // BACK is 22px high and only its exact x34..111/y97..118.
    assert(EspNativeGameplaySave_mainSelectorTap(60, 96) == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(120, 97) == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 97) == -1);
    assert(slotMode == 0 && mainSlotReady == 0);
    assert(EspNativeGameplaySave_mainSelectorTap(60, 97) == 0);
    assert(presentations >= 6);
    return 0;
}
"""

consts = "\n".join(f"constexpr int {name} = {value};"
                   for name, value in constants.items()) + "\n"
code = "\n".join([prelude, consts, paint, finish, begin, tap, cases])
with tempfile.TemporaryDirectory(prefix="doom-main-load-layout-") as temp:
    path = Path(temp)
    cpp = path / "selector.cpp"
    exe = path / "selector"
    cpp.write_text(code, encoding="utf-8")
    subprocess.run(["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print("[MAIN-LOAD-LAYOUT] PASS real C++ paint + taps: title compact, "
      "five slots raised, BACK=22px, pagination aligned, HUB untouched")
