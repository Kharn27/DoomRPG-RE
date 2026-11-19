"""Strict inverse of ARCH-RC18's two public symbol-owner handoffs.

Only for immutable historical Git SHA oracles. This never runs on firmware,
and does not relax any expected historical blob SHA.
"""

def restore_pre_uac_p1_menu_touch(source: str) -> str:
    """Strict UAC-P1 inverse before frozen SDL-F29/RC18 blob comparisons.

    This removes ONLY our independent preview path in the host oracle.
    Firmware continues to compile the unchanged new UAC path.
    """
    def undo(new: str, old: str) -> None:
        nonlocal source
        assert source.count(new) == 1, ("UAC-P1 menu delta drift", new[:90])
        source = source.replace(new, old, 1)

    undo(
        '#include "native_main_menu_touch_layout.h"\n'
        '#include "esp_uac_menu_preview.h"\n'
        '#include "esp_uac_bmp32.h"\n',
        '#include "native_main_menu_touch_layout.h"\n'
    )
    undo(
        '/* Query ONLY the proven main-menu selection owner, not an independent index. */\n'
        'int EspUacMenuPreview_selectedIndex(void) {\n'
        '    if (!touchActive || EspNativeRootOwners_live()->menuSystem == NULL ||\n'
        '        EspNativeRootOwners_live()->menuSystem->menu != MENU_MAIN) return -1;\n'
        '    return EspNativeRootOwners_live()->menuSystem->selectedIndex;\n'
        '}\n\n',
        ''
    )
    undo(
        '    /* A transition or LOAD modal must never inherit the physical SD overlay. */\n'
        '    EspUacMenuPreview_reset();\n\n',
        ''
    )
    start = '    /* The original logo was a no-op tap area.'
    stop = '\n    if (hit != gateSelectedItem) {'
    assert source.count(start) == 1 and source.count(stop) == 1
    a = source.index(start)
    b = source.index(stop, a)
    source = source[:a] + (
        '    hit = gateHitItem(screenX, screenY);\n\n'
        '    if (hit < 0) {\n'
        '        lastTappedItem = -1;\n'
        '        downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);\n'
        '        return;\n'
        '    }\n'
    ) + source[b:]
    undo(
        '        /* Feed the EXISTING selection/paint/hash owner a point in its\n'
        '         * matching old 54x24 card. Original physical touch is logged above.\n'
        '         * Never fork the MENU_MAIN model, action or double-tap contract. */\n'
        '        if (EspUacMenuPreview_mode() != 0)\n'
        '            downstreamTapCallback(260, (int16_t)(40 + 54 * hit),\n'
        '                                  pressure, rawX, rawY);\n'
        '        else\n'
        '            downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);\n',
        '        downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);\n'
    )
    undo(
        '    else {\n'
        '        EspUacMenuPreview_reset();\n'
        '        downstreamTapCallback = callback;\n'
        '        PlatformInput_setTapCallbackBase(callback);\n'
        '    }\n}',
        '    else {\n'
        '        downstreamTapCallback = callback;\n'
        '        PlatformInput_setTapCallbackBase(callback);\n'
        '    }\n}'
    )
    assert 'EspUacMenuPreview' not in source and 'EspUacMenu_hitPhysical' not in source
    return source


def restore_pre_rc18_menu_touch(source: str) -> str:
    source = restore_pre_uac_p1_menu_touch(source)
    new = (
        "/* ARCH-RC18: explicit native facade for menu-gated registration.\n"
        " * Generic sampling remains in PlatformInput_setTapCallbackBase(). */\n"
        "void PlatformInput_setTapCallback(PlatformTapCallback callback) {"
    )
    old = "void __wrap_PlatformInput_setTapCallback(PlatformTapCallback callback) {"
    assert source.count(new) == 1, "RC18 public callback facade drift"
    source = source.replace(new, old, 1)
    leaf = "PlatformInput_setTapCallbackBase("
    assert source.count(leaf) == 4, "RC18 PlatformInput raw C ABI drift"
    return source.replace(leaf, "__real_PlatformInput_setTapCallback(")
