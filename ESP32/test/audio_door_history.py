"""AUDIO-DOOR strict inverse for immutable SDL historical source SHA oracles.

Only reconstructs pre-AUDIO-DOOR bytes IN MEMORY for historical fixtures.
The firmware source, original SHA witnesses and historical checks never change.
"""

_SELECT_POST_COMMIT = "        /* All selective rollback paths (secret, frame, CHANGEMAP door) are\n         * closed. Publish exactly one original SFX per successfully changed\n         * line, never during an optimistic apply or failed animation frame. */\n        {\n            uint8_t doorIndex;\n            for (doorIndex = 0U; doorIndex < result.doorCount; ++doorIndex) {\n                const EspNativeGameplayActionDoorStep* step =\n                    &result.doors[doorIndex];\n                (void)EspNativeDoorSfx_publishCommitted(\n                    step->lineIndex, step->openBefore, step->openAfter,\n                    step->soundId, step->effectFlags);\n            }\n        }\n\n"

def restore_pre_audio_door_resident(source):
    binary = isinstance(source, bytes)
    if binary:
        source = source.decode("utf-8")
    include = '#include "esp_native_door_sfx.h"\n'
    assert source.count(include) == 1, ("AUDIO-DOOR history include drift", source.count(include))
    assert source.count(_SELECT_POST_COMMIT) == 1, (
        "AUDIO-DOOR history post-commit span drift", source.count(_SELECT_POST_COMMIT))
    source = source.replace(include, "", 1)
    source = source.replace(_SELECT_POST_COMMIT, "", 1)
    return source.encode("utf-8") if binary else source
