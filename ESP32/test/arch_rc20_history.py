"""ARCH-RC20 strict inverse of direct native topology read ownership.

Only frozen historical Git-blob parity tests use this transformation.
The ESP32 firmware compiles the new Base/public getters and never resurrects
GNU __wrap or __real symbols. Every reverse edit has exact multiplicity.
"""


def restore_pre_rc20(filename, source):
    binary = isinstance(source, bytes)
    text = source.decode("utf-8") if binary else source
    if filename == "esp_native_gameplay_action_engine.c":
        edits = (
            ("EspMapSpriteTopology_getVisualStateBase(",
             "__real_EspMapSpriteTopology_getVisualState(", 2),
            ("EspMapSpriteTopology_getEntityBase(",
             "__real_EspMapSpriteTopology_getEntity(", 2),
            ("int EspNativeGameplayActionEngine_getVisualState(",
             "int __wrap_EspMapSpriteTopology_getVisualState(", 1),
            ("int EspNativeGameplayActionEngine_getEntity(",
             "int __wrap_EspMapSpriteTopology_getEntity(", 1),
        )
    elif filename == "esp_native_gameplay_monster_combat.c":
        edits = (
            ("/* RC20: public composed topology projection is explicitly owned here.\n"
             " * ActionEngine and raw map topology remain private read layers. */\n",
             "/* Explicit MonsterCombat SELECT/service/reset owner; the sprite topology\n"
             " * linker wrappers remain unchanged in RC13. */\n"
             "#undef __wrap_EspMapSpriteTopology_getEntity\n"
             "#undef __wrap_EspMapSpriteTopology_getVisualState\n", 1),
            ("int EspMapSpriteTopology_getVisualState(",
             "int __wrap_EspMapSpriteTopology_getVisualState(", 1),
            ("int EspMapSpriteTopology_getEntity(",
             "int __wrap_EspMapSpriteTopology_getEntity(", 1),
        )
    else:
        return source
    for new, old, count in edits:
        actual = text.count(new)
        assert actual == count, (
            "ARCH-RC20 frozen source drift", filename, new, actual, count
        )
        text = text.replace(new, old)
    return text.encode("utf-8") if binary else text
