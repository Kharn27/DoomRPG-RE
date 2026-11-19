"""Strict inverse of ARCH-RC21 code changes for frozen historical Git-blob tests.

This never runs in firmware and must not resurrect any GNU __real or __wrap
interposition in production. Each historical test explicitly invokes this only
to compare its pre-milestone source against a pinned legacy Git blob SHA.
"""


def restore_pre_rc21(filename, original):
    binary = isinstance(original, bytes)
    text = original.decode("utf-8") if binary else original

    if filename == "esp_native_bsp_visibility.c":
        edits = (
            ("int EspNativeBspVisibility_mapSpriteVisibleBase(",
             "int EspNativeBspVisibility_mapSpriteVisible(", 1),
        )
    elif filename == "esp_map_access.c":
        edits = (
            ("int EspMapRuntime_getMapSpriteBase(",
             "int EspMapRuntime_getMapSprite(", 1),
        )
    elif filename == "esp_native_gameplay_sprite_overlay.c":
        edits = (
            ("int EspMapRuntime_getMapSprite(uint32_t index,",
             "int __wrap_EspMapRuntime_getMapSprite(uint32_t index,", 1),
            ("if (!EspMapRuntime_getMapSpriteBase(index, outSprite)) return 0;",
             "if (!__real_EspMapRuntime_getMapSprite(index, outSprite)) return 0;", 1),
        )
    elif filename == "esp_native_gameplay_monster_active_sequence.c":
        edits = (
            ("int EspNativeBspVisibility_mapSpriteVisible(",
             "int __wrap_EspNativeBspVisibility_mapSpriteVisible(", 1),
            ("int visible = EspNativeBspVisibility_mapSpriteVisibleBase(",
             "int visible = __real_EspNativeBspVisibility_mapSpriteVisible(", 1),
            ("!EspMapRuntime_getMapSpriteBase(mapSpriteIndex, &sprite) ||",
             "!__real_EspMapRuntime_getMapSprite(mapSpriteIndex, &sprite) ||", 1),
        )
    elif filename == "esp_native_gameplay_action_engine.c":
        edits = (
            ("EspMapRuntime_getMapSpriteBase(",
             "__real_EspMapRuntime_getMapSprite(", 8),
        )
    elif filename == "esp_native_gameplay_pickup.c":
        edits = (
            ("EspMapRuntime_getMapSpriteBase(",
             "__real_EspMapRuntime_getMapSprite(", 2),
        )
    elif filename == "esp_native_gameplay_player_resources.c":
        edits = (
            ("EspMapRuntime_getMapSpriteBase(",
             "__real_EspMapRuntime_getMapSprite(", 3),
        )
    else:
        return original

    for newer, old, expected in edits:
        count = text.count(newer)
        assert count == expected, (
            "ARCH-RC21 frozen source drift", filename, newer, count, expected
        )
        text = text.replace(newer, old)

    if filename == "esp_native_gameplay_sprite_overlay.c":
        signature = ("int __wrap_EspMapRuntime_getMapSprite(uint32_t index,\n"
                     "                                      EspMapSprite* outSprite) {")
        assert text.count(signature) == 1
        text = text.replace(
            signature,
            "int __real_EspMapRuntime_getMapSprite(uint32_t index,\n"
            "                                      EspMapSprite* outSprite);\n\n" + signature, 1)
    elif filename == "esp_native_gameplay_monster_active_sequence.c":
        signature = "static uint16_t read16le(const uint8_t* bytes) {"
        assert text.count(signature) == 1
        text = text.replace(
            signature,
            "int __real_EspNativeBspVisibility_mapSpriteVisible(\n"
            "    const EspNativeBspVisibilityState* state,\n"
            "    uint32_t mapSpriteIndex,\n"
            "    uint32_t* outLeafIndex);\n"
            "int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);\n\n"
            + signature, 1)
    return text.encode("utf-8") if binary else text
