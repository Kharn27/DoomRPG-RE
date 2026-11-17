#!/usr/bin/env python3
"""Exact post-F29 title-music delta reversal; frozen SDL Git blobs unchanged."""

def restore_pre_menu_audio_actions(text: str) -> str:
    include = '#include "esp_audio_entrance_music.h" /* MENU-M0: stop title before action */\n'
    dispatch = (
        "    /* Only confirmed START leaves the title immediately. HELP/OPTIONS\n"
        "     * and the LOAD slot picker are still the same music session. */\n"
        "    if (action == DOOMRPG_ESP32_MAIN_MENU_ACTION_START)\n"
        "        EspEntranceMusic_stopMenuBeforeAction();\n\n"
    )
    valid_load = (
        "    /* A valid slot is now selected. Stop the single SD title owner only\n"
        "     * here, before checkpoint load and map/flash ownership. */\n"
        "    EspEntranceMusic_stopMenuBeforeAction();\n"
    )
    for delta in (include, dispatch, valid_load):
        assert text.count(delta) == 1, ("MENU-M1 action drift", delta)
        text = text.replace(delta, "", 1)
    return text


def restore_pre_menu_audio_resident(text: str) -> str:
    """Undo only M1's post-teardown pre-title sound hook in resident owner."""
    include = '#include "esp_audio_entrance_music.h" /* title prestart before menu present */\n'
    hook = (
        "    /* MENU-M1: after gameplay teardown, start the fresh title visit before\n"
        "     * presenting the first main-menu frame, just like cold boot. */\n"
        "    (void)EspEntranceMusic_prestartMenuBeforeFirstFrame();\n"
    )
    assert text.count(include) == 1, "MENU-M1 resident include drift"
    assert text.count(hook) == 1, "MENU-M1 resident handoff drift"
    return text.replace(include, "", 1).replace(hook, "", 1)
