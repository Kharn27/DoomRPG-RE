#!/usr/bin/env python3
"""Remove ONLY the later UAC-120 menu pulse from frozen source-parity oracles.

The production source retains its idle animation; historical golden hashes
must still refer to the same pre-animation bytes without updating the SHA.
"""
INCLUDE = '#include "esp_uac_menu_preview.h"\n'
HOOK = (
    '    /* Already serviced each game-loop turn, including MENU_MAIN/OPTIONS.\n'
    '     * Repaint only the selected native-120 physical label when its phase\n'
    '     * changes; do not touch framebuffer, input state or gameplay scenes. */\n'
    '    if (EspUacMenuPreview_needsBlink())\n'
    '        (void)Esp32PlatformVideo_present();\n'
)

def restore_pre_uac_blink_actions(source: str) -> str:
    assert source.count(INCLUDE) == 1, "UAC pulse include changed"
    assert source.count(HOOK) == 1, "UAC pulse service changed"
    return source.replace(INCLUDE, "", 1).replace(HOOK, "", 1)
