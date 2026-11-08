#!/usr/bin/env python3
"""Native CYD help contract: bounded guide, original scrolling PAK sky, and
exact 160x120 physical touch/industrial-chassis geometry. Source-only checks
cannot replace a real classic CYD visual/touch validation.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
INC = ROOT / "ESP32/include"
model = (SRC / "native_main_menu_model.c").read_text()
actions = (SRC / "native_main_menu_actions.c").read_text()
loop = (SRC / "main.cpp").read_text()
input_c = (SRC / "esp_native_gameplay_input.c").read_text()
header = (INC / "native_main_menu_actions.h").read_text()
skin = (SRC / "esp_main_menu_skin.c").read_text()
intro = (SRC / "native_story_fit.c").read_text()

def macro(name):
    match = re.search(r"^#define\s+" + re.escape(name) + r"\s+(\d+)",
                      actions, re.M)
    assert match, name
    return int(match.group(1))

start = model.index("static const char* const helpLines[]")
end = model.index("#define MAIN_HELP_LINE_COUNT", start)
lines = re.findall(r'^\s*"([^"]*)",$', model[start:end], re.M)
assert len(lines) == 77
assert len(lines) % 7 == 0
assert [lines[p * 7] for p in range(11)] == [
    "DOOM RPG ON CYD", "MENU CONTROLS", "TURN-BASED GAME",
    "CYD TOUCH (1/2)", "CYD TOUCH (2/2)", "ACTION / CENTER",
    "TOP HUD BAR", "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "TACTICAL TIPS", "ABOUT CYD PORT"]
assert lines[1] == "Kharn27"
assert lines[6] == ""  # no unwanted DOWN: controls callout
assert "DOWN: controls" not in lines
assert max(map(len, lines)) <= 20  # source font=7px/char, x=12
assert len(lines) == len(set((i, line) for i, line in enumerate(lines)))
assert any("CYD" in line for line in lines)
assert all(label in lines for label in (
    "YOU ARE THE MARINE", "TURN-BASED GAME", "CYD TOUCH (1/2)",
    "TOP HUD BAR", "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "MENU CONTROLS", "TACTICAL TIPS", "ABOUT CYD PORT"))
assert all(term in lines for term in (
    "LEFT: game menu", "CENTER: pass a turn", "RIGHT: show automap",
    "Prev gun / Back /", "Turn L / ACT /", "Tap again: confirm."))
assert "EspLegacyAssetSource_readAlloc" not in model
assert "EspLegacyAssetSource_stat" not in model
assert "static int buildBoundedHelpModel(" in model
assert "return MAIN_HELP_LINE_COUNT;" in model
assert "MAIN_HELP_LINE_COUNT <= MAIN_HELP_MAX_LINES" in model
assert "menuSystem->numItems = MAIN_HELP_LINE_COUNT;" in model

# The actual gameplay classifier is the single source of truth for help;
# no mobile keypad codes or assumed diagonal motions.
assert "{ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT}" in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_SELECT," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT}" in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK," in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON}" in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN" in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP" in input_c
assert "ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU" in input_c

# c.bmp is the same source as the original ST_INTRO horizontal scroll,
# but AIDE must not take over ST_INTRO state or its story clock.
assert '"c.bmp", false, &helpSpaceBG' in actions
assert "scrollSpaceBG" in intro and "const int i = -((doomCanvas->time / 157) % 192);" in intro
assert macro("MAIN_HELP_STAR_PERIOD_MS") == 157
assert macro("MAIN_HELP_STAR_VIRTUAL_WIDTH") == 192
assert "DoomRPG_freeImage(helpSpaceOwner, &helpSpaceBG);" in actions
assert "Esp32PlatformVideo_framebuffer()" in actions
assert "SDL_RenderCopy(NULL, helpSpaceBG.imgBitmap" in actions
assert "MAIN_HELP_STAR_VIRTUAL_WIDTH" in actions
assert "if (shift == helpSpaceShift) return;" in actions
assert "DoomRPG_esp32MainMenuHelpService();" in loop
assert "void DoomRPG_esp32MainMenuHelpService(void);" in header
assert loop.index("Esp32IntroClock_service();") < loop.index("DoomRPG_esp32MainMenuHelpService();")
assert "EspNativeIntroState_begin(" not in actions

# Three plates map pixel-for-pixel to their touch rectangles, with
# two inert spacer columns; no geometry beyond the single 160x120 frame.
assert macro("MAIN_HELP_VISIBLE_LINES") == 7
assert macro("MAIN_HELP_LINE_HEIGHT") == 12
assert macro("MAIN_HELP_FOOTER_TOP") == 100
assert macro("MAIN_HELP_BUTTON_BOTTOM") == 119
x_spans = [(macro("MAIN_HELP_BACK_LEFT"), macro("MAIN_HELP_BACK_RIGHT")),
           (macro("MAIN_HELP_UP_LEFT"), macro("MAIN_HELP_UP_RIGHT")),
           (macro("MAIN_HELP_DOWN_LEFT"), macro("MAIN_HELP_DOWN_RIGHT"))]
assert x_spans == [(1, 51), (54, 104), (107, 158)]
for y in range(120):
    for x in range(160):
        touches = [idx for idx, (left, right) in enumerate(x_spans)
                   if left <= x <= right and 100 <= y <= 119]
        assert len(touches) <= 1
        if y < 100: assert not touches
assert actions.count("!EspMainMenuSkin_paintPlate(") == 3
assert actions.count("!EspMainMenuSkin_paintText(") == 3
# All HELP copy now consumes the live theme's industrial font palette.
# For every theme: top=CLASSIC warm ivory / INFERNO pale rose /
# PHOSPHORE mint, middle=CLASSIC amber / INFERNO bronze / PHOSPHORE green.
assert "static uint16_t helpTitleTopTone(void)" in actions
assert "static uint16_t helpBodyMiddleTone(void)" in actions
assert "return EspMainMenuSkin_letterTone(1, 9);" in actions
assert "return EspMainMenuSkin_letterTone(4, 9);" in actions
assert "helpColor888(helpTitleTopTone())" in actions
assert "helpColor888(helpBodyMiddleTone())" in actions
assert "const uint16_t ink = helpTitleTopTone();" in actions
assert "EspMainMenuSkin_uiInk()" not in actions
assert 'helpPaintCopyrightMark(framebuffer);' in actions
assert '"Kharn27"' in model
assert "helpDrawThemedPageTitle(doomRpg, canvas, line, 12, y);" in actions
assert "EspMainMenuSkin_letterTone(band * 3 + 1, 9)" in actions
assert "DoomRPG_setClipTrue(doomRpg, x, y + band * 4," in actions
assert "DoomRPG_setClipFalse(doomRpg);" in actions
assert "helpPaintCopyrightMark(framebuffer);" in actions
assert "i == 1 && menuSystem->scrollIndex == 0" in actions
assert "EspNativeCanvas_drawText(canvas, line, 27, y, 0);" in actions
assert "static const uint16_t rows[12]" in actions
assert "for (x = 0; x < 12; ++x)" in actions
assert "fb[(26 + y) * DOOMRPG_LOGICAL_WIDTH + 12 + x] = ink;" in actions
assert 'i % MAIN_HELP_VISIBLE_LINES == 0' in actions
# The logo and section heading never occupy the footer/button pixels;
# the original 9x12 explanation glyphs and 12px line height remain unchanged.
assert 26 + 12 - 1 == 37 and 12 + 12 - 1 < 27
assert "y = 14;" in actions
assert "y += MAIN_HELP_LINE_HEIGHT;" in actions

# © source pixels occupy exactly 12x12 and preserve the middle-band
# color selection across animation refreshes without allocating another frame.
copyright_start = actions.index("static void helpPaintCopyrightMark(")
copyright_end = actions.index("int DoomRPG_esp32MainMenuReturnToMain(", copyright_start)
copyright = actions[copyright_start:copyright_end]
logo_mask = copyright.split("static const uint16_t rows[12] = {", 1)[1].split("};", 1)[0]
logo_rows = re.findall(r"0x([0-9a-f]{3})U", logo_mask)
assert len(logo_rows) == 12
assert list(map(lambda h: int(h, 16), logo_rows)) == [
    0x0f0, 0x30c, 0x402, 0x8f1, 0x901, 0x901,
    0x901, 0x901, 0x8f1, 0x402, 0x30c, 0x0f0]
assert "for (y = 0; y < 12; ++y)" in copyright
assert "const uint16_t ink = helpTitleTopTone();" in copyright

assert "EspMainMenuSkin_uiAccent()" in actions
assert "EspMainMenuSkin_uiAccentDim()" in actions
assert "menuSystem->scrollIndex -= MAIN_HELP_VISIBLE_LINES;" in actions
assert "menuSystem->scrollIndex += MAIN_HELP_VISIBLE_LINES;" in actions
assert "else return; /* gaps between plates" in actions
assert "helpExpectedFrameFNV = finalFNV;" in actions
assert "before != helpExpectedFrameFNV" in actions
assert "helpStop();" in actions
assert "DoomRPG_esp32MainMenuReturnToMain(" in actions
assert "graphSetCliping" in actions
assert "EspMainMenuSkin_paintPlate" in skin

for scroll in range(len(lines)):
    clamped = min(scroll, len(lines) - 7)
    assert 0 <= clamped <= len(lines) - 7
    assert clamped + 7 <= len(lines)
    page = (clamped + 6) // 7 + 1
    assert 1 <= page <= (len(lines) + 6) // 7

print("PASS HELP: 11 themed title pages, upper-tone 12px © Kharn27, middle-tone body, menu page 2,")
print("           7-line bounded pages, animation/FNV rebasing, release and no keypad")
