"""Reconstruct immutable pre-audio menu C bodies for historical SDL-F19/F29.

These are strictly checked inverse transforms of the AUDIO-R2c SOUND-only
additions. Frozen old Git blob fingerprints remain unchanged. A drift in a
modern menu edit must FAIL CLOSED instead of silently changing the oracle.
"""

def _replace_once(text: str, new: str, old: str) -> str:
    assert text.count(new) == 1, ("AUDIO-R2c SOUND delta drift", new)
    return text.replace(new, old, 1)


def restore_sound_options_action(text: str) -> str:
    pairs = (
        ("#define OPTIONS_ENABLED_MASK ((1U << OPTIONS_BACK_ITEM) | (1U << OPTIONS_SOUND_ITEM))",
         "#define OPTIONS_ENABLED_MASK (1U << OPTIONS_BACK_ITEM)"),
        (" * VIDEO/LUMINOSITE/ORIENTATION/THEME and SOUND volume are native UI sublayers,",
         " * VIDEO/LUMINOSITE/ORIENTATION/THEME are compact native UI sublayers,"),
        ("selectedItem < OPTIONS_BACK_ITEM || selectedItem > OPTIONS_SOUND_ITEM)",
         "selectedItem < OPTIONS_BACK_ITEM || selectedItem > OPTIONS_VIDEO_ITEM)"),
        ('i == OPTIONS_VIDEO_ITEM || i == OPTIONS_SOUND_ITEM) ? "yes" : "no",',
         'i == OPTIONS_VIDEO_ITEM) ? "yes" : "no",'),
        ("videoChild=BRIGHTNESS/ORIENTATION/THEME soundChild=VOLUME-SESSION brightness=%u",
         "videoChild=BRIGHTNESS/ORIENTATION/THEME brightness=%u"),
    )
    for new, old in pairs:
        text = _replace_once(text, new, old)
    return text


def _undo_sound_s1(text: str) -> str:
    """Strictly undo only S1 nested SOUND UI to preserve the frozen F29 blob.

    Restore S1 -> audio-R2c, then the existing R2c inverse restores pre-F29.
    No historical checksum is changed or exempted.
    """
    if "static uint8_t soundMenuOpen = 0U;" not in text:
        return text
    pairs = (
        ("static uint8_t soundMenuOpen = 0U; /* BACK / ON-OFF / VOLUME */\n"
         "static uint8_t soundOpen = 0U; /* existing 5-card volume child */",
         "static uint8_t soundOpen = 0U; /* session-only volume child */"),
        ("                      (soundMenuOpen ? DOOMRPG_ESP32_OPTIONS_CARD_COUNT :\n", ""),
        ("(videoOpen ? DOOMRPG_ESP32_VIDEO_CARD_COUNT : OPTIONS_ITEM_COUNT))));",
         "(videoOpen ? DOOMRPG_ESP32_VIDEO_CARD_COUNT : OPTIONS_ITEM_COUNT)));"),
        ("        else if (soundMenuOpen)\n"
         "            DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);\n", ""),
    )
    for new, old in pairs:
        text = _replace_once(text, new, old)
    # The new SOUND painter + shared present wrapper are wholly outside the
    # old audio-R2c paintSoundState; remove only that contiguous new block.
    begin = "/* SOUND presentation belongs to the mutable native sublayer controller,"
    end = "static int paintSoundState(int armedChoice) {"
    assert text.count(begin) == text.count(end) == 1
    a, b = text.index(begin), text.index(end)
    assert a < b and "presentSoundUiFrame" in text[a:b]
    text = text[:a] + text[b:]
    pairs = (
        ("    presentSoundUiFrame();", "    (void)Esp32PlatformVideo_present();"),
        ("        orientationOpen != 0U || themeOpen != 0U ||\n"
         "        soundMenuOpen != 0U || soundOpen != 0U ||",
         "        orientationOpen != 0U || themeOpen != 0U || soundOpen != 0U ||"),
        ("           (soundMenuOpen ? optionsItemAt(screenX, screenY) :\n", ""),
        ("                        optionsItemAt(screenX, screenY)))));",
         "                        optionsItemAt(screenX, screenY))));"),
        ('           luminositeOpen ? "luminosite" : (soundOpen ? "sound-volume" :\n'
         '               (soundMenuOpen ? "sound-menu" : (orientationOpen ? "orientation" :\n'
         '               (themeOpen ? "theme" : (videoOpen ? "video" : "options"))))),',
         '           luminositeOpen ? "luminosite" : (orientationOpen ? "orientation" :\n'
         '               (themeOpen ? "theme" : (videoOpen ? "video" : "options"))),'),
        ('            if (!paintSoundMenuState(0, 0)) {\n'
         '                optionsBackActive=0;\n'
         '                PlatformInput_setTapCallback(NULL);\n'
         '                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "sound-volume-back-failed");',
         '            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {\n'
         '                optionsBackActive=0;\n'
         '                PlatformInput_setTapCallback(NULL);\n'
         '                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "sound-back-failed");'),
        ("[OPTIONSSOUND] VOLUME-BACK to=SOUND percent=%u persistence=session",
         "[OPTIONSSOUND] BACK percent=%u persistence=session"),
        ("            if (!paintSoundMenuState(0, 0)) {\n"
         "                (void)EspNativeAudioIntent_setMusicVolumePercent(before);",
         "            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {\n"
         "                (void)EspNativeAudioIntent_setMusicVolumePercent(before);"),
        ("autoBack=SOUND persistence=session",
         "autoBack=Options persistence=session"),
    )
    for new, old in pairs:
        text = _replace_once(text, new, old)
    # The intermediate tap branch is new and adjacent to the existing
    # luminosite child. Exactly one extra separating newline is removed.
    begin = "    /* SOUND intermediate: same 2 released taps as Options and VIDEO."
    end = "    if (luminositeOpen) {"
    assert text.count(begin) == text.count(end) == 1
    a, b = text.index(begin), text.index(end)
    assert a < b and "EspNativeAudioIntent_setSoundEnabled" in text[a:b]
    assert a > 0 and text[a-1] == "\n"
    text = text[:a-1] + text[b:]
    pairs = (
        ("    /* SOUND is a native ON/OFF + VOLUME intermediate child. */",
         "    /* SOUND is a native session-volume child. */"),
        ('    if (item == OPTIONS_SOUND_ITEM) {\n'
         '        if (!paintSoundMenuState(0,0)) {',
         '    if (item == OPTIONS_SOUND_ITEM) {\n'
         '        if (!paintSoundState(-1)) {'),
        ("        soundMenuOpen=1U;\n#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY",
         "        soundOpen=1U;\n#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY"),
        ('        printf("[OPTIONSSOUND] OPEN from=Options cards=BACK/ONOFF/VOLUME enabled=%d "\n'
         '               "persistence=session confirmTap=2\\n",\n'
         '               EspNativeAudioIntent_isSoundEnabled());',
         '        printf("[OPTIONSSOUND] OPEN from=Options levels=25/50/75/100 "\n'
         '               "persistence=session confirmTap=2\\n");'),
        ("    soundMenuOpen = 0U;\n    soundOpen = 0U;",
         "    soundOpen = 0U;"),
        ("SOUND=ONOFF/VOLUME-submenu", "SOUND=session-volume-5-cards"),
    )
    for new, old in pairs:
        text = _replace_once(text, new, old)
    assert "soundMenuOpen" not in text
    assert "presentSoundUiFrame" not in text
    return text


def restore_sound_options_back(text: str) -> str:
    text = _undo_sound_s1(text)
    text = _replace_once(text, '#include "esp_native_audio_intent.h"\n', "")
    text = _replace_once(text, '#define OPTIONS_SOUND_ITEM 2\n', "")
    text = _replace_once(text,
        'static uint8_t soundOpen = 0U; /* session-only volume child */\n', "")
    text = _replace_once(text, "if (luminositeOpen || soundOpen)",
                         "if (luminositeOpen)")
    assert text.count("(luminositeOpen || soundOpen)") == 2
    text = text.replace("(luminositeOpen || soundOpen)", "luminositeOpen")
    text = _replace_once(text,
        "orientationOpen != 0U || themeOpen != 0U || soundOpen != 0U ||",
        "orientationOpen != 0U || themeOpen != 0U ||")
    # Strip ONLY the entire new UI function (boundary to next old function).
    begin = "static int paintSoundState(int armedChoice) {"
    end = "static int paintOrientationState(void) {"
    assert text.count(begin) == text.count(end) == 1
    a, b = text.index(begin), text.index(end)
    assert a < b and "[OPTIONSSOUND] VISUAL" in text[a:b]
    text = text[:a] + text[b:]
    # Strip ONLY the Sound-specific tap branch, immediately ahead of
    # the original luminosite branch. No unrelated nested pages altered.
    begin = "    if (soundOpen) {"
    end = "    if (luminositeOpen) {"
    assert text.count(begin) == text.count(end) == 1
    a, b = text.index(begin), text.index(end)
    assert a < b and "EspNativeAudioIntent_setMusicVolumePercent(after)" in text[a:b]
    text = text[:a] + text[b:]
    text = _replace_once(text,
        "    /* SOUND is a native session-volume child. */\n"
        "    if (item != OPTIONS_BACK_ITEM && item != OPTIONS_VIDEO_ITEM &&\n"
        "        item != OPTIONS_SOUND_ITEM) {",
        "    /* OPTIONS root has only BACK/VIDEO. SOUND is intentionally deferred. */\n"
        "    if (item != OPTIONS_BACK_ITEM && item != OPTIONS_VIDEO_ITEM) {")
    begin = "    if (item == OPTIONS_SOUND_ITEM) {"
    end = '    printf("[OPTIONBACK] CONFIRM Back action=native-semantic-return'
    assert text.count(begin) == text.count(end) == 1
    a, b = text.index(begin), text.index(end)
    assert a < b and 'printf("[OPTIONSSOUND] OPEN' in text[a:b]
    text = text[:a] + text[b:]
    text = _replace_once(text,
        "    soundOpen = 0U;\n    optionsTapCount", "    optionsTapCount")
    text = _replace_once(text,
        "VIDEO=intermediate-4-cards SOUND=session-volume-5-cards fastOpaqueReturn=yes",
        "VIDEO=intermediate-4-cards SOUND=deferred fastOpaqueReturn=yes")
    return text
