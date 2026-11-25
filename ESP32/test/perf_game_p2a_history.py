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

P2B_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "        ++stats->bspCandidates;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        if (!preview && type == ENEMY_TYPE &&\n            EspNativeGameplayMonsterCombat_damageBlend(\n                (uint16_t)i, DoomRPG_GetUpTimeMS()))\n            EspPerfMoveVisibility_noteUnsafe();\n#endif\n",
            "        ++stats->bspCandidates;\n"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    if (!preview) {\n        EspPerfMoveVisibility_observe(&workspace->visibility, ambientSampleMs);\n        if (transientWorldBatch.active != 0U)\n            EspPerfMoveVisibility_noteUnsafe();\n    }\n#endif\n",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    if (!preview) EspPerfMoveVisibility_observe(&workspace->visibility);\n#endif\n"
        ]
    ],
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    if (!reused) EspPerfMoveVisibility_end(frame.finalPresented && moveFrameQuiet() ? 1 : 2);\n#endif\n",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_end(1);\n#endif\n"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    /* First goal always renders. Only a synchronous synthetic ThreeGoal\n     * continuation can consume the one-shot prior full-frame witness. */\n    const int quietBefore = moveFrameQuiet();\n    const uint32_t nowMs = (uint32_t)(esp_timer_get_time()/1000LL);\n    const int reused = EspPerfMoveVisibility_tryReuse(\n        movement != EspNativeGameplayMonsterMovement_view(),\n        quietBefore && !EspNativeSpriteRenderer_ambientDue(nowMs),\n        publishCapture.before.spriteIndex,\n        publishCapture.before.worldX, publishCapture.before.worldY,\n        publishCapture.after.worldX, publishCapture.after.worldY, nowMs);\n    if (reused) {\n        EspEntranceMusic_pumpGameplay();\n    } else {\n        EspPerfMoveVisibility_begin(publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, quietBefore);\n    }\n    if (!reused &&\n#else\n    if (\n#endif\n        !EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,\n                                           (uint8_t)player->viewAngle,\n                                           &frame)) {",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_begin(publishCapture.before.spriteIndex,\n                                publishCapture.before.worldX,\n                                publishCapture.before.worldY);\n#endif\n    if (!EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,\n                                           (uint8_t)player->viewAngle,\n                                           &frame)) {"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\nstatic int moveFrameQuiet(void) {\n    const uint32_t now = DoomRPG_GetUpTimeMS();\n    return EspNativeResidentGameplay_isActive() &&\n           EspNativeResidentGameplay_ownsWorldPresent() &&\n           !EspNativeResidentGameplay_isAutomapActive() &&\n           !EspNativeGameplayHub_isActive() &&\n           !EspNativeGameplayControls_isActive() &&\n           !EspNativeGameplayLevelUp_isActive() &&\n           !EspNativeGameplayTransition_isWaitingStats() &&\n           !EspNativeTransitionPresentation_isLoadingActive() &&\n           !EspNativeDoorAnimator_hasPendingFrames() &&\n           !EspNativeGameplayMonsterAttackVisual_isBusy() &&\n           !EspNativePlayerPainVisual_isActive(now) &&\n           !EspNativePlayerPainVisual_expiryDue(now) &&\n           !EspNativeFirstFrame_indicatorVisible() &&\n           !EspNativeGameplayPresentGate_isArmed() &&\n           !EspAssetPack_isOpen() &&\n           EspNativeGameplayMoveEvents_moveFrameQuiet() &&\n           EspNativeGameplayActionEngine_moveFrameQuiet() &&\n           EspNativeGameplayGibFx_moveFrameQuiet() &&\n           EspPickupMini_moveFrameQuiet();\n}\n#endif\n\nstatic int recoveryRedraw(",
            "static int recoveryRedraw("
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#include \"esp_perf_move_visibility.h\"\n#endif\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#include \"esp_asset_pack.h\"\n#include \"esp_audio_entrance_music.h\"\n#include \"esp_native_door_animator.h\"\n#include \"esp_native_first_frame.h\"\n#include \"esp_native_gameplay_action_engine.h\"\n#include \"esp_native_gameplay_controls.h\"\n#include \"esp_native_gameplay_gib_fx.h\"\n#include \"esp_native_gameplay_hub.h\"\n#include \"esp_native_gameplay_level_up.h\"\n#include \"esp_native_gameplay_monster_attack_visual.h\"\n#include \"esp_native_gameplay_move_events.h\"\n#include \"esp_native_gameplay_pickup_miniature.h\"\n#include \"esp_native_gameplay_player_pain_visual.h\"\n#include \"esp_native_gameplay_present_gate.h\"\n#include \"esp_native_gameplay_transition.h\"\n#include \"esp_native_transition_presentation.h\"\n#include \"esp_native_resident_gameplay.h\"\n#include \"esp_native_sprite_renderer.h\"\n#include <esp_timer.h>\n#endif\n",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#include \"esp_perf_move_visibility.h\"\n#endif\n"
        ]
    ]
}

P2C_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n            EspPerfMoveVisibility_invalidate();\n#endif\n            printf(\"[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=rng-boundary-close",
            "            printf(\"[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=rng-boundary-close"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        EspPerfMoveVisibility_invalidate();\n#endif\n        printf(\"[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=post-render-owner-mismatch",
            "        printf(\"[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=post-render-owner-mismatch"
        ],
        [
            "    outResult->committed = 1U;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    /* Seed/re-arm only after all topology/position/RNG checks have closed. */\n    if (reused) EspPerfMoveVisibility_commitReuse(moveFrameBlockers() == 0U);\n    else EspPerfMoveVisibility_end(frame.finalPresented && moveFrameBlockers() == 0U ? 1 : 2);\n#endif\n    DRPG_LOGT(\"[MONSTERMOVELIVE] COMMIT",
            "    outResult->committed = 1U;\n    DRPG_LOGT(\"[MONSTERMOVELIVE] COMMIT"
        ],
        [
            "    topology = EspMapSpriteTopology_view();\n    currentPosition = EspNativeGameplayMonsterPosition_find(\n        publishCapture.after.spriteIndex);",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    if (!reused) EspPerfMoveVisibility_end(frame.finalPresented && moveFrameQuiet() ? 1 : 2);\n#endif\n    topology = EspMapSpriteTopology_view();\n    currentPosition = EspNativeGameplayMonsterPosition_find(\n        publishCapture.after.spriteIndex);"
        ],
        [
            "publishCapture.after.worldX, publishCapture.after.worldY, blockersBefore == 0U);",
            "publishCapture.after.worldX, publishCapture.after.worldY, quietBefore);"
        ],
        [
            "    /* First goal always renders. Only the synchronous ThreeGoal chain\n     * may borrow the original settled-frame witness for at most two goals. */\n    const uint32_t blockersBefore = moveFrameBlockers();\n    const uint32_t nowMs = (uint32_t)(esp_timer_get_time()/1000LL);\n    const uint32_t blockers = blockersBefore |\n        (EspNativeSpriteRenderer_ambientDue(nowMs) ? ESP_PERF_MOVE_BLOCK_AMBIENT : 0U);\n    const int reused = EspPerfMoveVisibility_tryReuse(\n        movement != EspNativeGameplayMonsterMovement_view(),\n        blockers,",
            "    /* First goal always renders. Only a synchronous synthetic ThreeGoal\n     * continuation can consume the one-shot prior full-frame witness. */\n    const int quietBefore = moveFrameQuiet();\n    const uint32_t nowMs = (uint32_t)(esp_timer_get_time()/1000LL);\n    const int reused = EspPerfMoveVisibility_tryReuse(\n        movement != EspNativeGameplayMonsterMovement_view(),\n        quietBefore && !EspNativeSpriteRenderer_ambientDue(nowMs),"
        ],
        [
            "/* Evaluate every read-only owner instead of hiding later blockers behind\n * short-circuit evaluation. The bit mask preserves the P2b admission rule. */\nstatic uint32_t moveFrameBlockers(void) {\n    const uint32_t now = DoomRPG_GetUpTimeMS();\n    uint32_t mask = 0U;\n    if (!EspNativeResidentGameplay_isActive() || !EspNativeResidentGameplay_ownsWorldPresent()) mask |= ESP_PERF_MOVE_BLOCK_WORLD;\n    if (EspNativeResidentGameplay_isAutomapActive()) mask |= ESP_PERF_MOVE_BLOCK_AUTOMAP;\n    if (EspNativeGameplayHub_isActive()) mask |= ESP_PERF_MOVE_BLOCK_HUB;\n    if (EspNativeGameplayControls_isActive()) mask |= ESP_PERF_MOVE_BLOCK_TOUCH;\n    if (EspNativeGameplayLevelUp_isActive() || EspNativeGameplayTransition_isWaitingStats() || EspNativeTransitionPresentation_isLoadingActive()) mask |= ESP_PERF_MOVE_BLOCK_MODAL;\n    if (EspNativeDoorAnimator_hasPendingFrames()) mask |= ESP_PERF_MOVE_BLOCK_DOOR;\n    if (EspNativeGameplayMonsterAttackVisual_isBusy()) mask |= ESP_PERF_MOVE_BLOCK_ATTACK;\n    if (EspNativePlayerPainVisual_isActive(now) || EspNativePlayerPainVisual_expiryDue(now)) mask |= ESP_PERF_MOVE_BLOCK_PAIN;\n    if (EspNativeFirstFrame_indicatorVisible()) mask |= ESP_PERF_MOVE_BLOCK_INDICATOR;\n    if (EspNativeGameplayPresentGate_isArmed()) mask |= ESP_PERF_MOVE_BLOCK_PRESENT;\n    if (EspAssetPack_isOpen()) mask |= ESP_PERF_MOVE_BLOCK_PAK;\n    if (!EspNativeGameplayMoveEvents_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_MOVE_EVENT;\n    if (!EspNativeGameplayActionEngine_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_FEEDBACK;\n    if (!EspNativeGameplayGibFx_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_FX;\n    if (!EspPickupMini_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_PICKUP;\n    return mask;\n}",
            "static int moveFrameQuiet(void) {\n    const uint32_t now = DoomRPG_GetUpTimeMS();\n    return EspNativeResidentGameplay_isActive() &&\n           EspNativeResidentGameplay_ownsWorldPresent() &&\n           !EspNativeResidentGameplay_isAutomapActive() &&\n           !EspNativeGameplayHub_isActive() &&\n           !EspNativeGameplayControls_isActive() &&\n           !EspNativeGameplayLevelUp_isActive() &&\n           !EspNativeGameplayTransition_isWaitingStats() &&\n           !EspNativeTransitionPresentation_isLoadingActive() &&\n           !EspNativeDoorAnimator_hasPendingFrames() &&\n           !EspNativeGameplayMonsterAttackVisual_isBusy() &&\n           !EspNativePlayerPainVisual_isActive(now) &&\n           !EspNativePlayerPainVisual_expiryDue(now) &&\n           !EspNativeFirstFrame_indicatorVisible() &&\n           !EspNativeGameplayPresentGate_isArmed() &&\n           !EspAssetPack_isOpen() &&\n           EspNativeGameplayMoveEvents_moveFrameQuiet() &&\n           EspNativeGameplayActionEngine_moveFrameQuiet() &&\n           EspNativeGameplayGibFx_moveFrameQuiet() &&\n           EspPickupMini_moveFrameQuiet();\n}"
        ]
    ]
}

P2D_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "    const uint32_t blockers = blockersBefore |\n        (EspNativeFirstFrame_indicatorDue(nowMs) ? ESP_PERF_MOVE_BLOCK_INDICATOR : 0U) |\n        (EspNativeSpriteRenderer_ambientDue(nowMs) ? ESP_PERF_MOVE_BLOCK_AMBIENT : 0U);",
            "    const uint32_t blockers = blockersBefore |\n        (EspNativeSpriteRenderer_ambientDue(nowMs) ? ESP_PERF_MOVE_BLOCK_AMBIENT : 0U);"
        ],
        [
            "    /* A visible indicator is not a pending phase change. Its absolute-clock\n     * deadline is checked separately at reuse admission, not witness seeding. */\n",
            "    if (EspNativeFirstFrame_indicatorVisible()) mask |= ESP_PERF_MOVE_BLOCK_INDICATOR;\n"
        ]
    ]
}

P2E_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "    else {\n        const int quietAfter = moveFrameBlockers() == 0U;\n        const EspMapAutomapStateView* afterMap = EspMapAutomapState_view();\n        if (!continuation)\n            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV,\n                shadowMap == NULL || afterMap == NULL ||\n                shadowAutomap != afterMap->stateFNV1a ||\n                shadowActivation != EspNativeGameplayMonsterActivation_count(),\n                quietAfter);\n        EspPerfMoveVisibility_end(frame.finalPresented && quietAfter ? 1 : 2);\n        EspPerfMoveVisibility_rememberContext(\n            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);\n    }",
            "    else EspPerfMoveVisibility_end(frame.finalPresented && moveFrameBlockers() == 0U ? 1 : 2);"
        ],
        [
            "        if (!continuation) EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);\n        EspPerfMoveVisibility_end(0);\n        EspPerfMoveVisibility_invalidate();\n#endif",
            "        EspPerfMoveVisibility_end(0);\n#endif"
        ],
        [
            "    const int continuation = movement != EspNativeGameplayMonsterMovement_view();\n    const EspMapRuntimeView* shadowRuntime = EspMapRuntime_view();\n    const EspMapLineStateView* shadowLines = EspMapLineState_viewBase();\n    const EspMapLineTextureStateView* shadowTextures = EspMapLineTextureState_view();\n    const EspMapAutomapStateView* shadowMap = EspMapAutomapState_view();\n    const uint32_t shadowActivation = EspNativeGameplayMonsterActivation_count();\n    const uint32_t shadowAutomap = shadowMap != NULL ? shadowMap->stateFNV1a : 0U;\n    EspPerfMoveVisibilityContext shadowContext;\n    memset(&shadowContext, 0, sizeof(shadowContext));\n    const int shadowContextReady = shadowRuntime != NULL &&\n        shadowLines != NULL && shadowTextures != NULL;\n    if (shadowContextReady) {\n        shadowContext.arena = shadowRuntime->arenaFNV1a;\n        shadowContext.lineFNV = shadowLines->stateFNV1a;\n        shadowContext.textureFNV = shadowTextures->stateFNV1a;\n        shadowContext.x = player->viewX;\n        shadowContext.y = player->viewY;\n        shadowContext.z = player->viewZ;\n        shadowContext.angle = player->viewAngle;\n    }\n    if (!continuation)\n        (void)EspPerfMoveVisibility_firstShadowBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs);\n    const int reused = EspPerfMoveVisibility_tryReuse(\n        continuation,\n        blockers,",
            "    const int reused = EspPerfMoveVisibility_tryReuse(\n        movement != EspNativeGameplayMonsterMovement_view(),\n        blockers,"
        ],
        [
            "#include \"esp_asset_pack.h\"\n#include \"esp_map_automap_state.h\"\n#include \"esp_map_line_state.h\"\n#include \"esp_map_line_texture_state.h\"\n#include \"esp_native_gameplay_monster_activation.h\"\n#include \"esp_audio_entrance_music.h\"",
            "#include \"esp_asset_pack.h\"\n#include \"esp_audio_entrance_music.h\""
        ]
    ]
}

def restore_pre_perf_game_p2a(name, data):
    for newer, older in P2E_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2e wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2D_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2d wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2C_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2c wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2B_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2b wiring inverse")
        data = data.replace(a, b, 1)
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
