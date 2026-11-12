"""Exact historical HELP BOTTOM HUD section from the pre-blue PR #332 head.

Only SDL-F17 and SDL-F19 Git-blob oracles use this. Their original expected
hashes are unchanged; just undo this *later* visual-only feature before
reversing the actual SDL migration. No runtime imports or backing-store reads.
"""

PRE_HELP_BOTTOM_BLUE = "/* Original k/m/l/o/a BMPs, exact gameplay HUD painter, no substitutes.\n * Stage y100..119 once before HELP footer plates overwrite those pixels,\n * and keep only a 128x16 RGB565 thumbnail for subsequent soft repaints. */\nstatic int helpPaintBottomHudIllustration(uint16_t* fb) {\n    const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());\n    const uint8_t power = EspNativeHelpScene_breathPower(\n        helpBreathLevel(step));\n    int x, y;\n    if (fb == NULL) return 0;\n    if (!helpBottomHudReady) {\n        if (!EspNativeGameplayHud_paintHelpBottomBand(fb)) {\n            printf(\"[MAINHELP] BOTTOM-HUD FAIL native k/m/l/o/a ownership\\n\");\n            return 0;\n        }\n        for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {\n            for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {\n                const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;\n                const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x] =\n                    fb[sy * DOOMRPG_LOGICAL_WIDTH + sx];\n            }\n        }\n        helpBottomHudReady = 1U;\n    }\n    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y)\n        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x)\n            fb[(MAIN_HELP_HUD_BAR_Y + y) * DOOMRPG_LOGICAL_WIDTH +\n               MAIN_HELP_HUD_BAR_X + x] =\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x];\n\n    /* Scale the ACTUAL weapon hitbox x100..159 to x96..143 of this\n     * 128px miniature. Highlight the same four 3px touch corners as the\n     * canonical gameplay HUD (not arbitrary full-height blue dividers).\n     * x97/x142 and y44..46/y55..57; no framebuffer region is tinted. */\n    {\n        const int buttonLeft = MAIN_HELP_HUD_BAR_X +\n            ESP_NATIVE_HUD_WEAPON_LEFT * MAIN_HELP_HUD_BAR_WIDTH /\n            DOOMRPG_LOGICAL_WIDTH;\n        const int buttonRight = MAIN_HELP_HUD_BAR_X +\n            (ESP_NATIVE_HUD_WEAPON_RIGHT + 1) *\n            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH - 1;\n        const uint16_t amber = (uint16_t)(\n            (((31U * (uint32_t)power) / 255U) << 11) |\n            (((40U * (uint32_t)power) / 255U) << 5));\n        int edge, corner;\n        if (power > 0U) {\n            for (edge = 0; edge < 2; ++edge) {\n                const int buttonX = edge == 0\n                    ? buttonLeft + 1 : buttonRight - 1;\n                for (corner = 0; corner < 2; ++corner) {\n                    const int firstY = MAIN_HELP_HUD_BAR_Y +\n                        (corner == 0 ? 1 : MAIN_HELP_HUD_BAR_HEIGHT - 4);\n                    for (y = firstY; y < firstY + 3; ++y)\n                        fb[y * DOOMRPG_LOGICAL_WIDTH + buttonX] = amber;\n                }\n            }\n        }\n    }\n    helpHudBreathStep = step;\n    return 1;\n}\n\n"

def restore_pre_help_bottom_blue(source: str) -> str:
    """Undo the 2026-11-13 full-blue-breath HELP enhancement exactly."""
    new_begin = "/* Preserve the light/dark texture"
    end = "/* Build the exact native 4x3 WEAPONS panel"
    assert source.count(new_begin) == 1
    assert source.count(end) == 1
    first = source.index(new_begin)
    after = source.index(end, first)
    changed = source[first:after]
    assert "static uint16_t helpTintBottomHudWeaponBlue(" in changed
    assert "helpTintBottomHudWeaponBlue(*pixel, power)" in changed
    assert "const uint16_t amber" not in changed
    return source[:first] + PRE_HELP_BOTTOM_BLUE + source[after:]
