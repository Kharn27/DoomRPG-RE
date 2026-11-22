"""Strict inverse of UAC Options physical-only compositor for frozen SDL oracles.

Exactly eleven intentional deltas to native_main_menu_options_back.c and
two to options_action.c. Never weaken original historical Git blob SHA.
"""


def _replace_one(source: str, new: str, old: str) -> str:
    assert source.count(new) == 1, ("UAC OPTIONS source drift", new[:100])
    return source.replace(new, old, 1)


def restore_pre_uac_options_action(source: str) -> str:
    source = _replace_one(
        source, '#include "esp_uac_menu_preview.h"\n', '')
    source = _replace_one(
        source,
        '    /* Prepare the *physical* UAC submenus only after every canonical\n'
        '     * model/FNV/heap invariant has passed; old framebuffer stays intact. */\n'
        '    EspUacMenuPreview_armOptions();\n',
        '')
    return source


def restore_pre_uac_options_back(source: str) -> str:
    # Strip only the new native-120 presentation shortcut before the
    # original immutable SDL-F29/F19/SOUND history reconstruction.
    source = _replace_one(source,
        '    int item;\n    const int uacInstant=EspUacMenuPreview_optionsActive();\n',
        '    int item;\n')
    source = _replace_one(source,
        '    /* Native UAC volume has four physical preset slots, no BACK.\n'
        '     * Map only those taps to the preexisting semantic preset IDs 1..4. */\n'
        '    if(soundOpen && uacInstant && item>=0)++item;\n', '')
    guard='if (armedItem != item && !uacInstant) {'
    assert source.count(guard)==2, 'UAC percentage-only instant guard drift'
    source=source.replace(guard,'if (armedItem != item) {')
    # Keep the historical test's original logging exact; production
    # distinguishes a native-UAC percentage tap from procedural two taps.
    source = _replace_one(source,
        '            printf("[OPTIONSBRIGHTNESS] SELECT %u->%u changed=%s confirmation=%s autoBack=VIDEO persistence=sd-v3\\n",\n'
        '                   (unsigned)before, (unsigned)after,\n'
        '                   before == after ? "no" : "yes",\n'
        '                   uacInstant ? "single-tap" : "two-tap");\n',
        '            printf("[OPTIONSBRIGHTNESS] SELECT %u->%u changed=%s confirmation=two-tap autoBack=VIDEO persistence=sd-v3\\n",\n'
        '                   (unsigned)before, (unsigned)after,\n'
        '                   before == after ? "no" : "yes");\n')
    source = _replace_one(source,
        '            printf("[OPTIONSBRIGHTNESS] OPEN from=VIDEO percent=%u presets=25/50/75/100 confirmTap=%d autoBack=VIDEO\\n",\n'
        '                   (unsigned)Esp32Backlight_percent(),uacInstant?1:2);\n',
        '            printf("[OPTIONSBRIGHTNESS] OPEN from=VIDEO percent=%u presets=25/50/75/100 confirmTap=2 autoBack=VIDEO\\n",\n'
        '                   (unsigned)Esp32Backlight_percent());\n')
    source = _replace_one(source,
        '            printf("[OPTIONSSOUND] SELECT %u->%u changed=%s "\n'
        '                   "confirmation=%s autoBack=SOUND persistence=session\\n",\n'
        '                   (unsigned)before,(unsigned)after,\n'
        '                   before==after?"no":"yes",\n'
        '                   uacInstant?"single-tap":"two-tap");\n',
        '            printf("[OPTIONSSOUND] SELECT %u->%u changed=%s "\n'
        '                   "autoBack=SOUND persistence=session\\n",\n'
        '                   (unsigned)before,(unsigned)after,\n'
        '                   before==after?"no":"yes");\n')
    source = _replace_one(
        source, '#include "esp_uac_menu_preview.h"\n', '')
    new_lines = (
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_ROOT,selectedItem,armed,0x07U);\n',
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_VIDEO,selectedItem,armed,\n'
        '        (uint8_t)(0x09U | (Esp32Backlight_isReady()?0x02U:0U) |\n'
        '                  (Esp32Orientation_isReady()?0x04U:0U)));\n',
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_BRIGHTNESS,\n'
        '        (int)(Esp32Backlight_percent()/25U),1,0x1fU);\n',
        '    EspUacMenuPreview_optionsPage(EspNativeAudioIntent_isSoundEnabled()?\n'
        '        ESP_UAC_OPTIONS_SOUND_ON:ESP_UAC_OPTIONS_SOUND_OFF,selected,armed,0x07U);\n',
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_VOLUME,\n'
        '        armedChoice>=0?armedChoice:(int)(percent==0U?1U:percent/25U),\n'
        '        1,0x1fU);\n',
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_ORIENTATION,\n'
        '        (int)Esp32Orientation_get()+1,1,0x07U);\n',
        '    EspUacMenuPreview_optionsPage(ESP_UAC_OPTIONS_THEME,\n'
        '        (int)EspMainMenuSkin_getTheme()+1,1,0x0fU);\n',
        '    EspUacMenuPreview_optionsPage(layer==1?ESP_UAC_OPTIONS_BRIGHTNESS:\n'
        '        layer==2?ESP_UAC_OPTIONS_ORIENTATION:ESP_UAC_OPTIONS_THEME,\n'
        '        selectedItem,1,0x1fU);\n'
    )
    assert len(new_lines) == 8
    for block in new_lines:
        source = _replace_one(source, block, '')
    new_touch = (
        '    /* Match the visible 110px UAC plates exactly; do not permit taps on\n'
        '     * the invisible old logical cards hidden under the physical overlay. */\n'
        '    item = EspUacMenuPreview_optionsActive() ?\n'
        '           EspUacMenuPreview_optionsHit(screenX,screenY) :\n'
        '           ((luminositeOpen || soundOpen) ? luminositeItemAt(screenX, screenY) :\n'
    )
    old_touch = (
        '    item = (luminositeOpen || soundOpen) ? luminositeItemAt(screenX, screenY) :\n'
    )
    source = _replace_one(source, new_touch, old_touch)
    source = _replace_one(
        source,
        '                        optionsItemAt(screenX, screenY))))));\n',
        '                        optionsItemAt(screenX, screenY)))));\n')
    assert "EspUacMenuPreview_" not in source, "residual UAC delta"
    return source
