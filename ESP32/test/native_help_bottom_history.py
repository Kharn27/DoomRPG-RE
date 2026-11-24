"""Exact historical HELP BOTTOM HUD section from the pre-blue PR #332 head.

Only SDL-F17 and SDL-F19 Git-blob oracles use this. Their original expected
hashes are unchanged; just undo this *later* visual-only feature before
reversing the actual SDL migration. No runtime imports or backing-store reads.
"""

PRE_HELP_BOTTOM_BLUE = "/* Original k/m/l/o/a BMPs, exact gameplay HUD painter, no substitutes.\n * Stage y100..119 once before HELP footer plates overwrite those pixels,\n * and keep only a 128x16 RGB565 thumbnail for subsequent soft repaints. */\nstatic int helpPaintBottomHudIllustration(uint16_t* fb) {\n    const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());\n    const uint8_t power = EspNativeHelpScene_breathPower(\n        helpBreathLevel(step));\n    int x, y;\n    if (fb == NULL) return 0;\n    if (!helpBottomHudReady) {\n        if (!EspNativeGameplayHud_paintHelpBottomBand(fb)) {\n            printf(\"[MAINHELP] BOTTOM-HUD FAIL native k/m/l/o/a ownership\\n\");\n            return 0;\n        }\n        for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {\n            for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {\n                const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;\n                const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x] =\n                    fb[sy * DOOMRPG_LOGICAL_WIDTH + sx];\n            }\n        }\n        helpBottomHudReady = 1U;\n    }\n    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y)\n        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x)\n            fb[(MAIN_HELP_HUD_BAR_Y + y) * DOOMRPG_LOGICAL_WIDTH +\n               MAIN_HELP_HUD_BAR_X + x] =\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x];\n\n    /* Scale the ACTUAL weapon hitbox x100..159 to x96..143 of this\n     * 128px miniature. Highlight the same four 3px touch corners as the\n     * canonical gameplay HUD (not arbitrary full-height blue dividers).\n     * x97/x142 and y44..46/y55..57; no framebuffer region is tinted. */\n    {\n        const int buttonLeft = MAIN_HELP_HUD_BAR_X +\n            ESP_NATIVE_HUD_WEAPON_LEFT * MAIN_HELP_HUD_BAR_WIDTH /\n            DOOMRPG_LOGICAL_WIDTH;\n        const int buttonRight = MAIN_HELP_HUD_BAR_X +\n            (ESP_NATIVE_HUD_WEAPON_RIGHT + 1) *\n            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH - 1;\n        const uint16_t amber = (uint16_t)(\n            (((31U * (uint32_t)power) / 255U) << 11) |\n            (((40U * (uint32_t)power) / 255U) << 5));\n        int edge, corner;\n        if (power > 0U) {\n            for (edge = 0; edge < 2; ++edge) {\n                const int buttonX = edge == 0\n                    ? buttonLeft + 1 : buttonRight - 1;\n                for (corner = 0; corner < 2; ++corner) {\n                    const int firstY = MAIN_HELP_HUD_BAR_Y +\n                        (corner == 0 ? 1 : MAIN_HELP_HUD_BAR_HEIGHT - 4);\n                    for (y = firstY; y < firstY + 3; ++y)\n                        fb[y * DOOMRPG_LOGICAL_WIDTH + buttonX] = amber;\n                }\n            }\n        }\n    }\n    helpHudBreathStep = step;\n    return 1;\n}\n\n"

PRE_HELP_WEAPON_BLOCK = "/* Build the exact native 4x3 WEAPONS panel from its real sprite PAK\n * decoder, *without* a resident player, world, modal, turn or gameplay UI.\n * The compact immutable entity-type catalog may be initialized once from\n * entities.db; the normal game uses that same catalog later. During this\n * one-time HELP paint, 160x120 RGB565 serves as a scratch surface. The next\n * helpDrawStarfield() immediately replaces it before presentation. */\nstatic int helpBuildWeaponGridMini(uint16_t* framebuffer) {\n    EspAssetPackEntry entities;\n    EspNativeGameplayPlayerState demo;\n    int x, y, ok = 0;\n    if (helpWeaponMiniReady) return 1;\n    if (helpWeaponMiniAttempted || framebuffer == NULL ||\n        EspAssetPack_isOpen()) return 0;\n    helpWeaponMiniAttempted = 1U;\n    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;\n    if (!EspEntityDefTypeCatalog_isReady() &&\n        (!EspAssetPack_findEntry(\"/entities.db\", &entities) ||\n         !EspEntityDefTypeCatalog_buildFromPackEntry(&entities)))\n        goto done;\n    memset(&demo, 0, sizeof(demo));\n    demo.active = 1U;\n    demo.weapon = 2U;   /* Pistol equipped: sample only, never applied. */\n    demo.weapons = 0x0fffU; /* HELP demonstration: all 12 PAK-native weapons owned, 4x3 full-color grid. */\n    if (!EspNativeGameplayHubWeaponGrid_paint(framebuffer, &demo, 2U))\n        goto done;\n    /* Source grid has EXACT native bounds x2..157 / y35..118.\n     * Downsample nearest-neighbour over the FULL closed source rectangle\n     * (including right and bottom borders) to x8..151 / y39..98.\n     * Retain just one bounded static RGB565 miniature (17,280 B). */\n    for (y = 0; y < MAIN_HELP_WEAPON_MINI_H; ++y) {\n        const int sy = 35 + y * 83 / (MAIN_HELP_WEAPON_MINI_H - 1);\n        for (x = 0; x < MAIN_HELP_WEAPON_MINI_W; ++x) {\n            const int sx = 2 + x * 155 / (MAIN_HELP_WEAPON_MINI_W - 1);\n            helpWeaponMini[y * MAIN_HELP_WEAPON_MINI_W + x] =\n                framebuffer[sy * DOOMRPG_LOGICAL_WIDTH + sx];\n        }\n    }\n    helpWeaponMiniReady = 1U;\n    ok = 1;\ndone:\n    EspAssetPack_close();\n    printf(\"[MAINHELP] WEAPON-PREVIEW ready=%u source=real-native-grid/PAK icons=12 layout=4x3 mini=%ux%u bytes=%u world=no playerMutation=no packClosed=%u\\n\",\n           ok ? 1U : 0U,\n           (unsigned int)MAIN_HELP_WEAPON_MINI_W,\n           (unsigned int)MAIN_HELP_WEAPON_MINI_H,\n           (unsigned int)sizeof(helpWeaponMini),\n           EspAssetPack_isOpen() ? 0U : 1U);\n    return ok;\n}\nstatic void helpDrawWeaponGridMini(uint16_t* fb) {\n    int x, y;\n    if (fb == NULL || !helpWeaponMiniReady) return;\n    for (y = 0; y < MAIN_HELP_WEAPON_MINI_H; ++y)\n        for (x = 0; x < MAIN_HELP_WEAPON_MINI_W; ++x)\n            fb[(MAIN_HELP_WEAPON_MINI_Y + y) * DOOMRPG_LOGICAL_WIDTH +\n               MAIN_HELP_WEAPON_MINI_X + x] =\n                helpWeaponMini[y * MAIN_HELP_WEAPON_MINI_W + x];\n}\n\n/* On the WEAPON SELECT help page: crop the REAL weapon touch target\n * [100..159] from the canonical HUD thumbnail. Downsample to 36x12 so\n * its miniature sits on the SAME BASELINE as the themed title at y14.\n * No recreated weapon icon, no cropped face/compass neighbours. */\nstatic void helpDrawWeaponButtonCue(uint16_t* fb) {\n    const int sourceX = ESP_NATIVE_HUD_WEAPON_LEFT *\n                        MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH;\n    const int sourceWidth = (ESP_NATIVE_HUD_WEAPON_RIGHT + 1 -\n                             ESP_NATIVE_HUD_WEAPON_LEFT) *\n                            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH;\n    const int dstX = 116, dstY = 14, dstW = 36, dstH = 12;\n    int x, y;\n    if (!helpBottomHudReady || fb == NULL) return;\n    for (y = 0; y < dstH; ++y) {\n        const int sy = y * MAIN_HELP_HUD_BAR_HEIGHT / dstH;\n        for (x = 0; x < dstW; ++x) {\n            const int sx = sourceX + x * sourceWidth / dstW;\n            fb[(dstY + y) * DOOMRPG_LOGICAL_WIDTH + dstX + x] =\n                helpBottomHudMini[sy * MAIN_HELP_HUD_BAR_WIDTH + sx];\n        }\n    }\n}\n\n"
PRE_HELP_WEAPON_ANIM = "static int helpIsAnimatedHudPage(int scroll) {\n    return scroll == MAIN_HELP_HUD_PAGE_SCROLL ||\n           scroll == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL;\n}"
PRE_HELP_WEAPON_ANIM_NEW = "static int helpIsAnimatedHudPage(int scroll) {\n    return scroll == MAIN_HELP_HUD_PAGE_SCROLL ||\n           scroll == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL ||\n           scroll == MAIN_HELP_WEAPON_PAGE_SCROLL;\n}"

PRE_HELP_BOTTOM_LAYOUT = "    else if (menuSystem->scrollIndex == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL) {\n        const char* title = DoomRPG_esp32MainMenuHelpLine(\n            MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL);\n        const char* caption = DoomRPG_esp32MainMenuHelpLine(\n            MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL + 1);\n        if (title == NULL || caption == NULL || visible != 7) return 0;\n        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);\n        DoomRPG_setFontColor(doomRpg, 0xffffffffU);\n        EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);\n        if (!helpPaintBottomHudIllustration(framebuffer)) return 0;\n        for (i = 2; i <= 4; ++i) {\n            const char* label = DoomRPG_esp32MainMenuHelpLine(\n                MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL + i);\n            if (label == NULL || label[0] == '\\0') return 0;\n            helpDrawPositionCaption(doomRpg, canvas, label, i - 2,\n                                    64 + (i - 2) * MAIN_HELP_LINE_HEIGHT);\n        }\n    }\n"
PRE_HELP_BOTTOM_LAYOUT_DEFINE = "/* BOTTOM HUD has no redundant subtitle: preview and labels move up 12px.\n * The TOP HUD miniature retains its existing y43 position. */\n#define MAIN_HELP_BOTTOM_HUD_BAR_Y (MAIN_HELP_HUD_BAR_Y - MAIN_HELP_LINE_HEIGHT)\n"
PRE_HELP_BOTTOM_MODEL_OLD_COMMENT = "/* Native ROM-owned CYD guide: no obsolete 2005 keypad help.txt parser,\n * no SD allocation/read and no pointer-heavy MenuItem table. ASCII text\n * is at most 20 glyphs wide (7px advance, x=12 on the 160px canvas).\n */"
PRE_HELP_BOTTOM_MODEL_NEW_COMMENT = "/* Native ROM-owned CYD guide: no obsolete 2005 keypad help.txt parser,\n * no SD allocation/read and no pointer-heavy MenuItem table. Normally\n * 20 glyphs max at 7px advance/x12. The BOTTOM HUD page alone has two\n * longer lines with dedicated x8 / split-x45 bounded placement. */"
PRE_HELP_BOTTOM_MODEL_OLD_PAGE = "    \"BOTTOM HUD BAR\",\n    \"STATUS AT A GLANCE\",\n    \"LEFT: HP + ARMOR\",\n    \"CENTER: FACE / AMMO\",\n    \"RIGHT: DIRECTION\",\n    \"\",\n    \"\","
PRE_HELP_BOTTOM_MODEL_NEW_PAGE = "    \"BOTTOM HUD BAR\",\n    \"LEFT: HP + ARMOR\",\n    \"CENTER: FACE / AMMO\",\n    \"RIGHT: DIRECTION / AMMO\",\n    \"Touch for weapon menu\",\n    \"\",\n    \"\","

def restore_pre_help_bottom_model(source: str) -> str:
    """Undo only the special HELP bottom-page copy and column width note."""
    assert source.count(PRE_HELP_BOTTOM_MODEL_NEW_COMMENT) == 1
    assert source.count(PRE_HELP_BOTTOM_MODEL_NEW_PAGE) == 1
    return source.replace(PRE_HELP_BOTTOM_MODEL_NEW_COMMENT,
                          PRE_HELP_BOTTOM_MODEL_OLD_COMMENT, 1).replace(
                          PRE_HELP_BOTTOM_MODEL_NEW_PAGE,
                          PRE_HELP_BOTTOM_MODEL_OLD_PAGE, 1)


def restore_pre_help_bottom_blue(source: str) -> str:
    """Undo later weapon demo, post-F19 HELP reflow and blue painter."""
    # Before the first blue HEL P addition, the title-side cue was static and
    # the weapon page had no breath repaint. Reconstruct the exact pre-PR
    # whole-file bytes without weakening the old historical Git blob hashes.
    weapon_start = "/* Keep the source PAK geometry"
    next_function = "static int paintHelp("
    assert source.count(weapon_start) == 1
    assert source.count(next_function) == 1
    begin = source.index(weapon_start)
    after = source.index(next_function, begin)
    replaced = source[begin:after]
    assert "helpIlluminateWeaponGridMini();" in replaced
    assert "helpTintBottomHudWeaponBlue(original, power)" in replaced
    source = source[:begin] + PRE_HELP_WEAPON_BLOCK + source[after:]
    assert source.count(PRE_HELP_WEAPON_ANIM_NEW) == 1
    source = source.replace(PRE_HELP_WEAPON_ANIM_NEW, PRE_HELP_WEAPON_ANIM, 1)
    # Reflow inserted one Y alias and a new bottom HELP-specific page body.
    assert source.count(PRE_HELP_BOTTOM_LAYOUT_DEFINE) == 1
    source = source.replace(PRE_HELP_BOTTOM_LAYOUT_DEFINE, "", 1)
    marker = "    else if (menuSystem->scrollIndex == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL) {"
    following = "    else if (menuSystem->scrollIndex == MAIN_HELP_WEAPON_PAGE_SCROLL) {"
    assert source.count(marker) == 1 and source.count(following) == 1
    begin = source.index(marker)
    end = source.index(following, begin)
    assert 'EspNativeCanvas_drawText(canvas, right + 7, 46, 76, 0);' in source[begin:end]
    source = source[:begin] + PRE_HELP_BOTTOM_LAYOUT + source[end:]

    new_begin = "/* Preserve the light/dark texture"
    after_marker = "/* Build the exact native 4x3 WEAPONS panel"
    assert source.count(new_begin) == 1 and source.count(after_marker) == 1
    start = source.index(new_begin)
    end = source.index(after_marker, start)
    new_blue = source[start:end]
    assert "static uint16_t helpTintBottomHudWeaponBlue(" in new_blue
    assert "helpTintBottomHudWeaponBlue(*pixel, power)" in new_blue
    assert "const uint16_t amber" not in new_blue
    # The reflow also moves only the BOTTOM HUD illustration Y origin.
    assert "MAIN_HELP_BOTTOM_HUD_BAR_Y" in new_blue
    new_blue = new_blue.replace("MAIN_HELP_BOTTOM_HUD_BAR_Y", "MAIN_HELP_HUD_BAR_Y")
    return source[:start] + PRE_HELP_BOTTOM_BLUE + source[end:]


def restore_pre_help_light_body_pages(source: str) -> str:
    """Undo only later HELP 2-4 and 12-16 body ink for frozen SDL blobs."""
    later = """            /* HELP pages 2-4 and 12-16 use the first (light) theme tone
             * for body text. Section titles keep their 3-band gradient. */
            const int scroll = menuSystem->scrollIndex;
            const uint16_t bodyTone =
                ((scroll >= MAIN_HELP_VISIBLE_LINES &&
                  scroll <= 3 * MAIN_HELP_VISIBLE_LINES) ||
                 (scroll >= 11 * MAIN_HELP_VISIBLE_LINES &&
                  scroll <= 15 * MAIN_HELP_VISIBLE_LINES))
                    ? helpTitleTopTone() : helpBodyMiddleTone();
            DoomRPG_setFontColor(doomRpg,
                (menuSystem->scrollIndex == 0 && i == 5)
                    ? 0xffffffffU : helpColor888(bodyTone));"""
    earlier = """            /* John Carmack's tribute closes the cover. */
            DoomRPG_setFontColor(doomRpg,
                (menuSystem->scrollIndex == 0 && i == 5)
                    ? 0xffffffffU : helpColor888(helpBodyMiddleTone()));"""
    assert source.count(later) == 1, "HELP light body pages delta unexpectedly changed"
    return source.replace(later, earlier, 1)

# HELP header/reading-window visual polish: undo ONLY these later pixels for
# frozen SDL-F17/F29 whole-file hashes. Never update original golden SHAs.
_PRE_FRAME_DECL = (
    "    uint16_t accent = EspMainMenuSkin_uiAccent();\n"
    "    uint16_t dimAccent = EspMainMenuSkin_uiAccentDim();\n"
)
_PRE_FRAME = """    /* Translucent dark reading window: edit the ONE shared RGB565 frame,
     * no alpha texture/overlay allocation and no extra framebuffer. */
    for (y = 13; y <= 98; ++y) {
        for (x = 5; x <= 154; ++x) {
            uint16_t* pixel = &framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x];
            *pixel = (uint16_t)((*pixel >> 1) & 0x7befU);
        }
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + 5] = dimAccent;
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + 154] = dimAccent;
    }
    for (x = 5; x <= 154; ++x) {
        framebuffer[12 * DOOMRPG_LOGICAL_WIDTH + x] = accent;
        framebuffer[99 * DOOMRPG_LOGICAL_WIDTH + x] = dimAccent;
    }
"""
_FRAMELESS = """    /* Keep the translucent dark reading window, without the redundant
     * colored rectangular frame. Original c.bmp stars remain visible. */
    for (y = 13; y <= 98; ++y) {
        for (x = 5; x <= 154; ++x) {
            uint16_t* pixel = &framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x];
            *pixel = (uint16_t)((*pixel >> 1) & 0x7befU);
        }
    }
"""
_HELP_HEADER = (
    '    /* Page header + x/16 take the SECOND theme tone on every HELP page. */\n'
    '    DoomRPG_setFontColor(doomRpg, helpColor888(helpBodyMiddleTone()));\n'
    '    EspNativeCanvas_drawText(canvas, "HELP / CYD", 12, 0, 0);'
)
_PRE_HELP_HEADER = (
    '    DoomRPG_setFontColor(doomRpg, helpColor888(helpTitleTopTone()));\n'
    '    EspNativeCanvas_drawText(canvas, "HELP / CYD", 12, 0, 0);'
)

def restore_pre_help_frame_polish(source: str) -> str:
    """Restore exact pre-polish header and frame for existing SDL Git SHAs."""
    assert source.count(_HELP_HEADER) == 1, "HELP header color drift"
    assert source.count(_FRAMELESS) == 1, "HELP frameless background drift"
    assert source.count("    int x, y;\n") >= 1
    source = source.replace(_HELP_HEADER, _PRE_HELP_HEADER, 1)
    source = source.replace(_FRAMELESS, _PRE_FRAME, 1)
    marker = "    int x, y;\n"
    anchor = "static int helpDrawStarfield(DoomRPG_t* doomRpg) {"
    start = source.index(anchor)
    # Pre-polish declarations were BEFORE `int x, y`, not after it.
    x_pos = source.index(marker, start)
    assert source[x_pos:x_pos+len(_PRE_FRAME_DECL)] != _PRE_FRAME_DECL
    return source[:x_pos] + _PRE_FRAME_DECL + source[x_pos:]


# PAGE4 changes postdate frozen SDL-F17/F19/F29 source blobs. Reverse only
# the newer UI body-color and ROM text ordering while reconstructing those
# historical sources. Never change the original SHA fixtures.
_PRE_PAGE4_MODEL = "    \"Moves take a turn.\",\n    \"Enemies take turns.\",\n    \"Plan each attack.\",\n    \"Actions use turns.\",\n    \"Think before moving.\","
_POST_PAGE4_MODEL = "    \"Moves take a turn.\",\n    \"Enemies take turns.\",\n    \"Actions use turns.\",\n    \"Plan each attack.\",\n    \"Think before moving.\","
_PRE_PAGE4_COMMENT = "            /* HELP pages 2-4 and 12-16 use the first (light) theme tone\n             * for body text. Section titles keep their 3-band gradient. */"
_POST_PAGE4_COMMENT = "            /* HELP pages 2-4 and 12-16 use the first (light) theme tone,\n             * except page 4 body copy: neutral white independent of theme.\n             * Both page titles and HELP / CYD keep their themed colors. */"
_PRE_PAGE4_COLOR = "                (menuSystem->scrollIndex == 0 && i == 5)\n                    ? 0xffffffffU : helpColor888(bodyTone));"
_POST_PAGE4_COLOR = "                ((menuSystem->scrollIndex == 0 && i == 5) ||\n                 scroll == 3 * MAIN_HELP_VISIBLE_LINES)\n                    ? 0xffffffffU : helpColor888(bodyTone));"
# HELP-234: newer neutral-white body on pages 2/3 as well as page 4.
# Reverse it before the frozen PAGE4 and earlier SDL history rewinds.
_POST_PAGE234_COMMENT = """            /* HELP pages 2-4 use the same neutral white body ink,
             * independent of theme. Pages 12-16 retain the light theme tone.
             * Section titles and HELP / CYD remain theme-colored. */"""
_POST_PAGE234_COLOR = "                ((menuSystem->scrollIndex == 0 && i == 5) ||\\n                 (scroll >= MAIN_HELP_VISIBLE_LINES &&\\n                  scroll <= 3 * MAIN_HELP_VISIBLE_LINES))\\n                    ? 0xffffffffU : helpColor888(bodyTone));"



def restore_pre_help_page4_model(source: str) -> str:
    assert source.count(_POST_PAGE4_MODEL) == 1, "HELP page4 text source drift"
    return source.replace(_POST_PAGE4_MODEL, _PRE_PAGE4_MODEL, 1)


def restore_pre_help_page4_actions(source: str) -> str:
    assert source.count(_POST_PAGE234_COMMENT) == 1, "HELP page2/3 comment drift"
    assert source.count(_POST_PAGE234_COLOR) == 1, "HELP page2/3 white ink drift"
    source = source.replace(_POST_PAGE234_COMMENT, _POST_PAGE4_COMMENT, 1)
    source = source.replace(_POST_PAGE234_COLOR, _POST_PAGE4_COLOR, 1)
    assert source.count(_POST_PAGE4_COMMENT) == 1, "HELP page4 comment drift"
    assert source.count(_POST_PAGE4_COLOR) == 1, "HELP page4 white ink drift"
    return source.replace(_POST_PAGE4_COMMENT, _PRE_PAGE4_COMMENT, 1).replace(
        _POST_PAGE4_COLOR, _PRE_PAGE4_COLOR, 1)
