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


def restore_sound_options_back(text: str) -> str:
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
