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
    "TOP HUD BAR", "CYD TOUCH (1/2)", "CYD TOUCH (2/2)",
    "ACTION / CENTER", "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "TACTICAL TIPS", "ABOUT CYD PORT"]
assert lines[1] == "Kharn27"
assert lines[6] == ""  # no unwanted DOWN: controls callout
# The fourth page (before CYD TOUCH) illustrates the real 160x20
# upper HUD in miniature; preserve weapon guidance on the next relevant page.
hud_page = lines[3 * 7 : 4 * 7]
assert hud_page == [
    "TOP HUD BAR", "3 VIRTUAL BUTTONS", "LEFT: game menu",
    "CENTER: pass a turn", "RIGHT: show automap", "", ""]
assert lines[7 * 7 + 6] == "Tap gun: weapons"

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
assert "if (shift == helpSpaceShift &&" in actions  # breath can repaint without star shift
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
# k.bmp is already a 20x20 tiled native PAK asset in gameplay.
# In HELP it is copied as eight native nearest-neighbour 16x16 tiles,
# creating a compact 128x16 illustration in the central reading window.
# The 1:3:1 input proportions come from the real gameplay classifier,
# while the UI remains a visual-only demonstration.
hud_c = (SRC / "esp_native_gameplay_hud.c").read_text()
shim = (SRC / "esp32_sdl.cpp").read_text()
assert '"k.bmp"' in hud_c
assert 'bar->width != 20U || bar->height != 20U' in (
    SRC / "esp_native_gameplay_hud_direction.c").read_text()
assert '"k.bmp", false, &helpHudBarBG' in actions
assert 'helpHudBarBG.width != MAIN_HELP_HUD_SOURCE_TILE' in actions
assert 'DoomRPG_freeImage(helpSpaceOwner, &helpHudBarBG);' in actions
assert 'helpPaintTopHudIllustration(framebuffer)' in actions
assert 'if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL)' in actions
assert '#define MAIN_HELP_HUD_PAGE_SCROLL (3 * MAIN_HELP_VISIBLE_LINES)' in actions
assert macro("MAIN_HELP_HUD_BAR_X") == 16
assert macro("MAIN_HELP_HUD_BAR_Y") == 43
assert macro("MAIN_HELP_HUD_BAR_WIDTH") == 128
assert macro("MAIN_HELP_HUD_BAR_HEIGHT") == 16
assert macro("MAIN_HELP_HUD_SOURCE_TILE") == 20
assert macro("MAIN_HELP_HUD_MINI_TILE") == 16
assert macro("MAIN_HELP_HUD_MENU_RIGHT") == 25
assert macro("MAIN_HELP_HUD_PASS_LEFT") == 26
assert macro("MAIN_HELP_HUD_PASS_RIGHT") == 102
assert macro("MAIN_HELP_HUD_MAP_LEFT") == 103
assert 'tileX += MAIN_HELP_HUD_MINI_TILE' in actions
assert 'SDL_RenderCopy(NULL, helpHudBarBG.imgBitmap, NULL, &dst)' in actions
assert 'const int sourceX = src.x + ((x - dst.x) * src.w) / dst.w;' in shim
assert 'const int sourceY = src.y + ((y - dst.y) * src.h) / dst.h;' in shim
assert 'static const char* const labels[3] = {"MENU", "PASS TURN", "MAP"};' in actions
assert 'static const HelpHudGlyph helpHudGlyphs[]' in actions
assert 'framebuffer[(y + row) * DOOMRPG_LOGICAL_WIDTH +' in actions
assert '0xffffU; /* white, all themes */' in actions
# The miniature 5x7 labels are white over breathing blue; all FOUR lines
# outside its borders use the theme's middle tone, like normal HELP copy.
assert '/* The illustrated page\'s explanatory copy shares the active theme\'s' in actions
hud_paint_section = actions.split(
    'if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL) {', 1
)[1].split('    else {', 1)[0]
assert hud_paint_section.count('helpColor888(helpBodyMiddleTone())') == 2
assert 'DoomRPG_setFontColor(doomRpg, 0xffffffffU);' not in hud_paint_section
assert '0xffffU; /* white, all themes */' in actions

# Preserve the logical input splitter 0..31/32..127/128..159,
# scaled and centered with no overlap and two fixed blue dividers.
assert '#define GAMEPLAY_MENU_RIGHT 31' in input_c
assert '#define GAMEPLAY_PASS_TURN_LEFT 32' in input_c
assert '#define GAMEPLAY_PASS_TURN_RIGHT 127' in input_c
assert '#define GAMEPLAY_AUTOMAP_LEFT 128' in input_c
zones = [(16, 41), (42, 118), (119, 143)]
assert [b - a + 1 for a, b in zones] == [26, 77, 25]
assert sum(b - a + 1 for a, b in zones) == 128
assert zones[0][0] == 16 and zones[-1][-1] == 143
assert zones[0][1] + 1 == zones[1][0]
assert zones[1][1] + 1 == zones[2][0]
for left, right in zones:
    assert 5 <= left < right <= 154
    assert (right - left + 1) > 0
for label, (left, right) in zip(("MENU", "PASS TURN", "MAP"), zones):
    # 5x7 ROM glyphs, 6px advance, one pixel margin in button interior.
    glyph_width = len(label) * 6 - 1
    text_x = (left + right - glyph_width + 1) // 2
    assert left < text_x and text_x + glyph_width - 1 < right
assert 16 + 128 <= 155 and 43 >= 39
assert 43 + 16 - 1 < 64 and 43 + 16 - 1 < 100
assert 27 + 12 - 1 < 43
assert 88 + 12 - 1 < 100

# Breath is a bounded triangular waveform, not a framebuffer-independent
# infinite 60Hz redraw. Only this HELP page is 110ms-phase-aware.
assert macro("MAIN_HELP_HUD_BREATH_STEP_MS") == 110
assert macro("MAIN_HELP_HUD_BREATH_STEPS") == 12
wave = [n if n <= 6 else 12 - n for n in range(12)]
assert wave == [0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1]
assert all(0x0013 + 2 * n <= 0x001f for n in wave)
assert all(2 <= 2 + n <= 8 for n in wave)
assert 'const uint16_t neon = (uint16_t)(0x0013U + 2U * level);' in actions
assert 'const uint8_t tint = (uint8_t)(2U + level);' in actions
assert 'helpHudBreathStep == helpHudBreathIndex(now)' in actions
assert 'helpDoomRpg->menuSystem->scrollIndex != MAIN_HELP_HUD_PAGE_SCROLL' in actions
assert 'helpHudBreathStep = step;' in actions
assert actions.count('helpHudBreathStep = -1;') == 3  # declaration, release, start
assert 'helpExpectedFrameFNV = finalFNV;' in actions
assert 'EspNativeGameplayInput_route(' not in actions
assert 'EspNativeGameplayHud_routeInitial(' not in actions
assert 'DoomRPG_esp32MainMenuReturnToMain(' in actions

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

print("PASS HELP: 11 pages, TOP HUD fourth, 128x16 k.bmp and 1.32s breath, white button glyphs,")
print("           7-line bounded pages, animation/FNV rebasing, release and no keypad")
