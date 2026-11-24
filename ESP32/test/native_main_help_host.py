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
assert len(lines) == 119
assert len(lines) % 7 == 0
assert [lines[p * 7] for p in range(17)] == [
    "DOOM RPG ON CYD", "DOOM RPG: THE GAME", "MENU CONTROLS",
    "TURN-BASED GAME", "MAIN GAMEPLAY", "ACTION GAMEPLAY",
    "TOP HUD BAR", "TOP ROW: MOVE", "MIDDLE ROW: ACTION",
    "BOTTOM ROW: WEAPONS", "BOTTOM HUD BAR", "WEAPON SELECT",
    "ACTION / CENTER", "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "TACTICAL TIPS", "ABOUT CYD PORT"]
assert lines[1:6] == [
    "Kharn27", "Erick V. Garcia", "id + Fountainhead",
    "John Carmack:", "Endless admiration!"]
assert lines[7:14] == [
    "DOOM RPG: THE GAME", "On Mars, UAC labs", "are overrun by Hell.",
    "As a lone marine,", "explore dark halls,", "fight turn by turn,",
    "Find keys, secrets."]
assert lines[6] == ""  # no unwanted DOWN: controls callout
# Page 4/16 swaps explanatory text rows 3 and 4 only.
assert lines[3 * 7 : 4 * 7] == [
    "TURN-BASED GAME", "", "Moves take a turn.", "Enemies take turns.",
    "Actions use turns.", "Plan each attack.", "Think before moving."]
# Page 5 is the passive gameplay screenshot, page 6 the existing actions.
# The original TOP HUD tutorial follows them as page 7.
assert lines[4 * 7 : 5 * 7] == ["MAIN GAMEPLAY", "", "", "", "", "", ""]
assert lines[5 * 7 : 6 * 7] == ["ACTION GAMEPLAY", "", "", "", "", "", ""]
hud_page = lines[6 * 7 : 7 * 7]
assert hud_page == [
    "TOP HUD BAR", "3 VIRTUAL BUTTONS", "LEFT: game menu",
    "CENTER: pass a turn", "RIGHT: show automap", "", ""]
assert lines[13 * 7 + 6] == "Tap gun: weapons"
assert [lines[i * 7 : i * 7 + 5] for i in range(7, 10)] == [
    ["TOP ROW: MOVE", "3 TOP TOUCH ZONES", "LEFT: STRAFE LEFT",
     "CENTER: MOVE FORWARD", "RIGHT: STRAFE RIGHT"],
    ["MIDDLE ROW: ACTION", "3 MIDDLE TOUCH ZONES", "LEFT: TURN LEFT",
     "CENTER: ATTACK / USE", "RIGHT: TURN RIGHT"],
    ["BOTTOM ROW: WEAPONS", "3 BOTTOM TOUCH ZONES", "LEFT: PREV WEAPON",
     "CENTER: MOVE BACK", "RIGHT: NEXT WEAPON"]]
assert all(lines[i * 7 + 5:i * 7 + 7] == ["", ""] for i in range(7, 10))
assert "#define MAIN_HELP_MAX_LINES 119" in model
assert lines[10 * 7 : 11 * 7] == [
    "BOTTOM HUD BAR", "LEFT: HP + ARMOR", "CENTER: FACE / AMMO",
    "RIGHT: DIRECTION / AMMO", "Touch for weapon menu", "", ""]
assert "STATUS AT A GLANCE" not in lines
assert lines[10 * 7 + 3].startswith("RIGHT: ")
assert lines[10 * 7 + 4] == "Touch for weapon menu"
assert lines[11 * 7 : 12 * 7] == [
    "WEAPON SELECT", "Tap gun icon", "", "", "", "", ""]

assert "DOWN: controls" not in lines
# Two long strings on BOTTOM HUD are explicitly positioned across the 160px
# width. Every other guide entry remains within 20x7px / x12.
assert max(len(line) for i, line in enumerate(lines)
           if i not in (10 * 7 + 3, 10 * 7 + 4)) <= 20
assert len(lines[10 * 7 + 3]) == 23
assert len(lines[10 * 7 + 4]) == 21
assert 2 + (len("RIGHT:") - 1) * 7 + 9 == 46
assert 46 + (len("DIRECTION / AMMO") - 1) * 7 + 9 == 160
assert 8 + (len("Touch for weapon menu") - 1) * 7 + 9 == 157
assert len(lines) == len(set((i, line) for i, line in enumerate(lines)))
assert any("CYD" in line for line in lines)
assert all(label in lines for label in (
    "DOOM RPG: THE GAME", "TURN-BASED GAME", "MAIN GAMEPLAY",
    "TOP HUD BAR", "BOTTOM HUD BAR", "WEAPON SELECT",
    "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "MENU CONTROLS", "TACTICAL TIPS", "ABOUT CYD PORT"))
assert all(term in lines for term in (
    "LEFT: game menu", "CENTER: pass a turn", "RIGHT: show automap",
    "LEFT: PREV WEAPON", "CENTER: ATTACK / USE", "Tap again: confirm."))
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
assert "EspNativeTexture_blit(helpSpaceBG.imgBitmap" in actions
assert "MAIN_HELP_STAR_VIRTUAL_WIDTH" in actions
assert "if (shift == helpSpaceShift &&" in actions  # breath can repaint without star shift
assert "DoomRPG_esp32MainMenuHelpService();" in loop
assert "void DoomRPG_esp32MainMenuHelpService(void);" in header
assert loop.index("Esp32IntroClock_service();") < loop.index("DoomRPG_esp32MainMenuHelpService();")
assert "EspNativeIntroState_begin(" not in actions

# Fullscreen navigation: no bottom plates, one X in the header and two
# 80-pixel-wide touch halves. The X wins over previous on its own rectangle.
assert macro("MAIN_HELP_VISIBLE_LINES") == 7
assert macro("MAIN_HELP_LINE_HEIGHT") == 14
assert "MAIN_HELP_FOOTER_TOP" not in actions
assert "MAIN_HELP_BACK_LEFT" not in actions
assert "MAIN_HELP_BUTTON_BOTTOM" not in actions
assert "EspMainMenuSkin_paintPlate(framebuffer" not in actions
assert "EspMainMenuSkin_paintText(framebuffer" not in actions
assert '#include "esp_native_help_nav.h"' in actions
assert "EspNativeHelpNav_hit(logicalX, logicalY)" in actions
assert "EspNativeHelpNav_scroll(" in actions
assert "if (action == ESP_NATIVE_HELP_NAV_CLOSE)" in actions
assert "DoomRPG_esp32MainMenuReturnToMain(" in actions
assert "if (menuSystem->scrollIndex == beforeScroll) return;" in actions
assert 'input=header-x+left-prev+right-next' in actions
assert 'helpPaintPageChevron(framebuffer, 1, menuSystem->scrollIndex > 0)' in actions
assert 'helpPaintPageChevron(framebuffer, 0, menuSystem->scrollIndex < maxScroll)' in actions
assert '"HELP / CYD", 22, 0, 0' in actions
assert '"X", 4, 1, 0' in actions
assert 'DoomRPG_setFontColor(doomRpg, 0xffff2424U)' in actions
assert 'helpPaintExitFrame' not in actions  # red X has no border
assert 'static void helpPaintPageChevron(' in actions
assert 'static void helpPaintPageChevron(' in actions

# All HELP copy consumes the live industrial font theme.
# The HEADER alone uses the exact center (second) stop; page titles and
# body text keep their existing separate three-stop behavior.
assert "static uint16_t helpTitleTopTone(void)" in actions
assert "static uint16_t helpBodyMiddleTone(void)" in actions
assert "return EspMainMenuSkin_letterTone(1, 9);" in actions
assert "return EspMainMenuSkin_letterTone(4, 9);" in actions
assert "helpColor888(helpTitleTopTone())" not in actions.split('if (!helpDrawStarfield(doomRpg)) return 0;',1)[1].split('if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL)',1)[0]
help_header = actions.split('if (!helpDrawStarfield(doomRpg)) return 0;',1)[1].split('if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL)',1)[0]
assert 'helpColor888(helpBodyMiddleTone())' in help_header
assert help_header.index('helpColor888(helpBodyMiddleTone())') < help_header.index('"HELP / CYD"')
assert help_header.index('"HELP / CYD"') < help_header.index('EspNativeCanvas_drawText(canvas, page,')
assert 'snprintf(page, sizeof(page), "%d/%d"' in help_header
# No rectangular frame, no dark/translucent overlay. The real PAK
# c.bmp starfield goes straight to the ONE shared framebuffer on each paint.
starfield = actions.split('static int helpDrawStarfield(',1)[1].split('static uint32_t helpColor888(',1)[0]
assert 'EspNativeTexture_blit(helpSpaceBG.imgBitmap, NULL, &primary)' in starfield
assert 'EspNativeTexture_blit(helpSpaceBG.imgBitmap, NULL, &wrap)' in starfield
assert 'helpSpaceShift = shift;' in starfield
assert 'for (y = 13;' not in starfield
assert '*pixel =' not in starfield
assert 'framebuffer[' not in starfield
assert 'dimAccent' not in starfield and 'uint16_t accent' not in starfield
assert '0x7befU' not in starfield
assert "helpColor888(bodyTone)" in actions
assert "static uint16_t helpThirdTone(void)" in actions
assert "EspMainMenuSkin_uiInk()" not in actions
assert 'helpPaintCopyrightMark(framebuffer, y, tone);' in actions
assert '"Kharn27"' in model
# HELP pages 2-4 have neutral white explanatory ink, as requested.
# Page titles and HELP/CYD stay themed; pages 12-16 retain the light
# theme-tone body and the page-1 tribute remains unchanged.
light_pages = (2, 3, 4, 13, 14, 15, 16, 17)
assert [lines[(page - 1) * 7] for page in light_pages] == [
    "DOOM RPG: THE GAME", "MENU CONTROLS", "TURN-BASED GAME",
    "ACTION / CENTER", "WEAPONS & STATUS", "SYSTEM / SAVE LOAD",
    "TACTICAL TIPS", "ABOUT CYD PORT",
]
assert [page for page in range(1, 18)
        if (2 <= page <= 4) or (13 <= page <= 17)] == list(light_pages)
assert all(lines[(page - 1) * 7] for page in light_pages)
body_branch = actions.split("else if (line[0] != '\\0') {", 1)[1].split(
    "y += MAIN_HELP_LINE_HEIGHT;", 1
)[0]
assert "const int scroll = menuSystem->scrollIndex;" in body_branch
assert "scroll >= MAIN_HELP_VISIBLE_LINES &&" in body_branch
assert "scroll <= 3 * MAIN_HELP_VISIBLE_LINES" in body_branch
assert "scroll >= 12 * MAIN_HELP_VISIBLE_LINES &&" in body_branch
assert "scroll <= 16 * MAIN_HELP_VISIBLE_LINES" in body_branch
assert "? helpTitleTopTone() : helpBodyMiddleTone();" in body_branch
assert "helpColor888(bodyTone)" in body_branch
assert "(menuSystem->scrollIndex == 0 && i == 5)" in body_branch
assert "((menuSystem->scrollIndex == 0 && i == 5) ||\n                 (scroll >= MAIN_HELP_VISIBLE_LINES &&\n                  scroll <= 3 * MAIN_HELP_VISIBLE_LINES))" in body_branch
# Exhaust the fixed 16-page help catalogue: only pages 2/3/4 get white
# body by this branch (plus the preexisting page-1 tribute line).
assert [page for page in range(1, 18) if 7 <= (page - 1) * 7 <= 21] == [2, 3, 4]
assert "? 0xffffffffU : helpColor888(bodyTone)" in body_branch
assert "helpDrawThemedPageTitle(doomRpg, canvas, line, 12, y);" in actions
assert "EspMainMenuSkin_letterTone(band * 3 + 1, 9)" in actions
assert "DoomRPG_setClipTrue(doomRpg, x, y + band * 4," in actions
assert "DoomRPG_setClipFalse(doomRpg);" in actions
assert "helpPaintCopyrightMark(framebuffer, y, tone);" in actions
assert "i >= 1 && i <= 3 && menuSystem->scrollIndex == 0" in actions
assert "EspNativeCanvas_drawText(canvas, line, 27, y, 0);" in actions
assert "static const uint16_t rows[12]" in actions
assert "for (x = 0; x < 12; ++x)" in actions
assert "fb[(top + y) * DOOMRPG_LOGICAL_WIDTH + 12 + x] = ink;" in actions
assert 'i % MAIN_HELP_VISIBLE_LINES == 0' in actions
# The 9x12 atlas is unchanged; 14px line pitch reclaims the retired footer.
# Seven rows begin at y14 and end at y109, within the new y116 dark window.
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
assert "int top, uint16_t ink" in copyright

assert "EspMainMenuSkin_uiAccent()" not in starfield
# k.bmp is already a 20x20 tiled native PAK asset in gameplay.
# In HELP it is copied as eight native nearest-neighbour 16x16 tiles,
# creating a compact 128x16 illustration in the central reading window.
# The 1:3:1 input proportions come from the real gameplay classifier,
# while the UI remains a visual-only demonstration.
hud_c = (SRC / "esp_native_gameplay_hud.c").read_text()
shim = (ROOT / "ESP32/test/oracles/esp32_sdl.cpp").read_text()
assert '"k.bmp"' in hud_c
assert 'bar->width != 20U || bar->height != 20U' in (
    SRC / "esp_native_gameplay_hud_direction.c").read_text()
assert '"k.bmp", false, &helpHudBarBG' in actions
assert 'helpHudBarBG.width != MAIN_HELP_HUD_SOURCE_TILE' in actions
assert 'DoomRPG_freeImage(helpSpaceOwner, &helpHudBarBG);' in actions
assert 'helpPaintTopHudIllustration(framebuffer)' in actions
assert 'if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL)' in actions
assert '#define MAIN_HELP_HUD_PAGE_SCROLL (6 * MAIN_HELP_VISIBLE_LINES)' in actions
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
assert 'EspNativeTexture_blit(helpHudBarBG.imgBitmap, NULL, &dst)' in actions
nativeTexture = (ROOT / "ESP32/src/esp_native_texture.cpp").read_text(encoding="utf-8")
assert 'const int sourceX = src.x + ((x - dst.x) * src.w) / dst.w;' in nativeTexture
assert 'const int sourceY = src.y + ((y - dst.y) * src.h) / dst.h;' in nativeTexture
assert 'static const char* const labels[3] = {"MENU", "PASS TURN", "MAP"};' in actions
assert 'static const HelpHudGlyph helpHudGlyphs[]' in actions
assert 'framebuffer[(y + row) * DOOMRPG_LOGICAL_WIDTH +' in actions
assert 'dstX] = white; /* readable white breath, never blinking */' in actions
# "3 VIRTUAL BUTTONS" is WHITE, as are the tiny ROM button glyphs;
# LEFT/CENTER/RIGHT prefixes use the theme's first, second and third tones.
assert "/* The control-identification caption stays white. The three" in actions
hud_paint_section = actions.split(
    'if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL) {', 1
)[1].split('    else if (menuSystem->scrollIndex == MAIN_HELP_SCENE_PAGE_SCROLL &&', 1)[0]
assert 'helpDrawPositionCaption(doomRpg, canvas, label, i - 2,' in hud_paint_section
# TOP HUD and the three illustrated touch rows keep their shared loop;
# BOTTOM HUD now uses two direct captions plus a third compact line.
assert actions.count('helpDrawPositionCaption(doomRpg, canvas, label, i - 2,') == 2
assert 'helpDrawPositionCaption(doomRpg, canvas, label, i - 2,' in actions
assert 'static void helpDrawPositionCaption(' in actions
assert 'static const int tones[3] = {1, 4, 7};' in actions
assert 'EspNativeCanvas_drawText(canvas, prefixes[zone], 12, y, 0);' in actions
assert '12 + (int)n * 7, y, 0);' in actions
assert 'DoomRPG_setFontColor(doomRpg, 0xffffffffU);' in hud_paint_section
assert 'dstX] = white; /* readable white breath, never blinking */' in actions

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

scene_api = (INC / "esp_native_help_scene.h").read_text()
# Page 5: three native miniatures, 128x80 at x16..143/y30..109:
# real top k.bmp, immutable PAK wall/perspective with NO action contours,
# and actual bottom k/m/l/o/a HUD from the already-allocated bounded cache.
assert '#define MAIN_HELP_GAMEPLAY_PAGE_SCROLL (4 * MAIN_HELP_VISIBLE_LINES)' in actions
assert 'static int helpPaintMainGameplayOverview(uint16_t* fb)' in actions
assert 'EspNativeHelpScene_paintGameplay(fb)' in actions
assert 'if (!helpPaintMainGameplayOverview(framebuffer)) return 0;' in actions
assert 'MAIN_HELP_GAMEPLAY_TOP_Y + y' in actions
assert 'MAIN_HELP_GAMEPLAY_BOTTOM_Y + y' in actions
assert '#define ESP_NATIVE_HELP_GAMEPLAY_X 16' in scene_api
assert '#define ESP_NATIVE_HELP_GAMEPLAY_Y 46' in scene_api
assert '#define ESP_NATIVE_HELP_GAMEPLAY_W 128' in scene_api
assert '#define ESP_NATIVE_HELP_GAMEPLAY_H 48' in scene_api
assert 'int EspNativeHelpScene_paintGameplay(uint16_t* framebuffer)' in scene_api
passive = actions.split('static int helpPaintMainGameplayOverview(uint16_t* fb)',1)[1].split('static int paintHelp(',1)[0]
assert 'helpPaintTopHudIllustration(fb)' in passive
assert 'helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH]' in passive
assert 'helpPaintTouchCell(' not in passive
assert 'helpPaintTouchDividers(' not in passive
assert 30 + 16 == 46 and 46 + 48 == 94 and 94 + 16 == 110
assert 16 + 128 == 144 and 110 < 120

# One smoothstep breath source, 32 x 80ms = 2.56s.
# Keep backing intact; both 1-pixel colored borders and colored glyphs breathe.
scene = (SRC / "esp_native_help_scene.c").read_text()
hud_c = (SRC / "esp_native_gameplay_hud.c").read_text()
frame_c = (SRC / "esp_native_gameplay_frame.c").read_text()
assert '#define ESP_NATIVE_HELP_BREATH_STEP_MS 80U' in scene_api
assert '#define ESP_NATIVE_HELP_BREATH_STEPS 32U' in scene_api
assert 'EspNativeHelpScene_breathStep(now - helpSpaceEpoch)' in actions
assert 'EspNativeHelpScene_breathLevel((uint8_t)step)' in actions
assert 'EspNativeHelpScene_breathPower(level)' in scene
assert 'EspNativeHelpScene_breathPower(' in actions
assert '(48U - 2U * level) * 255U + 2048U' in scene
assert 'const uint16_t neon = (uint16_t)(((uint32_t)power * 31U) / 255U);' in actions
assert 'const unsigned int scale =' in actions
assert '164U + (uint32_t)breathPower * 92U / 255U' in actions
assert 'dstX] = white; /* readable white breath, never blinking */' in actions
assert 'MAIN_HELP_HUD_BAR_Y + 4, power);' in actions
assert ('MENU/PASS/MAP remain legible throughout the breath.' in actions
        or 'imaginary boundaries alone breathe:' in actions)
assert actions.count('if (level == 0U) {') == 0  # no binary blackout
wave = [n if n <= 16 else 32 - n for n in range(32)]
assert wave == list(range(17)) + list(range(15, 0, -1))
power = [(n * n * (48 - 2 * n) * 255 + 2048) // 4096 for n in wave]
assert power[0] == 0 and power[16] == 255 and power[-1] > 0
assert all(a <= b for a, b in zip(power[:16], power[1:17]))
assert max(abs(a - b) for a, b in zip(power, power[1:])) < 26
assert 'helpHudBreathStep == helpBreathStepAt(now)' in actions
assert '!helpIsAnimatedHudPage(EspNativeRootOwners_live()->menuSystem->scrollIndex)' in actions
assert 'helpHudBreathStep = step;' in actions
assert actions.count('helpHudBreathStep = -1;') == 3
assert 'helpExpectedFrameFNV = finalFNV;' in actions
assert 'EspNativeGameplayInput_route(' not in actions
assert 'EspNativeGameplayHud_routeInitial(' not in actions
assert 'DoomRPG_esp32MainMenuReturnToMain(' in actions

# Real HUD miniature is produced by gameplay's own 5-asset painter.
# The real HUD source y100..119 is a temporary scratch before starfield repaint.
assert 'static uint16_t helpBottomHudMini[MAIN_HELP_HUD_BAR_WIDTH *' in actions
assert 'helpBottomHudReady = 0U;' in actions
assert 'EspNativeGameplayHud_paintHelpBottomBand(fb)' in actions
assert 'static int helpPaintBottomHudIllustration(' in actions
assert '#define MAIN_HELP_BOTTOM_HUD_BAR_Y (MAIN_HELP_HUD_BAR_Y - 12)' in actions
assert macro("MAIN_HELP_HUD_BAR_Y") == 43
assert macro("MAIN_HELP_LINE_HEIGHT") == 14
assert 43 - 12 == 31
bottom_section = actions.split(
    'static int helpPaintBottomHudIllustration(uint16_t* fb) {', 1
)[1].split('/* Build the exact native 4x3 WEAPONS panel', 1)[0]
assert "MAIN_HELP_BOTTOM_HUD_BAR_Y + y" in bottom_section
assert "MAIN_HELP_HUD_BAR_Y + y" not in bottom_section
bottom_page = actions.split(
    'else if (menuSystem->scrollIndex == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL) {', 1
)[1].split('else if (menuSystem->scrollIndex == MAIN_HELP_WEAPON_PAGE_SCROLL) {', 1)[0]
assert 'helpDrawPositionCaption(doomRpg, canvas, left, 0, 52);' in bottom_page
assert 'helpDrawPositionCaption(doomRpg, canvas, center, 1, 64);' in bottom_page
assert 'EspNativeCanvas_drawText(canvas, "RIGHT:", 2, 76, 0);' in bottom_page
assert 'EspNativeCanvas_drawText(canvas, right + 7, 46, 76, 0);' in bottom_page
assert 'EspNativeCanvas_drawText(canvas, touch, 8, 88, 0);' in bottom_page
assert 'EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);' not in bottom_page
assert 31 + 16 - 1 == 46 < 52
assert 52 + 12 == 64 and 64 + 12 == 76
assert 76 + 12 == 88 and 88 + 12 == 100
assert 'helpPaintBottomHudIllustration(framebuffer)' in actions
assert 'MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL (10 * MAIN_HELP_VISIBLE_LINES)' in actions
assert 'helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x]' in actions
assert 'static uint16_t helpTintBottomHudWeaponBlue(' in actions
assert 'helpTintBottomHudWeaponBlue(*pixel, power)' in actions
bottom_paint = actions.split(
    'static int helpPaintBottomHudIllustration(uint16_t* fb) {', 1
)[1].split('static int helpBuildWeaponGridMini(', 1)[0]
assert 'const uint16_t amber' not in bottom_paint
assert 'buttonLeft; x <= buttonRight; ++x' in bottom_paint
assert 'if (power != 0U)' in bottom_paint
assert 'const uint32_t keep = 256U - mix;' in actions
assert '((uint32_t)power * 160U) / 255U' in actions
assert 'const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;' in actions
assert 'const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;' in actions
assert '"HP30"' not in actions and '"AR20"' not in actions
assert '"AM12"' not in actions and 'skin = 0xfba0U' not in actions
assert 'int EspNativeGameplayHud_paintHelpBottomBand(' in hud_c
assert 'paintPrepared(&prepared, framebuffer, NULL, 1)' in hud_c
assert 'if (!bottomOnly) drawTopTouchNotches(framebuffer, stats);' in hud_c
# The full amber corner overlay stays LIVE in gameplay, but the read-only
# HELP thumbnail omits it (paintPrepared called with bottomOnly=1).
assert 'if (status == ESP_NATIVE_GAMEPLAY_HUD_OK && !bottomOnly) {' in hud_c
assert 'drawWeaponTouchCorners(framebuffer, stats);' in hud_c
assert 'paintPrepared(&prepared, framebuffer, outStats, 0)' in hud_c
assert all(f'openAsset("{x}.bmp", &bmp,' in hud_c
           for x in ('k','m','l','o','a'))
assert 'static const int tones[3] = {1, 4, 7};' in actions

# Return to the ACTUAL FIRST 2026-11-08 PAK-backed DOOR/WALL scene.
# One 64x64 4bpp source texture selected from the pack's live logical IDs,
# salted by staged flash-map and HELP opening epoch. No BSP/plane renderer,
# no screenshot, no ZIP runtime, no startup gameplay world owner.
pack = (SRC / "esp_asset_pack.cpp").read_text()
pack_api = (INC / "esp_asset_pack.h").read_text()
controls = (SRC / "esp_native_gameplay_controls.c").read_text()
assert '#define MAIN_HELP_SCENE_PAGE_SCROLL (5 * MAIN_HELP_VISIBLE_LINES)' in actions
assert '#define MAIN_HELP_ROW_FIRST_SCROLL (7 * MAIN_HELP_VISIBLE_LINES)' in actions
assert '#define MAIN_HELP_ROW_LAST_SCROLL (9 * MAIN_HELP_VISIBLE_LINES)' in actions
assert 'helpIsRowHelpPage(menuSystem->scrollIndex)' in actions
assert 'EspNativeHelpScene_paintRow(' in actions
assert '63 + (i - 2) * MAIN_HELP_LINE_HEIGHT' in actions
assert 'EspNativeHelpScene_paintRow(uint16_t* framebuffer, uint8_t sourceRow,' in scene_api
assert 'sourceRow > 2U ||' in scene
assert 'static void helpPaintTouchCell(' in scene
assert 'static void helpPaintTouchDividers(' not in scene
assert 'const int top = 39, bottom = 61;' in scene
assert 'sourceRow, col,' in scene
assert 'top, bottom, level);' in scene
assert actions.count('helpSceneBreathStep = step;') == 2
assert 'EspNativeHelpScene_isReady()' in actions
assert 'EspNativeHelpScene_paint(' in actions
assert 'helpSceneBreathStep == helpBreathStepAt(now)' in actions
assert 'EspNativeHelpScene_reset();' in actions
assert 'EspAssetPack_mapFlashCachedMapId()' in actions
assert 'EspNativeHelpScene_begin(cachedMapId,' in actions
assert 'helpSpaceEpoch);' in actions
assert 'int EspNativeHelpScene_begin(uint8_t flashCachedMapId, uint32_t helpEpochMs);' in scene_api
assert 'static int loadOriginalMaterial(uint8_t cachedMapId, uint32_t seed)' in scene
assert '#define WALL_BYTES 2048U' in scene
assert '#define MAX_CANDIDATES 24U' in scene
assert 'cachedMapId * 57U' in scene and 'seed / 13000U' in scene
assert 'attempt * 37U' in scene
assert 'logicalCount > 256U ? 256U : logicalCount' in scene
assert 'EspAssetPack_findEntry("wtexels.bin", &walls)' in scene
assert 'EspAssetPack_findEntry("palettes.bin", &palettes)' in scene
assert 'EspAssetPack_findEntry("mappings.bin", &mappings)' in scene
assert 'material.ready = 1U;' in scene
assert 'material.logicalId = (uint16_t)logical;' in scene
assert 'sourceBgrToFramebuffer565(' in scene
assert 'static void helpPaintOriginalBackground(uint16_t* fb)' in scene
assert 'const int wallTop = 19 * flank / 54;' in scene
assert 'const int wallBottom = (PREVIEW_H - 1) - 17 * flank / 54;' in scene
assert 'ink = sampleWall(u, v);' in scene
assert '(int)sourceRow * 19' in scene and '(int)sourceRow * 23' in scene
assert 'static uint16_t helpBottomHudMini[' in actions
assert 'const int buttonLeft = MAIN_HELP_HUD_BAR_X +' in actions
assert 'ESP_NATIVE_HUD_WEAPON_LEFT * MAIN_HELP_HUD_BAR_WIDTH /' in actions
assert 'const int buttonRight = MAIN_HELP_HUD_BAR_X +' in actions
assert 'ESP_NATIVE_HUD_WEAPON_RIGHT + 1' in actions
assert 16 + 100 * 128 // 160 == 96
assert 16 + (159 + 1) * 128 // 160 - 1 == 143
assert 'separators[2] = {80, 115}' not in actions
assert 'lastWorld' not in scene
assert 'captureGameplayViewport' not in frame_c
assert 'uint8_t EspAssetPack_mapFlashCachedMapId(void)' in pack
assert 'uint8_t EspAssetPack_mapFlashCachedMapId(void);' in pack_api
assert 'EspAssetPack_mapFlashStage(' not in actions
assert 'EspAssetPack_mapFlashPrepare(' not in actions
assert 'EspMapRuntime_' not in scene
assert 'EspBspReader_' not in scene
assert 'scene.planeMap' not in scene
assert 'ESP_NATIVE_GAMEPLAY_ACTION_' not in scene
assert 'mediaTexels' not in scene
assert 'malloc(' not in scene
assert 'static const int xZone[4] = {0, 48, 96, PREVIEW_W};' in scene
assert 'static const int yZone[4] = {0, 22, 45, PREVIEW_H};' in scene
assert 'PREVIEW_X 8' in scene and 'PREVIEW_Y 29' in scene
assert 'PREVIEW_W 144' in scene and 'PREVIEW_H 68' in scene
assert 'row == 1 && col == 1 ? 0xf800U' in scene
assert 'row == 2 && col != 1 ? 0xffe0U' in scene
for color in ('NEON_GREEN_CORE  0x07e0U',
              'NEON_RED_CORE    0xf800U',
              'NEON_YELLOW_CORE 0xffe0U'):
    assert color in controls

# Weapon selector: exact existing PAK bitshapes/mappings 4x3 painter,
# local demonstration player only, same 144x60 BSS stretched to 144x76
# inside the newly usable footer space.
grid_c = (SRC / 'esp_native_gameplay_hub_weapon_grid.c').read_text()
assert '#define MAIN_HELP_WEAPON_PAGE_SCROLL (11 * MAIN_HELP_VISIBLE_LINES)' in actions
assert '#define MAIN_HELP_WEAPON_MINI_X 8' in actions
assert '#define MAIN_HELP_WEAPON_MINI_Y 39' in actions
assert '#define MAIN_HELP_WEAPON_MINI_W 144' in actions
assert '#define MAIN_HELP_WEAPON_MINI_H 60' in actions
assert '#define MAIN_HELP_WEAPON_DRAW_H 76' in actions
assert 'const int sourceY = y * MAIN_HELP_WEAPON_MINI_H /' in actions
assert 'for (y = 0; y < MAIN_HELP_WEAPON_DRAW_H; ++y)' in actions
assert 'static uint16_t helpWeaponMini[MAIN_HELP_WEAPON_MINI_W *' in actions
assert 'helpBuildWeaponGridMini(framebuffer)' in actions
assert 'helpDrawWeaponGridMini(framebuffer);' in actions
assert 'helpDrawWeaponButtonCue(framebuffer);' in actions
assert 'helpBottomHudMini[sy * MAIN_HELP_HUD_BAR_WIDTH + sx]' in actions
assert 'const int dstX = 108, dstY = 12, dstW = 46, dstH = 15;' in actions
assert 'helpTintBottomHudWeaponBlue(original, power)' in actions
assert 'helpHudBreathStep = step;' in actions
assert 'scroll == MAIN_HELP_WEAPON_PAGE_SCROLL;' in actions
assert 'static uint16_t helpWeaponPreviewLitPixel(' in actions
assert 'static void helpIlluminateWeaponGridMini(void)' in actions
assert 'helpIlluminateWeaponGridMini();' in actions
assert 'original == 0x0841U || original == 0x0000U' in actions
assert 'red = ((uint32_t)(original >> 11) * 5U) / 4U + 2U;' in actions
assert 'demo.weapons = 0x0fffU;' in actions
assert 'sourceX + x * sourceWidth / dstW' in actions
assert 108 > 12 + len("WEAPON SELECT") * 7  # no title/icon overlap
assert 108 + 46 <= 154 and 12 + 15 - 1 < 27
assert 12 >= 12  # page counter raster ends at y11
assert 108 + 46 - 1 == 153 and 12 + 15 - 1 == 26
assert 'if (!helpBottomHudReady)' in actions
assert 'EspEntityDefTypeCatalog_buildFromPackEntry(&entities)' in actions
assert 'EspNativeGameplayHubWeaponGrid_paint(framebuffer, &demo, 2U)' in actions
assert 'demo.active = 1U;' in actions
assert 'demo.weapons = 0x0fffU;' in actions
assert 'Tap owned to equip' not in lines
assert 'EspNativeCanvas_drawText(canvas, detail, 12, 85, 0);' not in actions
assert 'demo.weapon = 2U;' in actions
assert 'helpWeaponMiniAttempted = 1U;' in actions
assert 'helpWeaponMiniReady = 1U;' in actions
assert 'const int sy = 35 + y * 83 / (MAIN_HELP_WEAPON_MINI_H - 1);' in actions
assert 'const int sx = 2 + x * 155 / (MAIN_HELP_WEAPON_MINI_W - 1);' in actions
assert 'selectedWeapon >= ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT' in grid_c
assert 'EspAssetPack_findEntry("stexels.bin", &stexels)' in grid_c
assert 'GRID_CELL_WIDTH 39' in grid_c and 'GRID_CELL_HEIGHT 28' in grid_c
# Caption at y27..38, cached mini 144x60, drawn scaled at y39..114.
# The 17,280-byte source cache and native pixel backing remain unchanged.
assert 8 + 144 - 1 == 151
assert 39 + 76 - 1 == 114 < 117
assert 27 + 12 - 1 == 38 < 39
assert 2 + (144 - 1) * 155 // (144 - 1) == 157
assert 35 + (60 - 1) * 83 // (60 - 1) == 118
assert all(2 <= 2 + x * 155 // 143 <= 157 for x in range(144))
assert all(35 <= 35 + y * 83 // 59 <= 118 for y in range(60))
assert 144 * 60 * 2 == 17280
assert 17280 - 10752 == 6528  # increase over previous 128x42 cache
assert 'mini=%ux%u bytes=%u' in actions

# Hardware feedback on CYD TOUCH: turn symbols must curve, not be
# three perpendicular line segments; weapon controls need explicit words.
assert 'static void helpPaintTurnArc(' in scene
assert 'helpPaintTurnArc(framebuffer, cx + (col == 0 ? -2 : 2), cy, col, core);' in scene
assert 'const int mirror = col == 0 ? 1 : -1;' in scene
assert 'const int headCx = cx - mirror * 3;' in scene
assert 'segment(fb, headCx - mirror * 3, cy - 4,' in scene
assert 'headCx - mirror * 7, cy, core);' in scene
assert 'segment(fb, headCx - mirror * 7, cy,' in scene
assert 'headCx - mirror * 3, cy + 4, core);' in scene
assert 'static const int8_t turnCurve[][2]' in scene
curve_data = scene.split('static const int8_t turnCurve[][2] = {', 1)[1].split('};', 1)[0]
turn_points = [
    (int(x), int(y)) for x, y in
    re.findall(r'\{\s*(-?\d+),\s*(-?\d+)\s*\}', curve_data)
]
assert turn_points == [
    (6, 5), (6, 3), (6, 0), (5, -3),
    (3, -5), (1, -6), (-1, -6),
    (-3, -6), (-5, -4), (-6, -2), (-6, 0)]
assert all(abs(b[0] - a[0]) <= 3 and abs(b[1] - a[1]) <= 3
           for a, b in zip(turn_points, turn_points[1:]))
# Turning symbols and their mirrored arrowheads fit their own green cell.
for left, right in ((8, 55), (104, 151)):
    cx = (left + right) // 2
    cy = (51 + 73) // 2
    mirror = 1 if left == 8 else -1
    # The loop keeps its previously validated 2px offset. The chevron
    # remains independent but comes back 2px after the too-wide 5px shift.
    # Its tip is still outside the loop on both sides.
    shifted_cx = cx + (-2 if left == 8 else 2)
    curve = [(shifted_cx + mirror * x, cy + y) for x, y in turn_points]
    head_cx = shifted_cx - mirror * 3
    chevron = [(head_cx - mirror * dx, cy + dy)
               for dx, dy in ((3, -4), (7, 0), (3, 4))]
    assert min(x for x, _ in curve) == (23 if left == 8 else 123)
    assert max(x for x, _ in curve) == (35 if left == 8 else 135)
    assert min(x for x, _ in chevron) == (19 if left == 8 else 135)
    assert max(x for x, _ in chevron) == (23 if left == 8 else 139)
    # The tip remains four pixels out from the nearest curve coordinate.
    assert (min(x for x, _ in curve) - min(x for x, _ in chevron)
            if left == 8 else max(x for x, _ in chevron) -
            max(x for x, _ in curve)) == 4
    assert all(left + 2 < x < right - 2 and 51 < y < 73
               for x, y in curve + chevron)

assert 'static void helpPaintWeaponCaption(' in scene
assert 'static void helpPaintWeaponWord(' in scene
assert 'static const HelpWeaponGlyph helpWeaponGlyphs[]' in scene
assert 'col == 0 ? "PREV" : "NEXT"' in scene
assert 'const int textTop = top + ((bottom - top + 1) - 15) / 2;' in scene
assert 'for (y = textTop; y <= textTop + 15; ++y)' in scene
assert 'helpPaintWeaponWord(fb, centerX, textTop, "WPN", ink);' in scene
assert 'helpPaintWeaponWord(fb, centerX, textTop + 8, secondLine, ink);' in scene
assert 'if (row == 2 && col != 1)' in scene
assert 'helpPaintWeaponCaption(fb, (left + right) / 2, top, bottom,' in scene
assert 'col == 0 ? "PREV" : "NEXT", core);' in scene
assert 'static void helpPaintTouchDividers(' not in scene
assert 'dim565(*pixel, 160U)' in scene
glyph_data = scene.split('static const HelpWeaponGlyph helpWeaponGlyphs[] = {', 1)[1].split('};', 1)[0]
glyphs = re.findall(r"'([A-Z])',\s*\{([^}]+)\}", glyph_data)
assert {letter for letter, _ in glyphs} == set("WPNREVXT")
assert len(glyphs) == 8
for letter, row_text in glyphs:
    rows = re.findall(r'0x([0-9a-f]{2})', row_text)
    assert len(rows) == 7, letter
    assert all(0 < int(mask, 16) < 32 for mask in rows), letter

# Two 5x7 lines, a one-pixel gap, and the dimmed reading pad must
# all stay inside the bottom-left and bottom-right 42/43x18px zones.
for left, right in ((8, 55), (104, 151)):
    cx = (left + right) // 2
    assert left < cx - 14 and cx + 13 < right
    for text in ("WPN", "PREV", "NEXT"):
        w = len(text) * 6 - 1
        x = cx - w // 2
        assert left < x and x + w - 1 < right
    top, bottom = 74, 96
    caption_height = 2 * 7 + 1
    text_top = top + (bottom - top + 1 - caption_height) // 2
    assert text_top == 78
    assert (text_top + 8 + 6) == 92
    assert (text_top + text_top + caption_height - 1) // 2 == (top + bottom) // 2
    assert top < text_top and text_top + 15 < bottom

assert 8 <= 8 + 144 <= 155
assert 29 + 68 - 1 < 100
assert 'helpCorridorPixel' not in scene
assert 'lastWorld' not in scene
assert 'ESP_NATIVE_HELP_BREATH_STEPS 32U' in scene_api
assert 'if (shift == helpSpaceShift &&' in actions
assert 'helpExpectedFrameFNV = finalFNV;' in actions


assert "EspMainMenuSkin_uiAccentDim()" not in starfield
assert "EspNativeHelpNav_scroll(" in actions
assert 'PAGE-%s scroll=%d->%d half=%s' in actions
assert 'PAGE-UP scroll' not in actions and 'PAGE-DOWN scroll' not in actions
assert "helpExpectedFrameFNV = finalFNV;" in actions
assert "before != helpExpectedFrameFNV" in actions
assert "helpStop();" in actions
assert "DoomRPG_esp32MainMenuReturnToMain(" in actions
assert "EspNativePresentation_clipEnabled()" in actions
assert "EspMainMenuSkin_paintPlate" in skin

for scroll in range(len(lines)):
    clamped = min(scroll, len(lines) - 7)
    assert 0 <= clamped <= len(lines) - 7
    assert clamped + 7 <= len(lines)
    page = (clamped + 6) // 7 + 1
    assert 1 <= page <= (len(lines) + 6) // 7

print("PASS HELP: 17 pages; original live-PAK wall/door illustration restored,")
print("           bottom HUD button scaled to real hitbox, icon inline title,")
print("           12 PAK weapon icons cached 144x60, drawn 144x76; 2 half-screen touch zones")
