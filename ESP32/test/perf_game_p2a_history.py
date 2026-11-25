"""Exact inverse of P2a additive diagnostics; frozen historical hashes unchanged."""
EDITS = {
    "esp_native_bsp_visibility.c": [
        "\n/* Explicit old position, same leaf traversal as the production renderer. */\nint EspNativeBspVisibility_positionVisible(\n    const EspNativeBspVisibilityState* state, uint32_t spriteIndex,\n    uint16_t x, uint16_t y, uint32_t* outLeaf) {\n    const EspMapRuntimeView* runtime = EspMapRuntime_view();\n    EspMapSprite sprite;\n    uint32_t leaf;\n    if (outLeaf != NULL) *outLeaf = UINT32_MAX;\n    if (state == NULL || runtime == NULL ||\n        spriteIndex >= runtime->mapSpriteCount ||\n        !EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;\n    sprite.x = x;\n    sprite.y = y;\n    if (!spriteLeaf(runtime, &sprite, &leaf) ||\n        leaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES) return 0;\n    if (outLeaf != NULL) *outLeaf = leaf;\n    return (state->visibleLeaves[leaf >> 5] & (1U << (leaf & 31U))) != 0U;\n}\n\n"
    ],
    "esp_native_sprite_renderer.c": [
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#include \"esp_perf_move_visibility.h\"\n#endif\n",
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    if (!preview) EspPerfMoveVisibility_observe(&workspace->visibility);\n#endif\n"
    ],
    "esp_native_gameplay_monster_movement_publish.c": [
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#include \"esp_perf_move_visibility.h\"\n#endif\n",
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_begin(publishCapture.before.spriteIndex,\n                                publishCapture.before.worldX,\n                                publishCapture.before.worldY);\n#endif\n",
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        EspPerfMoveVisibility_end(0);\n#endif\n",
        "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_end(1);\n#endif\n"
    ]
}

P2B_OWNER_EDITS = {
    "esp_native_gameplay_action_engine.c": [
        [
            "\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\nint EspNativeGameplayActionEngine_moveFrameQuiet(void) {\n    return !actionState.feedbackPending && !actionState.feedbackVisible &&\n           !actionState.feedbackFollowupCount &&\n           !actionState.viewportFlashPending && !actionState.viewportFlashVisible &&\n           !actionState.framebufferFresh && !directionCue.pendingValid &&\n           !directionCue.stageNonlethalBorder &&\n           !EspNativeGameplayFacingLabel_isDirty();\n}\n#endif\n",
            ""
        ]
    ],
    "esp_native_gameplay_gib_fx.c": [
        [
            "\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\nint EspNativeGameplayGibFx_moveFrameQuiet(void) {\n    return !gibFxOwner.active && !hitFxOwner.active;\n}\n#endif\n",
            ""
        ]
    ],
    "esp_native_gameplay_move_events.c": [
        [
            "\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\nint EspNativeGameplayMoveEvents_moveFrameQuiet(void) {\n    return !transaction.active;\n}\n#endif\n",
            ""
        ]
    ],
    "esp_native_gameplay_pickup_miniature.c": [
        [
            "\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\nint EspPickupMini_moveFrameQuiet(void) {\n    return !state.active && !state.count;\n}\n#endif\n",
            ""
        ]
    ]
}

def restore_pre_perf_game_p2a(name, data):
    for newer, older in P2B_OWNER_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2b owner inverse")
        data = data.replace(a, b, 1)
    if name == "esp_native_bsp_visibility.c":
        newer = "/* Resolve explicit coordinates through the production BSP, bypassing the spawn cache. */\nint EspNativeBspVisibility_positionLeaf(uint32_t spriteIndex,\n    uint16_t x, uint16_t y, uint32_t* outLeaf) {\n    const EspMapRuntimeView* runtime = EspMapRuntime_view();\n    EspMapSprite sprite;\n    uint32_t leaf;\n    if (outLeaf != NULL) *outLeaf = UINT32_MAX;\n    if (outLeaf == NULL || runtime == NULL ||\n        spriteIndex >= runtime->mapSpriteCount ||\n        !EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;\n    sprite.x = x;\n    sprite.y = y;\n    if (!spriteLeaf(runtime, &sprite, &leaf) ||\n        leaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES) return 0;\n    *outLeaf = leaf;\n    return 1;\n}\n\nint EspNativeBspVisibility_positionVisible(\n    const EspNativeBspVisibilityState* state, uint32_t spriteIndex,\n    uint16_t x, uint16_t y, uint32_t* outLeaf) {\n    uint32_t leaf;\n    if (outLeaf != NULL) *outLeaf = UINT32_MAX;\n    if (state == NULL ||\n        !EspNativeBspVisibility_positionLeaf(spriteIndex, x, y, &leaf)) return 0;\n    if (outLeaf != NULL) *outLeaf = leaf;\n    return (state->visibleLeaves[leaf >> 5] & (1U << (leaf & 31U))) != 0U;\n}\n\n".encode("utf-8")
        older = "/* Explicit old position, same leaf traversal as the production renderer. */\nint EspNativeBspVisibility_positionVisible(\n    const EspNativeBspVisibilityState* state, uint32_t spriteIndex,\n    uint16_t x, uint16_t y, uint32_t* outLeaf) {\n    const EspMapRuntimeView* runtime = EspMapRuntime_view();\n    EspMapSprite sprite;\n    uint32_t leaf;\n    if (outLeaf != NULL) *outLeaf = UINT32_MAX;\n    if (state == NULL || runtime == NULL ||\n        spriteIndex >= runtime->mapSpriteCount ||\n        !EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;\n    sprite.x = x;\n    sprite.y = y;\n    if (!spriteLeaf(runtime, &sprite, &leaf) ||\n        leaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES) return 0;\n    if (outLeaf != NULL) *outLeaf = leaf;\n    return (state->visibleLeaves[leaf >> 5] & (1U << (leaf & 31U))) != 0U;\n}\n\n".encode("utf-8")
        assert data.count(newer) == 1
        data = data.replace(newer, older, 1)
    for block in EDITS.get(name, []):
        block = block.encode("utf-8")
        assert data.count(block) == 1, (name, "P2a inverse anchor")
        data = data.replace(block, b"", 1)
    return data
