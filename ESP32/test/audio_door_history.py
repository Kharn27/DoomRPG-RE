"""AUDIO-DOOR strict inverse for immutable SDL historical source SHA oracles.

Only reconstructs pre-AUDIO-DOOR bytes IN MEMORY for historical fixtures.
The firmware source, original SHA witnesses and historical checks never change.
"""

_SELECT_POST_COMMIT = "        /* All selective rollback paths (secret, frame, CHANGEMAP door) are\n         * closed. Publish exactly one original SFX per successfully changed\n         * line, never during an optimistic apply or failed animation frame. */\n        {\n            uint8_t doorIndex;\n            for (doorIndex = 0U; doorIndex < result.doorCount; ++doorIndex) {\n                const EspNativeGameplayActionDoorStep* step =\n                    &result.doors[doorIndex];\n                (void)EspNativeDoorSfx_publishCommitted(\n                    step->lineIndex, step->openBefore, step->openAfter,\n                    step->soundId, step->effectFlags);\n            }\n        }\n\n"

def restore_pre_audio_door_resident(source):
    binary = isinstance(source, bytes)
    if binary:
        source = source.decode("utf-8")

    def undo(new, old, label):
        nonlocal source
        assert source.count(new) == 1, (
            "AUDIO-DOOR historical exact inverse drift", label, source.count(new))
        source = source.replace(new, old, 1)

    undo('#include "esp_native_door_sfx.h"\n', "", "include")
    undo(
        "        /* Stage only accepted 0->1 SELECT cues. The actual audio publication\n"
        "         * waits for the FIRST successfully presented animation image, not the\n"
        "         * full four-frame settle; no early cue on failed first frame. */\n"
        "        EspNativeDoorSfx_stageSelectOpenings(&result);\n",
        "", "select-stage")
    undo(
        '                                 "SELECT-DOOR")) {\n'
        "            EspNativeDoorSfx_cancelSelect();\n",
        '                                 "SELECT-DOOR")) {\n',
        "select-render-failure")
    undo(
        "                EspNativeDoorSfx_cancelSelect();\n"
        "                const int rolledBack =",
        "                const int rolledBack =",
        "select-changemap-failure")
    undo(
        "            EspNativeDoorSfx_cancelSelect();\n"
        '            disableGameplay("select-door-monster-turn-request");',
        '            disableGameplay("select-door-monster-turn-request");',
        "select-turn-failure")
    undo(
        "\n        /* Keep previous late commit for CLOSE and unanimated opens, but do\n"
        "         * not repeat an OPEN already emitted at its first visual frame. */\n"
        "        EspNativeDoorSfx_finishSelect(&result);\n\n",
        "", "select-final")
    return source.encode("utf-8") if binary else source


def restore_pre_audio_door_dynamic(source):
    """Restore exact old dynamic-door C for frozen SDL-F28 SHA test only."""
    binary = isinstance(source, bytes)
    if binary:
        source = source.decode("utf-8")
    imports = (
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
        '#include "esp_native_door_sfx.h"\n'
        '#include "esp_audio_entrance_music.h"\n'
        "#endif\n"
    )
    # The post-AUDIO-DOOR hook now also admits held MOVE-close 5064
    # after its first physically presented frame. Historical SDL-F28
    # reconstruction still removes this one exact, reviewed hook and
    # verifies the original whole source against the unchanged Git SHA.
    first_visual = (
        "\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
        "            /*\n"
        "             * Both door cues follow physical presentation, with the PAK\n"
        "             * lease closed. OPEN 5063 is emitted after its first opening\n"
        "             * frame; held MOVE-close 5064 is staged at MOVE commit and\n"
        "             * emitted only after the first visible closing frame FOLLOWING\n"
        "             * the monster turn. Other closes keep their commit timing.\n"
        "             * This hook never runs on failed/held/unpresented frames.\n"
        "             */\n"
        "            if (animationFrame.ordinal == 1U) {\n"
        "                const uint8_t opened =\n"
        "                    EspNativeDoorSfx_onFirstPresentedOpeningFrame();\n"
        "                const uint8_t closed =\n"
        "                    EspNativeDoorSfx_onFirstPresentedHeldClosingFrame();\n"
        "                if (opened != 0U || closed != 0U) {\n"
        "                    EspEntranceMusic_pumpGameplay();\n"
        "                }\n"
        "            }\n"
        "#endif\n"
    )
    for exact, label in (
        (imports, "bounded imports"),
        (first_visual, "first-frame SFX hook"),
    ):
        assert source.count(exact) == 1, ("AUDIO-DOOR dynamic inverse drift", label, source.count(exact))
        source = source.replace(exact, "", 1)
    return source.encode("utf-8") if binary else source
