"""Exact inverse of AUDIO-PICKUP #419 for immutable SDL-F27 Git blob tests.

Do not rewrite golden hashes or relax the historic test. Undo ONLY this
post-SDL new handoff and recover the pre-#419 resource source byte-for-byte.
"""
def restore_pre_audio_pickup_modal_resources(source):
    binary = isinstance(source, bytes)
    current = source.decode("utf-8") if binary else source
    include = '#include "esp_audio_entrance_music.h"\n'
    assert current.count(include) == 1, "AUDIO-PICKUP #419 include drift"
    current = current.replace(include, "", 1)

    begin = ("        if (firstWeaponDialog != NULL) {\n"
             "            EspNativeGameplayDialogBeginStatus dialogStatus =")
    after = '        DRPG_LOGD("[PLAYERRES] FEEDBACK'
    commit = '        DRPG_LOGD("[PLAYERRES] COMMIT'
    publish = ("        const int pickupCuePublished="
               "EspNativeAudioIntent_publish(5060U,0U,3U);\n")
    assert current.count(begin) == 1 and current.count(after) == 1
    assert current.count(commit) == 1 and current.count(publish) == 1
    start = current.index(begin)
    end = current.index(after, start)
    dialog = current[start:end]
    assert dialog.count(
        "EspNativeGameplayDialog_beginStandalone(firstWeaponDialog);"
    ) == 1
    assert dialog.endswith("        }\n"), "AUDIO-PICKUP dialog span drift"

    pickup_begin = current.index(publish)
    assert pickup_begin < start, "AUDIO-PICKUP publish/modal order drift"
    new_intro = (
        publish +
        "        /* Resource/world mutation is now irreversible: the synchronous\n"
        "         * redraw succeeded, and NO rollback path remains below this point.\n"
        "         * Give the audio owner one bounded chance to mix the cue while the\n"
        "         * original music ring and PAK source are still free. For normal\n"
        "         * pickups the ordinary cooperative audio service is unchanged. */\n"
        "        if (firstWeaponDialog != NULL && pickupCuePublished)\n"
        "            (void)EspEntranceMusic_handoffPickupBeforeModal(\n"
        "                EspNativeAudioIntent_count());\n"
        "        /* The standalone UI takes and retains the PAK lease only AFTER\n"
        "         * the pickup intent was published and the audio handoff attempted. */\n"
    )
    assert current[pickup_begin:start] == new_intro, (
        "AUDIO-PICKUP #419 non-modal hook drift")
    current = (
        current[:pickup_begin] +
        "        (void)EspNativeAudioIntent_publish(5060U,0U,3U);\n" +
        current[end:]
    )
    assert current.count(commit) == 1
    current = current.replace(commit, dialog + commit, 1)
    return current.encode("utf-8") if binary else current
