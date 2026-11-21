"""ARCH-RC22: exact inverse of two native facade renames for historical blobs.

ONLY the SDL-F27 immutable Git-blob oracle consumes this transformation.
It restores the pre-RC22 source text in memory, not in the firmware, and
fails on any unexpected delta rather than weakening the historical SHA.
"""


def replace_exact(data: bytes, newer: bytes, older: bytes, count: int = 1) -> bytes:
    assert data.count(newer) == count, (
        "RC22 inverse drift: expected " + str(count) +
        " exact instances of " + repr(newer[:90]))
    return data.replace(newer, older)


def restore_pre_rc22(filename: str, current: bytes) -> bytes:
    if filename == "esp_native_gameplay_pickup.c":
        current = replace_exact(
            current,
            b"int EspMapRuntime_getMapSpriteBase(uint32_t index,\n"
            b"                                      EspMapSprite* outSprite);\n"
            b"static uint16_t tileForView(",
            b"int EspMapRuntime_getMapSpriteBase(uint32_t index,\n"
            b"                                      EspMapSprite* outSprite);\n"
            b"const EspNativeGameplayHudState* __real_EspNativeGameplayHud_view(void);\n"
            b"EspPlayerViewMoveStatus __real_EspPlayerView_commitPreparedMove(\n"
            b"    const EspPlayerViewState* expectedBefore,\n"
            b"    const EspPlayerViewState* preparedAfter);\n"
            b"static uint16_t tileForView(")
        current = replace_exact(
            current, b"EspNativeGameplayPickup_hudView(",
            b"__wrap_EspNativeGameplayHud_view(")
        current = replace_exact(
            current, b"EspNativeGameplayHud_viewBase()",
            b"__real_EspNativeGameplayHud_view()", 2)
        current = replace_exact(
            current, b"EspNativeGameplayPickup_commitPreparedMove(",
            b"__wrap_EspPlayerView_commitPreparedMove(")
        current = replace_exact(
            current, b"EspPlayerView_commitPreparedMoveBase(",
            b"__real_EspPlayerView_commitPreparedMove(")
    elif filename == "esp_native_gameplay_player_resources.c":
        current = replace_exact(
            current,
            b"/* Public resource facades use raw Base declarations from owner headers. */\n",
            b"const EspNativeGameplayHudState* __real_EspNativeGameplayHud_view(void);\n"
            b"EspPlayerViewMoveStatus __real_EspPlayerView_commitPreparedMove(\n"
            b"    const EspPlayerViewState* expectedBefore,\n"
            b"    const EspPlayerViewState* preparedAfter);\n")
        current = replace_exact(
            current, b"EspPlayerViewMoveStatus EspPlayerView_commitPreparedMove(",
            b"EspPlayerViewMoveStatus __wrap_EspPlayerView_commitPreparedMove(")
        current = replace_exact(
            current, b"EspPlayerView_commitPreparedMoveBase(expectedBefore, preparedAfter)",
            b"__real_EspPlayerView_commitPreparedMove(expectedBefore, preparedAfter)")
        current = replace_exact(
            current, b"const EspNativeGameplayHudState* EspNativeGameplayHud_view(void) {",
            b"const EspNativeGameplayHudState* __wrap_EspNativeGameplayHud_view(void) {")
        current = replace_exact(
            current, b"EspNativeGameplayHud_viewBase()",
            b"__real_EspNativeGameplayHud_view()")
    return current
