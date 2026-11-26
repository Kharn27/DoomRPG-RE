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

P2F_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "    animation = EspNativeAmbientSprite_frame(parent->info,entityType,\n                                              entitySubtype,\n                                              parent->index,ambientSampleMs);\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_noteAmbientPhase(parent->info, entityType,\n        entitySubtype, parent->index, ambientSampleMs);\n#endif",
            "    animation = EspNativeAmbientSprite_frame(parent->info,entityType,\n                                              entitySubtype,\n                                              parent->index,ambientSampleMs);"
        ]
    ],
    "esp_native_gameplay_frame.c": [
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_frameStarted(preview);\n#endif\n    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));\n    if (frameScratch.busy) return 0;",
            "    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));\n    if (frameScratch.busy) return 0;"
        ],
        [
            "#include \"esp_audio_entrance_music.h\"\n#include \"esp_perf_move_visibility.h\"\n#endif",
            "#include \"esp_audio_entrance_music.h\"\n#endif"
        ]
    ]
}

P2G_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "                y = EspNativeSpriteMask_runEnd(bits, y, frame->height, 0);\n                if (y >= frame->height) break;\n\n                start = y;\n                texelBase = cursor;\n                y = EspNativeSpriteMask_runEnd(bits, y, frame->height, 1);\n                length = y - start;\n                cursor += (uint32_t)length;",
            "                while (y < frame->height &&\n                       (bits[y / 8] & (1U << (y & 7))) == 0U) {\n                    ++y;\n                }\n                if (y >= frame->height) break;\n\n                start = y;\n                texelBase = cursor;\n                while (y < frame->height &&\n                       (bits[y / 8] & (1U << (y & 7))) != 0U) {\n                    ++cursor;\n                    ++y;\n                }\n\n                length = y - start;"
        ],
        [
            "        active += EspNativeSpriteMask_countColumn(\n            column, (unsigned)frame->height);",
            "        int y;\n        for (y = 0; y < frame->height; ++y) {\n            if ((column[y / 8] & (1U << (y & 7))) != 0U) ++active;\n        }"
        ],
        [
            "#include \"esp_native_ambient_sprite.h\"\n#include \"esp_native_sprite_mask.h\"",
            "#include \"esp_native_ambient_sprite.h\""
        ]
    ]
}

P2H_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "                /* P2h: prove the entire normal opaque span in bounds\n                 * once. The 32-bit cursor is safe because packedBytes is\n                 * bounded to 2048 and texelStep is a nonnegative int.\n                 * Keep the original loop for additive/glow and malformed\n                 * ranges, including its exact partial-failure semantics. */\n                if (renderMode == RENDER_MODE_NORMAL && !glow &&\n                    frame->packedBytes <= MAX_TEXELS &&\n                    position >= 0 && texelStep >= 0 &&\n                    position + (int64_t)(remaining - 1) * texelStep <\n                        ((int64_t)frame->packedBytes << 13)) {\n                    uint32_t pos = (uint32_t)position;\n                    int left = remaining;\n                    while (left-- > 0) {\n                        const uint8_t packed = frame->texels[pos >> 13];\n                        *destination = frame->palette[\n                            (packed >> ((pos >> 10) & 4U)) & 15U];\n                        destination += pitch;\n                        pos += (uint32_t)texelStep;\n                    }\n                    stats->pixelsDrawn += (uint32_t)remaining;\n                    continue;\n                }\n\n                while (remaining-- > 0) {\n                    uint32_t packedIndex;",
            "                while (remaining-- > 0) {\n                    uint32_t packedIndex;"
        ]
    ]
}

P2I_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "                /* P2i: the same bounded cursor proof for mode 7.\n                 * Preserve the original approximate RGB565 carry, and\n                 * distinguish base-mode counters from glow counters. */\n                if (renderMode == RENDER_MODE_ADD &&\n                    frame->packedBytes <= MAX_TEXELS &&\n                    position >= 0 && texelStep >= 0 &&\n                    position + (int64_t)(remaining - 1) * texelStep <\n                        ((int64_t)frame->packedBytes << 13)) {\n                    uint32_t pos = (uint32_t)position;\n                    int left = remaining;\n                    while (left-- > 0) {\n                        const uint8_t packed = frame->texels[pos >> 13];\n                        const uint16_t source = frame->palette[\n                            (packed >> ((pos >> 10) & 4U)) & 15U];\n                        const uint32_t color = (uint32_t)(source & 0xf7deU) +\n                            ((uint32_t)(*destination) & 0xf7deU);\n                        const uint32_t carry = color & 0x10820U;\n                        *destination = (uint16_t)((color & 0xf7deU) |\n                            (carry >> 1) | (carry >> 2) | (carry >> 3));\n                        destination += pitch;\n                        pos += (uint32_t)texelStep;\n                    }\n                    if (glow) stats->glowPixels += (uint32_t)remaining;\n                    else {\n                        stats->pixelsDrawn += (uint32_t)remaining;\n                        stats->mode7Pixels += (uint32_t)remaining;\n                    }\n                    continue;\n                }\n\n                /* P2h: prove the entire normal opaque span in bounds",
            "                /* P2h: prove the entire normal opaque span in bounds"
        ],
        [
            "        scale = dSide == 0 ? constantScale : (0x40000000 / depth) << 2;\n        /* Occluded columns still advance both original recurrences. */\n        column = render->columnScale[x] >= scale\n            ? (int)(DoomRPG_FixedMul(tex, scale) >> 16) : 0;\n        depth += dSide;\n        tex += dTex;\n\n        if (render->columnScale[x] >= scale) {",
            "        scale = (0x40000000 / depth) << 2;\n        column = (int)(DoomRPG_FixedMul(tex, scale) >> 16);\n        depth += dSide;\n        tex += dTex;\n\n        if (render->columnScale[x] >= scale) {"
        ],
        [
            "    /* A camera-facing billboard has identical projected depth at\n     * both ends. dSide == 0 is the exact integer recurrence criterion,\n     * also covering oriented spans whose projected delta rounds to zero. */\n    if (dSide == 0 && x < x2 && depth > 0)\n        constantScale = (0x40000000 / depth) << 2;\n\n    while (x < x2) {\n        int scale;\n        int column;\n        int texelStep;",
            "    while (x < x2) {\n        int scale;\n        int column;\n        int texelStep;"
        ],
        [
            "    int tex;\n    int depth;\n    int constantScale = 0;\n\n    if (renderMode",
            "    int tex;\n    int depth;\n\n    if (renderMode"
        ]
    ]
}

P2J_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "    *outStats = stats;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_STAMP(j5);\n    if (spritePerf.active && ok) {\n        /* readUs overlaps sources/load/draw; do not sum those metrics. */\n        printf(\"[PERFGAMEP2J] ordinal=%u preview=%u candidates=%u drops=%u transient=%u visibilityUs=%u orderUs=%u sourcesUs=%u drawUs=%u tailUs=%u readCalls=%u readUs=%u mapLoadCalls=%u mapLoadUs=%u mapSpanCalls=%u mapSpanUs=%u packReads=%u pixels=%u glowPixels=%u sample=1/16\\n\",\n            (unsigned)spritePerf.ordinal,(unsigned)preview,(unsigned)orderCount,\n            (unsigned)dropOrderCount,(unsigned)transientWorldBatch.active,\n            (unsigned)(j1-j0),(unsigned)(j2-j1),(unsigned)(j3-j2),\n            (unsigned)(j4-j3),(unsigned)(j5-j4),\n            (unsigned)spritePerf.readCalls,(unsigned)spritePerf.readUs,\n            (unsigned)spritePerf.loadCalls,(unsigned)spritePerf.loadUs,\n            (unsigned)spritePerf.spanCalls,(unsigned)spritePerf.spanUs,\n            (unsigned)stats.packReads,(unsigned)stats.pixelsDrawn,\n            (unsigned)stats.glowPixels);\n    }\n    spritePerf.active = 0U;\n#endif\n#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE",
            "    *outStats = stats;\n#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_STAMP(j4);\n#endif\n    /* A complete frame need not contain a visible additive sprite or glow.",
            "    /* A complete frame need not contain a visible additive sprite or glow."
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_STAMP(j2);\n#endif\n    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;\n    opened = 1;\n    if (!initSources(&sources, &stats)) goto done;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_STAMP(j3);\n#endif",
            "    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;\n    opened = 1;\n    if (!initSources(&sources, &stats)) goto done;"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_STAMP(j1);\n#endif\n    stats.depthNodes = workspace->visibility.nodes;",
            "    stats.depthNodes = workspace->visibility.nodes;"
        ],
        [
            "    saveScratch(render, saved);\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    ++spritePerf.ordinal;\n    spritePerf.active = (uint8_t)((spritePerf.ordinal & 15U) == 0U);\n    if (spritePerf.active) {\n        spritePerf.readUs=spritePerf.readCalls=spritePerf.loadUs=0U;\n        spritePerf.loadCalls=spritePerf.spanUs=spritePerf.spanCalls=0U;\n        j0=esp_timer_get_time();\n    }\n#endif\n    if (scratchOwnerLogged == 0U) {",
            "    saveScratch(render, saved);\n    if (scratchOwnerLogged == 0U) {"
        ],
        [
            "    int opened = 0;\n    int ok = 0;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    int64_t j0=0LL,j1=0LL,j2=0LL,j3=0LL,j4=0LL,j5=0LL;\n    spritePerf.active = 0U;\n#endif\n\n    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));",
            "    int opened = 0;\n    int ok = 0;\n\n    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_ACCUM(spanStart, spanUs, spanCalls);\n#endif\n",
            "        P2J_ACCUM(spanStart, spanUs, spanCalls);\n"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_START(spanStart);\n#endif\n",
            "        P2J_START(spanStart);\n"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_ACCUM(loadStart, loadUs, loadCalls);\n#endif\n",
            "        P2J_ACCUM(loadStart, loadUs, loadCalls);\n"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_START(loadStart);\n#endif\n",
            "        P2J_START(loadStart);\n"
        ],
        [
            "    {\n        P2J_START(spanStart);\n        const int drawn = spans(render, &line, frame, renderMode, glow, stats);\n        P2J_ACCUM(spanStart, spanUs, spanCalls);\n        if (tinted != 0U) {",
            "    {\n        const int drawn = spans(render, &line, frame, renderMode, glow, stats);\n        if (tinted != 0U) {"
        ],
        [
            "        P2J_ACCUM(loadStart, loadUs, loadCalls);\n        if (crossAnimation != NULL) *crossAnimation = animation;\n    }\n\n    minimum =",
            "        if (crossAnimation != NULL) *crossAnimation = animation;\n    }\n\n    minimum ="
        ],
        [
            "    else {\n        P2J_START(loadStart);\n        if (!loadFrame(sources, logical, animation, glow,\n                       frame, seenLogical, stats)) {",
            "    else {\n        if (!loadFrame(sources, logical, animation, glow,\n                       frame, seenLogical, stats)) {"
        ],
        [
            "        offset > entry->size || bytes > entry->size - offset) {\n        return 0;\n    }\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_START(readStart);\n#endif\n    if (!EspAssetPack_readRange(entry, offset, destination, bytes)) return 0;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n    P2J_ACCUM(readStart, readUs, readCalls);\n#endif\n    ++stats->packReads;",
            "        offset > entry->size || bytes > entry->size - offset ||\n        !EspAssetPack_readRange(entry, offset, destination, bytes)) {\n        return 0;\n    }\n    ++stats->packReads;"
        ],
        [
            "static uint8_t scratchOwnerLogged;\n\n/* P2j: bounded diagnostic, one in sixteen successful render attempts.\n * No extra reads or geometry operations. All state is renderer-private;\n * timing is disabled before publishing stats or returning to gameplay. */\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#ifndef DOOMRPG_PERF_SPRITE_SAMPLE\n#define DOOMRPG_PERF_SPRITE_SAMPLE 1\n#endif\n#if DOOMRPG_PERF_SPRITE_SAMPLE\nstatic struct {\n    uint32_t ordinal, readUs, readCalls, loadUs, loadCalls, spanUs, spanCalls;\n    uint8_t active;\n} spritePerf;\n#define P2J_START(name) const int64_t name = spritePerf.active ? esp_timer_get_time() : 0LL\n#define P2J_ACCUM(name, field, calls) do { if (spritePerf.active) { \\\n    spritePerf.field += (uint32_t)(esp_timer_get_time() - (name)); \\\n    ++spritePerf.calls; } } while (0)\n#define P2J_STAMP(name) do { if (spritePerf.active) (name) = esp_timer_get_time(); } while (0)\n#else\n#define P2J_START(name) do { } while (0)\n#define P2J_ACCUM(name, field, calls) do { } while (0)\n#define P2J_STAMP(name) do { } while (0)\n#endif\n#else\n#define P2J_START(name) do { } while (0)\n#define P2J_ACCUM(name, field, calls) do { } while (0)\n#define P2J_STAMP(name) do { } while (0)\n#endif\n",
            "static uint8_t scratchOwnerLogged;\n"
        ]
    ]
}

P2K_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "static int projectionRecordIndex(uint16_t spriteIndex, uint32_t* outIndex) {\n    const EspNativeGameplayMonsterView* monsters =\n        EspNativeGameplayMonsterState_view();\n    uint32_t lo = 0U, hi;\n\n    if (outIndex != NULL) *outIndex = 0U;\n    if (monsters == NULL || monsters->records == NULL || outIndex == NULL ||\n        monsters->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) {\n        return 0;\n    }\n    /* MonsterState is strictly sprite-index ordered on construction and\n     * validated restore. The ordinal remains the original record index. */\n    hi = monsters->count;\n    while (lo < hi) {\n        const uint32_t mid = lo + ((hi - lo) >> 1);\n        const uint16_t current = monsters->records[mid].spriteIndex;\n        if (current == spriteIndex) {\n            *outIndex = mid;\n            return 1;\n        }\n        if (current < spriteIndex) lo = mid + 1U;\n        else hi = mid;\n    }\n    return 0;\n}",
            "static int projectionRecordIndex(uint16_t spriteIndex, uint32_t* outIndex) {\n    const EspNativeGameplayMonsterView* monsters =\n        EspNativeGameplayMonsterState_view();\n    uint32_t i;\n\n    if (outIndex != NULL) *outIndex = 0U;\n    if (monsters == NULL || monsters->records == NULL || outIndex == NULL ||\n        monsters->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) {\n        return 0;\n    }\n    for (i = 0U; i < monsters->count; ++i) {\n        if (monsters->records[i].spriteIndex == spriteIndex) {\n            *outIndex = i;\n            return 1;\n        }\n    }\n    return 0;\n}"
        ]
    ],
    "esp_native_gameplay_monster_position.c": [
        [
            "static EspNativeGameplayMonsterPositionRecord* findMutable(uint16_t spriteIndex) {\n    uint32_t lo = 0U, hi = positionView.count;\n    if (positionRecords == NULL) return NULL;\n    /* Position records mirror the sorted MonsterState order, including\n     * validated checkpoint restore. Movement edits coordinates, not keys. */\n    while (lo < hi) {\n        const uint32_t mid = lo + ((hi - lo) >> 1);\n        const uint16_t current = positionRecords[mid].spriteIndex;\n        if (current == spriteIndex) return &positionRecords[mid];\n        if (current < spriteIndex) lo = mid + 1U;\n        else hi = mid;\n    }\n    return NULL;\n}",
            "static EspNativeGameplayMonsterPositionRecord* findMutable(uint16_t spriteIndex) {\n    uint32_t i;\n    if (positionRecords == NULL) return NULL;\n    for (i = 0U; i < positionView.count; ++i) {\n        if (positionRecords[i].spriteIndex == spriteIndex) return &positionRecords[i];\n    }\n    return NULL;\n}"
        ]
    ]
}

P2L_WIRING_EDITS = {
    "esp_native_bsp_visibility.h": [
        [
            "int EspNativeBspVisibility_publishAutomap(\n    const EspNativeBspVisibilityState* state,\n    uint16_t* outLinesMutated,\n    uint16_t* outSpritesMutated);\n/* Only for the same serialized sprite render, with no position/geometry\n * mutations since the negative admission queries. NULL uses the old route. */\nint EspNativeBspVisibility_publishAutomapWithRejected(\n    const EspNativeBspVisibilityState* state,\n    const uint8_t rejected[ESP_NATIVE_BSP_REJECT_MEMO_BYTES],\n    uint16_t* outLinesMutated, uint16_t* outSpritesMutated,\n    uint16_t* outReused);",
            "int EspNativeBspVisibility_publishAutomap(\n    const EspNativeBspVisibilityState* state,\n    uint16_t* outLinesMutated,\n    uint16_t* outSpritesMutated);"
        ],
        [
            "#define ESP_NATIVE_BSP_VISIBILITY_LINE_BYTES \\\n    (ESP_NATIVE_BSP_VISIBILITY_MAX_LINES / 8U)\n\n/* P2l: caller-owned one-render memo of successful negative queries only.\n * Hidden/unqueried and positive sprites are never skipped by automap. */\n#define ESP_NATIVE_BSP_REJECT_MEMO_SPRITES 1024U\n#define ESP_NATIVE_BSP_REJECT_MEMO_BYTES (ESP_NATIVE_BSP_REJECT_MEMO_SPRITES / 8U)",
            "#define ESP_NATIVE_BSP_VISIBILITY_LINE_BYTES \\\n    (ESP_NATIVE_BSP_VISIBILITY_MAX_LINES / 8U)"
        ]
    ],
    "esp_native_bsp_visibility.c": [
        [
            "int EspNativeBspVisibility_publishAutomapWithRejected(\n    const EspNativeBspVisibilityState* state,\n    const uint8_t rejected[ESP_NATIVE_BSP_REJECT_MEMO_BYTES],\n    uint16_t* outLinesMutated, uint16_t* outSpritesMutated,\n    uint16_t* outReused) {\n    const EspMapRuntimeView* runtime = EspMapRuntime_view();\n    uint32_t i;\n    uint16_t linesMutated = 0U;\n    uint16_t spritesMutated = 0U;\n    uint16_t reused = 0U;\n    if (outReused != NULL) *outReused = 0U;\n\n    if (outLinesMutated != NULL) *outLinesMutated = 0U;\n    if (outSpritesMutated != NULL) *outSpritesMutated = 0U;\n    if (state == NULL || runtime == NULL ||\n        !EspMapAutomapState_isReady() ||\n        runtime->lineCount > ESP_NATIVE_BSP_VISIBILITY_MAX_LINES) {\n        return 0;\n    }\n\n    for (i = 0U; i < runtime->lineCount; ++i) {\n        uint8_t revealed;\n        if ((state->visibleAutomapLines[i >> 3] &\n             (uint8_t)(1U << (i & 7U))) == 0U) {\n            continue;\n        }\n        if (!EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;\n        if (revealed == 0U) {\n            if (!EspMapAutomapState_setLineRevealed(i, 1U)) return 0;\n            if (linesMutated != UINT16_MAX) ++linesMutated;\n        }\n    }\n\n    for (i = 0U; i < runtime->mapSpriteCount; ++i) {\n        uint32_t leaf = UINT32_MAX;\n        uint8_t revealed;\n        /* Only a previously successful, rejected leaf query can bypass\n         * the second lookup. The activation wrapper has no side effects\n         * for a negative query. Positive and unqueried sprites still use it. */\n        if (rejected != NULL &&\n            runtime->mapSpriteCount <= ESP_NATIVE_BSP_REJECT_MEMO_SPRITES &&\n            (rejected[i >> 3U] & (uint8_t)(1U << (i & 7U))) != 0U) {\n            ++reused;\n            continue;\n        }\n        if (!EspNativeBspVisibility_mapSpriteVisible(state, i, &leaf)) {\n            if (leaf == UINT32_MAX) return 0;\n            continue;\n        }\n        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed)) return 0;\n        if (revealed == 0U) {\n            if (!EspMapAutomapState_setSpriteRevealed(i, 1U)) return 0;\n            if (spritesMutated != UINT16_MAX) ++spritesMutated;\n        }\n    }\n\n    if (outLinesMutated != NULL) *outLinesMutated = linesMutated;\n    if (outSpritesMutated != NULL) *outSpritesMutated = spritesMutated;\n    if (outReused != NULL) *outReused = reused;\n    return 1;\n}\n\nint EspNativeBspVisibility_publishAutomap(\n    const EspNativeBspVisibilityState* state,\n    uint16_t* outLinesMutated,\n    uint16_t* outSpritesMutated) {\n    return EspNativeBspVisibility_publishAutomapWithRejected(\n        state, NULL, outLinesMutated, outSpritesMutated, NULL);\n}\n",
            "int EspNativeBspVisibility_publishAutomap(\n    const EspNativeBspVisibilityState* state,\n    uint16_t* outLinesMutated,\n    uint16_t* outSpritesMutated) {\n    const EspMapRuntimeView* runtime = EspMapRuntime_view();\n    uint32_t i;\n    uint16_t linesMutated = 0U;\n    uint16_t spritesMutated = 0U;\n\n    if (outLinesMutated != NULL) *outLinesMutated = 0U;\n    if (outSpritesMutated != NULL) *outSpritesMutated = 0U;\n    if (state == NULL || runtime == NULL ||\n        !EspMapAutomapState_isReady() ||\n        runtime->lineCount > ESP_NATIVE_BSP_VISIBILITY_MAX_LINES) {\n        return 0;\n    }\n\n    for (i = 0U; i < runtime->lineCount; ++i) {\n        uint8_t revealed;\n        if ((state->visibleAutomapLines[i >> 3] &\n             (uint8_t)(1U << (i & 7U))) == 0U) {\n            continue;\n        }\n        if (!EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;\n        if (revealed == 0U) {\n            if (!EspMapAutomapState_setLineRevealed(i, 1U)) return 0;\n            if (linesMutated != UINT16_MAX) ++linesMutated;\n        }\n    }\n\n    for (i = 0U; i < runtime->mapSpriteCount; ++i) {\n        uint32_t leaf = UINT32_MAX;\n        uint8_t revealed;\n        if (!EspNativeBspVisibility_mapSpriteVisible(state, i, &leaf)) {\n            if (leaf == UINT32_MAX) return 0;\n            continue;\n        }\n        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed)) return 0;\n        if (revealed == 0U) {\n            if (!EspMapAutomapState_setSpriteRevealed(i, 1U)) return 0;\n            if (spritesMutated != UINT16_MAX) ++spritesMutated;\n        }\n    }\n\n    if (outLinesMutated != NULL) *outLinesMutated = linesMutated;\n    if (outSpritesMutated != NULL) *outSpritesMutated = spritesMutated;\n    return 1;\n}\n"
        ]
    ],
    "esp_native_sprite_renderer.c": [
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        if (spritePerf.active)\n            printf(\"[PERFGAMEP2L] ordinal=%u rejectedReused=%u sprites=%u sample=1/16\\n\",\n                   (unsigned)spritePerf.ordinal, (unsigned)reused,\n                   (unsigned)runtime->mapSpriteCount);\n#endif\n        if (linesMutated != 0U || spritesMutated != 0U) {\n            printf(\"[AUTOMAPVIS] PUBLISH",
            "        if (linesMutated != 0U || spritesMutated != 0U) {\n            printf(\"[AUTOMAPVIS] PUBLISH"
        ],
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        uint16_t reused = 0U;\n        /* No world changes between buildOrder and publication. Restrict\n         * reuse to no-drop frames, excluding the catalog prime path too. */\n        if (!EspNativeBspVisibility_publishAutomapWithRejected(\n                &workspace->visibility,\n                dropOrderCount == 0U && transientWorldBatch.active == 0U\n                    ? workspace->rejected : NULL,\n                &linesMutated, &spritesMutated, &reused)) {\n#else\n        if (!EspNativeBspVisibility_publishAutomap(\n                &workspace->visibility,\n                &linesMutated,\n                &spritesMutated)) {\n#endif",
            "        if (!EspNativeBspVisibility_publishAutomap(\n                &workspace->visibility,\n                &linesMutated,\n                &spritesMutated)) {"
        ],
        [
            "                    workspace->order, &stats, &orderCount, preview,\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n                    preview ? NULL : workspace->rejected,\n#endif\n#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE",
            "                    workspace->order, &stats, &orderCount, preview,\n#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE"
        ],
        [
            "            if (leaf == UINT32_MAX) return 0;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n            if (rejected != NULL &&\n                runtime->mapSpriteCount <= ESP_NATIVE_BSP_REJECT_MEMO_SPRITES)\n                rejected[i >> 3U] |= (uint8_t)(1U << (i & 7U));\n#endif\n            ++stats->bspRejected;",
            "            if (leaf == UINT32_MAX) return 0;\n            ++stats->bspRejected;"
        ],
        [
            "                      int preview,\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n                      uint8_t* rejected,\n#endif\n                      SpriteOrderProbe* probe) {",
            "                      int preview,\n                      SpriteOrderProbe* probe) {"
        ],
        [
            "    EspNativeBspVisibilityState visibility;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    uint8_t rejected[ESP_NATIVE_BSP_REJECT_MEMO_BYTES];\n#endif\n} SpriteWorkspace;",
            "    EspNativeBspVisibilityState visibility;\n} SpriteWorkspace;"
        ]
    ]
}

P2N_WIRING_EDITS = {
    "esp_native_bsp_visibility.c": [
        [
            "    const int memoReady = rejected != NULL &&\n        runtime->mapSpriteCount <= ESP_NATIVE_BSP_REJECT_MEMO_SPRITES;\n    for (i = 0U; i < runtime->mapSpriteCount; ++i) {\n        uint32_t leaf = UINT32_MAX;\n        uint8_t revealed;\n        /* Skip eight known negatives together, never padding. */\n        if (memoReady && (i & 7U) == 0U &&\n            runtime->mapSpriteCount - i >= 8U && rejected[i >> 3U] == 0xffU) {\n            reused += 8U;\n            i += 7U;\n            continue;\n        }\n        if (memoReady &&\n            (rejected[i >> 3U] & (uint8_t)(1U << (i & 7U))) != 0U) {\n            ++reused;\n            continue;\n        }",
            "    for (i = 0U; i < runtime->mapSpriteCount; ++i) {\n        uint32_t leaf = UINT32_MAX;\n        uint8_t revealed;\n        /* Only a previously successful, rejected leaf query can bypass\n         * the second lookup. The activation wrapper has no side effects\n         * for a negative query. Positive and unqueried sprites still use it. */\n        if (rejected != NULL &&\n            runtime->mapSpriteCount <= ESP_NATIVE_BSP_REJECT_MEMO_SPRITES &&\n            (rejected[i >> 3U] & (uint8_t)(1U << (i & 7U))) != 0U) {\n            ++reused;\n            continue;\n        }"
        ],
        [
            "    /* P2n: skip bytes with no visible unrevealed lines. The owner\n     * view is read-only; retain checked per-line reads on mismatch. */\n    const EspMapAutomapStateView* automap = EspMapAutomapState_view();\n    const int lineBitsReady = automap != NULL &&\n        automap->lineRevealedBits != NULL &&\n        automap->lineCount == runtime->lineCount &&\n        automap->lineBitsetBytes == (runtime->lineCount + 7U) / 8U;\n    for (i = 0U; i < runtime->lineCount;) {\n        uint8_t visible = state->visibleAutomapLines[i >> 3U];\n        if (lineBitsReady)\n            visible &= (uint8_t)~automap->lineRevealedBits[i >> 3U];\n        if (visible == 0U) {\n            i += 8U;\n            continue;\n        }\n        for (uint32_t bit = 0U; bit < 8U && i < runtime->lineCount; ++bit, ++i) {\n            uint8_t revealed = 0U;\n            if ((visible & (uint8_t)(1U << bit)) == 0U) continue;\n            if (!lineBitsReady &&\n                !EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;\n            if (revealed == 0U) {\n                if (!EspMapAutomapState_setLineRevealed(i, 1U)) return 0;\n                if (linesMutated != UINT16_MAX) ++linesMutated;\n            }\n        }\n    }",
            "    for (i = 0U; i < runtime->lineCount; ++i) {\n        uint8_t revealed;\n        if ((state->visibleAutomapLines[i >> 3] &\n             (uint8_t)(1U << (i & 7U))) == 0U) {\n            continue;\n        }\n        if (!EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;\n        if (revealed == 0U) {\n            if (!EspMapAutomapState_setLineRevealed(i, 1U)) return 0;\n            if (linesMutated != UINT16_MAX) ++linesMutated;\n        }\n    }"
        ]
    ],
    "esp_native_sprite_renderer.c": [
        [
            "        const int automapOk = EspNativeBspVisibility_publishAutomapWithRejected(\n                &workspace->visibility,\n                dropOrderCount == 0U && transientWorldBatch.active == 0U\n                    ? workspace->rejected : NULL,\n                &linesMutated, &spritesMutated, &reused);\n#else\n        const int automapOk = EspNativeBspVisibility_publishAutomap(\n                &workspace->visibility,\n                &linesMutated,\n                &spritesMutated);\n#endif\n        if (!automapOk) {",
            "        if (!EspNativeBspVisibility_publishAutomapWithRejected(\n                &workspace->visibility,\n                dropOrderCount == 0U && transientWorldBatch.active == 0U\n                    ? workspace->rejected : NULL,\n                &linesMutated, &spritesMutated, &reused)) {\n#else\n        if (!EspNativeBspVisibility_publishAutomap(\n                &workspace->visibility,\n                &linesMutated,\n                &spritesMutated)) {\n#endif"
        ]
    ]
}

P2O_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "#include \"esp_native_gameplay_player_pain_visual.h\"\n#include \"esp_native_gameplay_player_state.h\"",
            "#include \"esp_native_gameplay_player_pain_visual.h\""
        ],
        [
            "    if (!continuation)\n        EspPerfMoveVisibility_semanticBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs,\n            shadowAutomap, shadowActivation,\n            EspNativeGameplayPlayerState_fingerprint());\n    if (!continuation)\n        (void)EspPerfMoveVisibility_firstShadowBegin(",
            "    if (!continuation)\n        (void)EspPerfMoveVisibility_firstShadowBegin("
        ],
        [
            "        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(0, 0U, 0U, 1, 0, 0U, 0U);\n            EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);\n        }",
            "        if (!continuation) EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);"
        ],
        [
            "        const uint32_t afterActivation = EspNativeGameplayMonsterActivation_count();\n        const uint32_t afterPlayer = EspNativeGameplayPlayerState_fingerprint();\n        const int sideEffects = shadowMap == NULL || afterMap == NULL ||\n            shadowAutomap != afterMap->stateFNV1a || shadowActivation != afterActivation;\n        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects,\n                quietAfter, afterPlayer, frame.totalMicros);\n            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects, quietAfter);\n        }\n        EspPerfMoveVisibility_end(frame.finalPresented && quietAfter ? 1 : 2);\n        EspPerfMoveVisibility_rememberContext(\n            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);\n        EspPerfMoveVisibility_semanticRemember(\n            afterMap != NULL ? afterMap->stateFNV1a : 0U, afterActivation,\n            afterPlayer, frame.frameAfterFNV,\n            frame.finalPresented && quietAfter && shadowContextReady && afterMap != NULL);",
            "        if (!continuation)\n            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV,\n                shadowMap == NULL || afterMap == NULL ||\n                shadowAutomap != afterMap->stateFNV1a ||\n                shadowActivation != EspNativeGameplayMonsterActivation_count(),\n                quietAfter);\n        EspPerfMoveVisibility_end(frame.finalPresented && quietAfter ? 1 : 2);\n        EspPerfMoveVisibility_rememberContext(\n            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);"
        ]
    ]
}

P2P_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    /* Oriented geometry can depend on frame bounds: retain the full guard. */\n    if ((parent->info & ORIENT_MASK) != 0U)\n        EspPerfMoveVisibility_noteAmbientPhase(parent->info, entityType,\n            entitySubtype, parent->index, ambientSampleMs);\n#endif",
            "#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n    EspPerfMoveVisibility_noteAmbientPhase(parent->info, entityType,\n        entitySubtype, parent->index, ambientSampleMs);\n#endif"
        ],
        [
            "            if (glow) ++stats->glowNearCulled;\n            else ++stats->nearCulled;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n            EspPerfMoveVisibility_noteAmbientNearCull(parent->info,\n                entityType, ambientSampleMs);\n#endif\n            return 1;\n        }\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        /* P2p: center depth cannot depend on frame bounds or animation. */\n        EspPerfMoveVisibility_noteAmbientPhase(parent->info, entityType,\n            entitySubtype, parent->index, ambientSampleMs);\n#endif\n\n        line.vert1 = center;",
            "            if (glow) ++stats->glowNearCulled;\n            else ++stats->nearCulled;\n            return 1;\n        }\n\n        line.vert1 = center;"
        ],
        [
            "        ++stats->ambientCandidates;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n        EspPerfMoveVisibility_noteAmbientRasterPhase(parent->info, entityType,\n            entitySubtype, parent->index, ambientSampleMs);\n#endif\n        if (!glow && EspNativeAmbientSprite_fireEligible(",
            "        ++stats->ambientCandidates;\n        if (!glow && EspNativeAmbientSprite_fireEligible("
        ]
    ]
}

P2Q_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "#define RENDER_MODE_ADD 7U\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#ifndef DOOMRPG_PERF_NEAR_LOAD\n#define DOOMRPG_PERF_NEAR_LOAD 1\n#endif\n#endif\n",
            "#define RENDER_MODE_ADD 7U\n"
        ],
        [
            "    uint32_t ordinal, readUs, readCalls, loadUs, loadCalls, spanUs, spanCalls;\n    uint32_t nearLoadsAvoided;\n",
            "    uint32_t ordinal, readUs, readCalls, loadUs, loadCalls, spanUs, spanCalls;\n"
        ],
        [
            "    /* Project the billboard center once; its depth is independent of the\n     * decoded frame bounds. Oriented sprites still require the frame. */\n    if ((parent->info & ORIENT_MASK) == 0U) {\n        memset(&center, 0, sizeof(center));\n        center.x = spriteX;\n        center.y = spriteY;\n        EspNativeRenderGeometry_transformPoint(\n        render->viewCos_, render->viewSin_, render->viewSin,\n        render->viewCos, render->viewTransX, render->viewTransY,\n        &center.x, &center.y);\n        center.x -= 0x100000;\n#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n        /* P2q early decode rejection BEGIN */\n        if (center.x < 0x40000 && crossAnimation == NULL && !glow &&\n            logical < sources->spriteIds && logical < 256U &&\n            EspNativeGraphicsCatalog_findSprite(logical) != NULL &&\n            EspAssetPack_isMapFlashActive() && !EspAssetPack_isSourceProbeActive()) {\n            TRACE_SPRITE(parent->index, \"[SPRITETRACE] DRAW sprite=%u result=near-culled depth=%d animation=%u glow=%u\\n\", (unsigned int)parent->index, (unsigned int)center.x, (unsigned int)animation, (unsigned int)(glow != 0));\n            ++stats->nearCulled;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n            EspPerfMoveVisibility_noteAmbientNearCull(parent->info,\n                entityType, ambientSampleMs);\n#if DOOMRPG_PERF_SPRITE_SAMPLE\n            if (spritePerf.active) ++spritePerf.nearLoadsAvoided;\n#endif\n#endif\n            return 1;\n        }\n        /* P2q early decode rejection END */\n#endif\n    }\n    /* All four CROSS billboards use the same immutable logical frame and\n",
            "    /* All four CROSS billboards use the same immutable logical frame and\n"
        ],
        [
            "    if ((parent->info & ORIENT_MASK) == 0U) {\n        if (center.x < 0x40000) {",
            "    if ((parent->info & ORIENT_MASK) == 0U) {\n        memset(&center, 0, sizeof(center));\n        center.x = spriteX;\n        center.y = spriteY;\n        EspNativeRenderGeometry_transformPoint(\n        render->viewCos_, render->viewSin_, render->viewSin,\n        render->viewCos, render->viewTransX, render->viewTransY,\n        &center.x, &center.y);\n        center.x -= 0x100000;\n        if (center.x < 0x40000) {"
        ],
        [
            "        spritePerf.loadCalls=spritePerf.spanUs=spritePerf.spanCalls=0U;\n        spritePerf.nearLoadsAvoided=0U;\n",
            "        spritePerf.loadCalls=spritePerf.spanUs=spritePerf.spanCalls=0U;\n"
        ],
        [
            "            (unsigned)stats.glowPixels);\n        printf(\"[PERFGAMEP2Q] ordinal=%u preview=%u nearLoadsAvoided=%u mapLoadCalls=%u sample=1/16\\n\",\n            (unsigned)spritePerf.ordinal, (unsigned)preview,\n            (unsigned)spritePerf.nearLoadsAvoided, (unsigned)spritePerf.loadCalls);\n    }\n    spritePerf.active = 0U;",
            "            (unsigned)stats.glowPixels);\n    }\n    spritePerf.active = 0U;"
        ]
    ]
}

P2R_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "typedef struct FrameLocation_s {\n    uint32_t shapeOffset;\n    uint32_t paletteOffset;\n} FrameLocation;\n\n/* Bounds are sufficient for geometry; payload is needed only to rasterize. */\nstatic int loadFrameBounds(const Sources* sources, uint16_t logical,\n                           uint32_t animation, Frame* frame,\n                           FrameLocation* location,\n                           EspNativeSpriteStats* stats) {\n    uint8_t idBytes[2];\n    uint8_t pair[8];\n    uint8_t header[12];\n    uint32_t actual;\n    uint32_t shapeOffset;\n    int32_t sourceOffset;\n    int32_t paletteOffset;\n\n    if (logical >= sources->spriteIds || logical >= 256U ||\n        EspNativeGraphicsCatalog_findSprite(logical) == NULL) {\n        return 0;\n    }\n\n    memset(frame, 0, sizeof(*frame));\n    if (!readRange(&sources->mappings,\n                   sources->spriteIdBase + (uint32_t)logical * 2U,\n                   idBytes, sizeof(idBytes), stats)) {\n        return 0;\n    }\n\n    actual = (uint32_t)le16(idBytes) + animation;\n    if (actual >= sources->bitShapePairs || actual > UINT16_MAX ||\n        !readRange(&sources->mappings,\n                   sources->spritePairBase + actual * PAIR_BYTES,\n                   pair, sizeof(pair), stats)) {\n        return 0;\n    }\n\n    sourceOffset = (int32_t)le32(pair);\n    paletteOffset = (int32_t)le32(pair + 4U);\n    if (sourceOffset < 0 || paletteOffset < 0 ||\n        (uint32_t)paletteOffset > sources->paletteEntries ||\n        16U > sources->paletteEntries - (uint32_t)paletteOffset) {\n        return 0;\n    }\n\n    shapeOffset = TEXEL_HEADER + (uint32_t)sourceOffset;\n    if (!readRange(&sources->bitshapes, shapeOffset, header,\n                   sizeof(header), stats)) {\n        return 0;\n    }\n\n    frame->logical = logical;\n    frame->actual = (uint16_t)actual;\n    frame->texelOffset = le32(header);\n    frame->xMin = header[8];\n    frame->xMax = header[9];\n    frame->yMin = header[10];\n    frame->yMax = header[11];\n    if (frame->xMax < frame->xMin || frame->yMax < frame->yMin) return 0;\n\n    frame->width = frame->xMax - frame->xMin + 1;\n    frame->height = frame->yMax - frame->yMin + 1;\n    frame->pitch = (frame->height + 7) / 8;\n    if (frame->width <= 0 || frame->width > MAX_DIM ||\n        frame->height <= 0 || frame->height > MAX_DIM) {\n        return 0;\n    }\n\n    frame->maskBytes = (uint32_t)frame->width * (uint32_t)frame->pitch;\n    if (frame->maskBytes == 0U || frame->maskBytes > MAX_MASK) return 0;\n    location->shapeOffset = shapeOffset;\n    location->paletteOffset = (uint32_t)paletteOffset;\n    return 1;\n}\n\nstatic int loadFramePayload(const Sources* sources, int glow, Frame* frame,\n                            const FrameLocation* location,\n                            uint32_t seenLogical[8],\n                            EspNativeSpriteStats* stats) {\n    uint8_t palette[32];\n    uint32_t maskOffset, x, active = 0U, base, relative;\n    uint32_t spriteTexelOffset, p, loadedBytes;\n    const uint16_t logical = frame->logical;\n\n    if (!readRange(&sources->palettes,\n                   PALETTE_HEADER + location->paletteOffset * 2U,\n                   palette, sizeof(palette), stats)) return 0;\n    maskOffset = location->shapeOffset + SHAPE_HEADER;\n    if (!readRange(&sources->bitshapes, maskOffset, frame->mask,\n                   frame->maskBytes, stats)) {\n        return 0;\n    }\n\n    frame->prefix[0] = 0U;\n    for (x = 0U; x < (uint32_t)frame->width; ++x) {\n        const uint8_t* column = frame->mask + x * (uint32_t)frame->pitch;\n        active += EspNativeSpriteMask_countColumn(\n            column, (unsigned)frame->height);\n        frame->prefix[x + 1U] = (uint16_t)active;\n    }\n\n    frame->active = active;\n    frame->packedBytes = ((active + 1U) & ~1U) / 2U;\n    if (frame->packedBytes == 0U || frame->packedBytes > MAX_TEXELS) return 0;\n\n    base = sources->wallBytes * 2U;\n    if (frame->texelOffset < base ||\n        ((frame->texelOffset - base) & 1U) != 0U) {\n        return 0;\n    }\n    relative = frame->texelOffset - base;\n    spriteTexelOffset = TEXEL_HEADER + relative / 2U;\n    if (!readRange(&sources->stexels, spriteTexelOffset, frame->texels,\n                   frame->packedBytes, stats)) {\n        return 0;\n    }\n\n    for (p = 0U; p < 16U; ++p) {\n        frame->palette[p] = source565(le16(palette + p * 2U));\n    }\n\n    loadedBytes = frame->maskBytes + frame->packedBytes;\n    if (glow) {\n        ++stats->glowFrameLoads;\n        stats->glowFrameBytes += loadedBytes;\n        if (loadedBytes > stats->glowMaxFrameBytes) {\n            stats->glowMaxFrameBytes = loadedBytes;\n        }\n    }\n    else {\n        uint32_t word;\n        uint32_t bit;\n        ++stats->frameLoads;\n        stats->frameBytes += loadedBytes;\n        if (loadedBytes > stats->maxFrameBytes) stats->maxFrameBytes = loadedBytes;\n        word = (uint32_t)logical >> 5;\n        bit = 1U << ((uint32_t)logical & 31U);\n        if ((seenLogical[word] & bit) == 0U) {\n            seenLogical[word] |= bit;\n            ++stats->uniqueLogical;\n        }\n    }\n    return 1;\n}\n\nstatic int loadFrame(const Sources* sources,\n                     uint16_t logical,\n                     uint32_t animation,\n                     int glow,\n                     Frame* frame,\n                     uint32_t seenLogical[8],\n                     EspNativeSpriteStats* stats) {\n    FrameLocation location;\n    return loadFrameBounds(sources, logical, animation, frame, &location, stats) &&\n           loadFramePayload(sources, glow, frame, &location, seenLogical, stats);\n}\n\n",
            "static int loadFrame(const Sources* sources,\n                     uint16_t logical,\n                     uint32_t animation,\n                     int glow,\n                     Frame* frame,\n                     uint32_t seenLogical[8],\n                     EspNativeSpriteStats* stats) {\n    uint8_t idBytes[2];\n    uint8_t pair[8];\n    uint8_t header[12];\n    uint8_t palette[32];\n    uint32_t actual;\n    uint32_t shapeOffset;\n    uint32_t maskOffset;\n    int32_t sourceOffset;\n    int32_t paletteOffset;\n    uint32_t x;\n    uint32_t active = 0U;\n    uint32_t base;\n    uint32_t relative;\n    uint32_t spriteTexelOffset;\n    uint32_t p;\n    uint32_t loadedBytes;\n\n    if (logical >= sources->spriteIds || logical >= 256U ||\n        EspNativeGraphicsCatalog_findSprite(logical) == NULL) {\n        return 0;\n    }\n\n    memset(frame, 0, sizeof(*frame));\n    if (!readRange(&sources->mappings,\n                   sources->spriteIdBase + (uint32_t)logical * 2U,\n                   idBytes, sizeof(idBytes), stats)) {\n        return 0;\n    }\n\n    actual = (uint32_t)le16(idBytes) + animation;\n    if (actual >= sources->bitShapePairs || actual > UINT16_MAX ||\n        !readRange(&sources->mappings,\n                   sources->spritePairBase + actual * PAIR_BYTES,\n                   pair, sizeof(pair), stats)) {\n        return 0;\n    }\n\n    sourceOffset = (int32_t)le32(pair);\n    paletteOffset = (int32_t)le32(pair + 4U);\n    if (sourceOffset < 0 || paletteOffset < 0 ||\n        (uint32_t)paletteOffset > sources->paletteEntries ||\n        16U > sources->paletteEntries - (uint32_t)paletteOffset) {\n        return 0;\n    }\n\n    shapeOffset = TEXEL_HEADER + (uint32_t)sourceOffset;\n    if (!readRange(&sources->bitshapes, shapeOffset, header,\n                   sizeof(header), stats) ||\n        !readRange(&sources->palettes,\n                   PALETTE_HEADER + (uint32_t)paletteOffset * 2U,\n                   palette, sizeof(palette), stats)) {\n        return 0;\n    }\n\n    frame->logical = logical;\n    frame->actual = (uint16_t)actual;\n    frame->texelOffset = le32(header);\n    frame->xMin = header[8];\n    frame->xMax = header[9];\n    frame->yMin = header[10];\n    frame->yMax = header[11];\n    if (frame->xMax < frame->xMin || frame->yMax < frame->yMin) return 0;\n\n    frame->width = frame->xMax - frame->xMin + 1;\n    frame->height = frame->yMax - frame->yMin + 1;\n    frame->pitch = (frame->height + 7) / 8;\n    if (frame->width <= 0 || frame->width > MAX_DIM ||\n        frame->height <= 0 || frame->height > MAX_DIM) {\n        return 0;\n    }\n\n    frame->maskBytes = (uint32_t)frame->width * (uint32_t)frame->pitch;\n    if (frame->maskBytes == 0U || frame->maskBytes > MAX_MASK) return 0;\n    maskOffset = shapeOffset + SHAPE_HEADER;\n    if (!readRange(&sources->bitshapes, maskOffset, frame->mask,\n                   frame->maskBytes, stats)) {\n        return 0;\n    }\n\n    frame->prefix[0] = 0U;\n    for (x = 0U; x < (uint32_t)frame->width; ++x) {\n        const uint8_t* column = frame->mask + x * (uint32_t)frame->pitch;\n        active += EspNativeSpriteMask_countColumn(\n            column, (unsigned)frame->height);\n        frame->prefix[x + 1U] = (uint16_t)active;\n    }\n\n    frame->active = active;\n    frame->packedBytes = ((active + 1U) & ~1U) / 2U;\n    if (frame->packedBytes == 0U || frame->packedBytes > MAX_TEXELS) return 0;\n\n    base = sources->wallBytes * 2U;\n    if (frame->texelOffset < base ||\n        ((frame->texelOffset - base) & 1U) != 0U) {\n        return 0;\n    }\n    relative = frame->texelOffset - base;\n    spriteTexelOffset = TEXEL_HEADER + relative / 2U;\n    if (!readRange(&sources->stexels, spriteTexelOffset, frame->texels,\n                   frame->packedBytes, stats)) {\n        return 0;\n    }\n\n    for (p = 0U; p < 16U; ++p) {\n        frame->palette[p] = source565(le16(palette + p * 2U));\n    }\n\n    loadedBytes = frame->maskBytes + frame->packedBytes;\n    if (glow) {\n        ++stats->glowFrameLoads;\n        stats->glowFrameBytes += loadedBytes;\n        if (loadedBytes > stats->glowMaxFrameBytes) {\n            stats->glowMaxFrameBytes = loadedBytes;\n        }\n    }\n    else {\n        uint32_t word;\n        uint32_t bit;\n        ++stats->frameLoads;\n        stats->frameBytes += loadedBytes;\n        if (loadedBytes > stats->maxFrameBytes) stats->maxFrameBytes = loadedBytes;\n        word = (uint32_t)logical >> 5;\n        bit = 1U << ((uint32_t)logical & 31U);\n        if ((seenLogical[word] & bit) == 0U) {\n            seenLogical[word] |= bit;\n            ++stats->uniqueLogical;\n        }\n    }\n    return 1;\n}\n\n"
        ],
        [
            "    uint32_t nearLoadsAvoided, clipPayloadsAvoided, boundsUs, boundsCalls;\n",
            "    uint32_t nearLoadsAvoided;\n"
        ],
        [
            "    int spriteY;\n#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n    /* P2r declaration BEGIN */\n    FrameLocation deferredLocation;\n    int deferredPayload = 0;\n    /* P2r declaration END */\n#endif\n#if defined(DOOMRPG_ESP32_TRACE_SPRITE)\n",
            "    int spriteY;\n#if defined(DOOMRPG_ESP32_TRACE_SPRITE)\n"
        ],
        [
            "#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n        /* P2r bounds-only BEGIN */\n        if (crossAnimation == NULL && !glow &&\n            (parent->info & ORIENT_MASK) == 0U &&\n            center.x >= 0x40000 &&\n            EspAssetPack_isMapFlashActive() && !EspAssetPack_isSourceProbeActive()) {\n            if (!loadFrameBounds(sources, logical, animation, frame,\n                                 &deferredLocation, stats)) return 0;\n            deferredPayload = 1;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n            P2J_ACCUM(loadStart, boundsUs, boundsCalls);\n#endif\n        } else\n        /* P2r bounds-only END */\n#endif\n        if (!loadFrame(sources, logical, animation, glow,\n                       frame, seenLogical, stats)) {",
            "        if (!loadFrame(sources, logical, animation, glow,\n                       frame, seenLogical, stats)) {"
        ],
        [
            "#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n        if (!deferredPayload)\n#endif\n            P2J_ACCUM(loadStart, loadUs, loadCalls);\n#endif\n        if (crossAnimation != NULL) *crossAnimation = animation;",
            "        P2J_ACCUM(loadStart, loadUs, loadCalls);\n#endif\n        if (crossAnimation != NULL) *crossAnimation = animation;"
        ],
        [
            "    if (!EspNativeDynamicLineRender_clipLine(&line)) {\n#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n        /* P2r clip counter BEGIN */\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        if (deferredPayload && spritePerf.active) ++spritePerf.clipPayloadsAvoided;\n#endif\n        /* P2r clip counter END */\n#endif\n        TRACE_SPRITE(parent->index, \"[SPRITETRACE] DRAW sprite=%u result=clip-culled",
            "    if (!EspNativeDynamicLineRender_clipLine(&line)) {\n        TRACE_SPRITE(parent->index, \"[SPRITETRACE] DRAW sprite=%u result=clip-culled"
        ],
        [
            "#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n    /* P2r payload BEGIN */\n    if (deferredPayload) {\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_START(payloadStart);\n#endif\n        if (!loadFramePayload(sources, glow, frame, &deferredLocation,\n                              seenLogical, stats)) return 0;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_ACCUM(payloadStart, loadUs, loadCalls);\n#endif\n    }\n    /* P2r payload END */\n#endif\n    /* Source Render_renderSpriteObject() sets damageBlend while this monster's",
            "    /* Source Render_renderSpriteObject() sets damageBlend while this monster's"
        ],
        [
            "        spritePerf.nearLoadsAvoided=0U;\n        spritePerf.clipPayloadsAvoided=spritePerf.boundsUs=spritePerf.boundsCalls=0U;\n",
            "        spritePerf.nearLoadsAvoided=0U;\n"
        ],
        [
            "            (unsigned)spritePerf.nearLoadsAvoided, (unsigned)spritePerf.loadCalls);\n        printf(\"[PERFGAMEP2R] ordinal=%u clipPayloadsAvoided=%u boundsCalls=%u boundsUs=%u sample=1/16\\n\",\n            (unsigned)spritePerf.ordinal, (unsigned)spritePerf.clipPayloadsAvoided,\n            (unsigned)spritePerf.boundsCalls, (unsigned)spritePerf.boundsUs);\n    }",
            "            (unsigned)spritePerf.nearLoadsAvoided, (unsigned)spritePerf.loadCalls);\n    }"
        ]
    ]
}

P2S_WIRING_EDITS = {
    "esp_native_sprite_renderer.c": [
        [
            "#if defined(DOOMRPG_PERF_NEAR_LOAD) && DOOMRPG_PERF_NEAR_LOAD\n/* P2s: same projected integer column interval and scale as spans().\n * Only constant-depth billboards qualify. Unknown/degenerate geometry\n * takes the original checked decoder and raster path. No pixels read. */\nstatic unsigned fullyOccludedColumns(const Render_t* render,\n                                     const Line_t* line) {\n    int x, x2, scale;\n    if (line->vert2.x <= line->vert1.x ||\n        line->vert1.y <= 0 || line->vert1.y != line->vert2.y ||\n        line->vert1.y < 4) return 0U;\n    x = (line->vert1.x + 65535) >> 16;\n    x2 = (line->vert2.x + 65535) >> 16;\n    if (x < render->screenLeft) x = render->screenLeft;\n    if (x2 > render->screenRight) x2 = render->screenRight;\n    if (x < 0 || x2 > render->screenWidth || x >= x2) return 0U;\n    scale = (0x40000000 / line->vert1.y) << 2;\n    if (scale <= 0) return 0U;\n    for (int c = x; c < x2; ++c) {\n        /* Equality is drawable in spans(), never reject it. */\n        if (render->columnScale[c] >= scale) return 0U;\n    }\n    return (unsigned)(x2 - x);\n}\n\n#endif\nstatic int drawWorldDropFrame(Render_t* render,",
            "static int drawWorldDropFrame(Render_t* render,"
        ],
        [
            "uint32_t nearLoadsAvoided, clipPayloadsAvoided, boundsUs, boundsCalls, wallPayloadsAvoided;",
            "uint32_t nearLoadsAvoided, clipPayloadsAvoided, boundsUs, boundsCalls;"
        ],
        [
            "    if (deferredPayload) {\n        /* P2s: the existing depth buffer proves all columns invisible.\n         * Preserve the exact wall-occluded count and potential phase\n         * guard, while avoiding palette/mask/texels and span traversal. */\n        const unsigned occluded =\n            (renderMode == RENDER_MODE_NORMAL || renderMode == RENDER_MODE_ADD)\n            ? fullyOccludedColumns(render, &line) : 0U;\n        if (occluded != 0U) {\n            stats->wallOccludedColumns += occluded;\n            /* spans() succeeds even with zero drawable columns. Count\n             * this finished base object exactly as the original path. */\n            ++stats->draws;\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n            if (spritePerf.active) ++spritePerf.wallPayloadsAvoided;\n#endif\n            TRACE_SPRITE(parent->index, \"[SPRITETRACE] DRAW sprite=%u result=wall-occluded columns=%u\\n\",\n                (unsigned int)parent->index, occluded);\n            return 1;\n        }\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_START(payloadStart);",
            "    if (deferredPayload) {\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_PERF_SPRITE_SAMPLE\n        P2J_START(payloadStart);"
        ],
        [
            "spritePerf.clipPayloadsAvoided=spritePerf.boundsUs=spritePerf.boundsCalls=0U;\n        spritePerf.wallPayloadsAvoided=0U;",
            "spritePerf.clipPayloadsAvoided=spritePerf.boundsUs=spritePerf.boundsCalls=0U;"
        ],
        [
            "            (unsigned)spritePerf.boundsCalls, (unsigned)spritePerf.boundsUs);\n        printf(\"[PERFGAMEP2S] ordinal=%u wallPayloadsAvoided=%u sample=1/16\\n\",\n            (unsigned)spritePerf.ordinal, (unsigned)spritePerf.wallPayloadsAvoided);",
            "            (unsigned)spritePerf.boundsCalls, (unsigned)spritePerf.boundsUs);"
        ]
    ]
}

P2T_WIRING_EDITS = {
    "esp_native_first_frame.c": [
        [
            "            if (actual >= texelPairs) goto done;\n#if DOOMRPG_PERF_WALL_GUARD_BATCH\n            if (batch) {\n                sourceOffset = (int32_t)readLe32(\n                    phasePairs + phase * MAPPING_PAIR_BYTES);\n            }\n            else\n#endif\n            {\n                if (!EspAssetPack_readRange(\n                        &work->mappings,\n                        MAPPINGS_HEADER_BYTES + actual * MAPPING_PAIR_BYTES,\n                        pair,\n                        sizeof(pair))) {\n                    goto done;\n                }\n                sourceOffset = (int32_t)readLe32(pair);\n            }",
            "            if (actual >= texelPairs) goto done;\n            if (!EspAssetPack_readRange(\n                    &work->mappings,\n                    MAPPINGS_HEADER_BYTES + actual * MAPPING_PAIR_BYTES,\n                    pair,\n                    sizeof(pair))) {\n                goto done;\n            }\n            sourceOffset = (int32_t)readLe32(pair);"
        ],
        [
            "        baseActual = readLe16(idBytes);\n#if DOOMRPG_PERF_WALL_GUARD_BATCH\n        if (batch) {\n            if (baseActual >= texelPairs || texelPairs - baseActual < 4U)\n                goto done;\n            if (!EspAssetPack_readRange(\n                    &work->mappings,\n                    MAPPINGS_HEADER_BYTES + baseActual * MAPPING_PAIR_BYTES,\n                    phasePairs, sizeof(phasePairs))) goto done;\n        }\n#endif\n\n        for (phase = 0U; phase < 4U; ++phase) {",
            "        baseActual = readLe16(idBytes);\n\n        for (phase = 0U; phase < 4U; ++phase) {"
        ],
        [
            "    uint8_t pair[MAPPING_PAIR_BYTES];\n#if DOOMRPG_PERF_WALL_GUARD_BATCH\n    /* Four consecutive pairs are one exact contiguous mappings range.\n     * Keep the original per-frame guard lifetime; no persistent cache. */\n    uint8_t phasePairs[4U * MAPPING_PAIR_BYTES];\n    const int batch = EspAssetPack_isMapFlashActive() &&\n                      !EspAssetPack_isSourceProbeActive();\n#endif\n    uint8_t packedByte = 0U;",
            "    uint8_t pair[MAPPING_PAIR_BYTES];\n    uint8_t packedByte = 0U;"
        ],
        [
            "#define MAPPING_PAIR_BYTES 8U\n#ifndef DOOMRPG_PERF_WALL_GUARD_BATCH\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#define DOOMRPG_PERF_WALL_GUARD_BATCH 1\n#else\n#define DOOMRPG_PERF_WALL_GUARD_BATCH 0\n#endif\n#endif\n",
            "#define MAPPING_PAIR_BYTES 8U\n"
        ]
    ]
}

P2_CLEAN_WIRING_EDITS = {
    "esp_native_gameplay_monster_movement_publish.c": [
        [
            "#if DOOMRPG_PERF_P2_SHADOW\n        EspPerfMoveVisibility_rememberContext(\n            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);\n        EspPerfMoveVisibility_semanticRemember(\n            afterMap != NULL ? afterMap->stateFNV1a : 0U, afterActivation,\n            afterPlayer, frame.frameAfterFNV,\n            frame.finalPresented && quietAfter && shadowContextReady && afterMap != NULL);\n#endif\n",
            "        EspPerfMoveVisibility_rememberContext(\n            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);\n        EspPerfMoveVisibility_semanticRemember(\n            afterMap != NULL ? afterMap->stateFNV1a : 0U, afterActivation,\n            afterPlayer, frame.frameAfterFNV,\n            frame.finalPresented && quietAfter && shadowContextReady && afterMap != NULL);\n"
        ],
        [
            "#if DOOMRPG_PERF_P2_SHADOW\n        const EspMapAutomapStateView* afterMap = EspMapAutomapState_view();\n        const uint32_t afterActivation = EspNativeGameplayMonsterActivation_count();\n        const uint32_t afterPlayer = EspNativeGameplayPlayerState_fingerprint();\n        const int sideEffects = shadowMap == NULL || afterMap == NULL ||\n            shadowAutomap != afterMap->stateFNV1a || shadowActivation != afterActivation;\n        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects,\n                quietAfter, afterPlayer, frame.totalMicros);\n            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects, quietAfter);\n        }\n#endif\n",
            "        const EspMapAutomapStateView* afterMap = EspMapAutomapState_view();\n        const uint32_t afterActivation = EspNativeGameplayMonsterActivation_count();\n        const uint32_t afterPlayer = EspNativeGameplayPlayerState_fingerprint();\n        const int sideEffects = shadowMap == NULL || afterMap == NULL ||\n            shadowAutomap != afterMap->stateFNV1a || shadowActivation != afterActivation;\n        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects,\n                quietAfter, afterPlayer, frame.totalMicros);\n            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,\n                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects, quietAfter);\n        }\n"
        ],
        [
            "#if DOOMRPG_PERF_P2_SHADOW\n        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(0, 0U, 0U, 1, 0, 0U, 0U);\n            EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);\n        }\n#endif\n",
            "        if (!continuation) {\n            (void)EspPerfMoveVisibility_semanticEnd(0, 0U, 0U, 1, 0, 0U, 0U);\n            EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);\n        }\n"
        ],
        [
            "#if DOOMRPG_PERF_P2_SHADOW\n    const EspMapRuntimeView* shadowRuntime = EspMapRuntime_view();\n    const EspMapLineStateView* shadowLines = EspMapLineState_viewBase();\n    const EspMapLineTextureStateView* shadowTextures = EspMapLineTextureState_view();\n    const EspMapAutomapStateView* shadowMap = EspMapAutomapState_view();\n    const uint32_t shadowActivation = EspNativeGameplayMonsterActivation_count();\n    const uint32_t shadowAutomap = shadowMap != NULL ? shadowMap->stateFNV1a : 0U;\n    EspPerfMoveVisibilityContext shadowContext;\n    memset(&shadowContext, 0, sizeof(shadowContext));\n    const int shadowContextReady = shadowRuntime != NULL &&\n        shadowLines != NULL && shadowTextures != NULL;\n    if (shadowContextReady) {\n        shadowContext.arena = shadowRuntime->arenaFNV1a;\n        shadowContext.lineFNV = shadowLines->stateFNV1a;\n        shadowContext.textureFNV = shadowTextures->stateFNV1a;\n        shadowContext.x = player->viewX;\n        shadowContext.y = player->viewY;\n        shadowContext.z = player->viewZ;\n        shadowContext.angle = player->viewAngle;\n    }\n    if (!continuation)\n        EspPerfMoveVisibility_semanticBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs,\n            shadowAutomap, shadowActivation,\n            EspNativeGameplayPlayerState_fingerprint());\n    if (!continuation)\n        (void)EspPerfMoveVisibility_firstShadowBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs);\n#endif\n",
            "    const EspMapRuntimeView* shadowRuntime = EspMapRuntime_view();\n    const EspMapLineStateView* shadowLines = EspMapLineState_viewBase();\n    const EspMapLineTextureStateView* shadowTextures = EspMapLineTextureState_view();\n    const EspMapAutomapStateView* shadowMap = EspMapAutomapState_view();\n    const uint32_t shadowActivation = EspNativeGameplayMonsterActivation_count();\n    const uint32_t shadowAutomap = shadowMap != NULL ? shadowMap->stateFNV1a : 0U;\n    EspPerfMoveVisibilityContext shadowContext;\n    memset(&shadowContext, 0, sizeof(shadowContext));\n    const int shadowContextReady = shadowRuntime != NULL &&\n        shadowLines != NULL && shadowTextures != NULL;\n    if (shadowContextReady) {\n        shadowContext.arena = shadowRuntime->arenaFNV1a;\n        shadowContext.lineFNV = shadowLines->stateFNV1a;\n        shadowContext.textureFNV = shadowTextures->stateFNV1a;\n        shadowContext.x = player->viewX;\n        shadowContext.y = player->viewY;\n        shadowContext.z = player->viewZ;\n        shadowContext.angle = player->viewAngle;\n    }\n    if (!continuation)\n        EspPerfMoveVisibility_semanticBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs,\n            shadowAutomap, shadowActivation,\n            EspNativeGameplayPlayerState_fingerprint());\n    if (!continuation)\n        (void)EspPerfMoveVisibility_firstShadowBegin(\n            shadowContextReady ? &shadowContext : NULL, blockers,\n            publishCapture.before.spriteIndex,\n            publishCapture.before.worldX, publishCapture.before.worldY,\n            publishCapture.after.worldX, publishCapture.after.worldY, nowMs);\n"
        ]
    ]
}

def restore_pre_perf_game_p2a(name, data):
    if name == "esp_native_first_frame.c":
        block = "    /* P3b: prove the entire forward span stays inside this packed\n     * texture. The affine recurrence is monotonic, so the endpoints\n     * suffice. Keep short, reverse and boundary spans on the original\n     * path, including legacy successor I/O and partial failure stats. */\n    if (pixelCount >= 4 && texelPosition >= 0 && texelStep >= 0 &&\n        (int64_t)texelPosition + (int64_t)(pixelCount - 1) * texelStep <\n            ((int64_t)WALL_PACKED_BYTES << 13)) {\n        uint32_t pos = (uint32_t)texelPosition;\n        int left = pixelCount;\n        while (left-- > 0) {\n            const uint8_t packed = texels[pos >> 13];\n            *pixels = source->paletteRgb565[\n                (packed >> ((pos >> 10) & 4U)) & 15U];\n            pixels += pitch;\n            pos += (uint32_t)texelStep;\n        }\n        work->pixelsDrawn += (uint32_t)pixelCount;\n        return 1;\n    }\n\n".encode("utf-8")
        assert data.count(block) == 1, (name, "P3b wall span inverse")
        data = data.replace(block, b"", 1)
    for newer, older in {"esp_native_first_frame.c":[["        wallGuardMemo.textureEpoch = textureEpoch;","        wallGuardMemo.count = catalog->textureCount;\n        for (i = 0U; i < catalog->textureCount; ++i)\n            wallGuardMemo.ids[i] = (uint8_t)catalog->textures[i].resourceId;"],["        legacyWallGuards[legacyWallGuardCount++] = wallGuardMemo.guard;\n        return 1;","        for (i = 0U; i < catalog->textureCount; ++i)\n            if (wallGuardMemo.ids[i] != catalog->textures[i].resourceId) break;\n        if (i == catalog->textureCount) {\n            legacyWallGuards[legacyWallGuardCount++] = wallGuardMemo.guard;\n            return 1;\n        }"],["    const uint32_t textureEpoch = EspNativeGraphicsCatalog_textureEpoch();\n    const int memoEligible = epoch != 0U && textureEpoch != 0U;\n    if (memoEligible && wallGuardMemo.epoch == epoch &&\n        wallGuardMemo.textureEpoch == textureEpoch &&","    int memoEligible = epoch != 0U && catalog->textureCount <= WALL_MEMO_TEXTURE_LIMIT;\n    for (i = 0U; memoEligible && i < catalog->textureCount; ++i)\n        if (catalog->textures[i].resourceId > UINT8_MAX) memoEligible = 0;\n    if (memoEligible && wallGuardMemo.epoch == epoch &&\n        wallGuardMemo.count == catalog->textureCount &&"],["/* One successful immutable flash result. Nonrecycled texture-owner\n * epoch proves exact ordered texture identity without a duplicate ID list. */\nstatic struct {\n    LegacyWallGuard guard;\n    uint32_t epoch, textureEpoch, mappingsSize, wallSize;\n} wallGuardMemo;","/* One successful immutable flash result. Exact ordered texture ID witness\n * (no hash collision, no retained catalog pointer), bounded to 64 IDs. */\n#define WALL_MEMO_TEXTURE_LIMIT 64U\nstatic struct {\n    LegacyWallGuard guard;\n    uint32_t epoch, mappingsSize, wallSize;\n    uint16_t count;\n    uint8_t ids[WALL_MEMO_TEXTURE_LIMIT];\n} wallGuardMemo;"]],"esp_native_graphics_catalog.c":[["    if (textureEpoch != UINT32_MAX) ++textureEpoch;\n    catalogView.textures = catalogArena;\n    catalogView.sprites = catalogArena + textureCount;","    catalogView.textures = catalogArena;\n    catalogView.sprites = catalogArena + textureCount;"],["uint32_t EspNativeGraphicsCatalog_textureEpoch(void) {\n    return EspNativeGraphicsCatalog_isReady() && textureEpoch != UINT32_MAX\n        ? textureEpoch : 0U;\n}\n\nconst EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {","const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {"],["void EspNativeGraphicsCatalog_reset(void) {\n    if (textureEpoch != UINT32_MAX) ++textureEpoch;\n    free(catalogArena);","void EspNativeGraphicsCatalog_reset(void) {\n    free(catalogArena);"],["static EspNativeGraphicsCatalogView catalogView;\n/* Texture records are immutable after build. Sprite-only expansions copy\n * the entire ordered texture prefix unchanged. Never recycle a generation. */\nstatic uint32_t textureEpoch = 1U;","static EspNativeGraphicsCatalogView catalogView;"]],"esp_native_graphics_catalog.h":[["/* Nonzero unique ordered immutable texture generation while ready.\n * Sprite-only growth retains it; reset/build invalidates it; saturation\n * returns zero forever rather than aliasing a previously issued token. */\nuint32_t EspNativeGraphicsCatalog_textureEpoch(void);\nint EspNativeGraphicsCatalog_isReady(void);","int EspNativeGraphicsCatalog_isReady(void);"]]}.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2Y compact memo inverse")
        data = data.replace(a, b, 1)
    for newer, older in {"esp_native_gameplay_frame.c":[["static uint32_t fnv1aUpdate(uint32_t hash, const void* data, uint32_t bytes) {\n    const uint8_t* p = (const uint8_t*)data;\n    if (p == NULL && bytes != 0U) return 0U;\n    /* Byte-exact FNV-1a; no alignment assumption or speculative tail load.\n     * Unroll the hot framebuffer walk, retaining every multiply and byte. */\n    while (bytes >= 8U) {\n        hash = (hash ^ p[0]) * 16777619U;\n        hash = (hash ^ p[1]) * 16777619U;\n        hash = (hash ^ p[2]) * 16777619U;\n        hash = (hash ^ p[3]) * 16777619U;\n        hash = (hash ^ p[4]) * 16777619U;\n        hash = (hash ^ p[5]) * 16777619U;\n        hash = (hash ^ p[6]) * 16777619U;\n        hash = (hash ^ p[7]) * 16777619U;\n        p += 8U;\n        bytes -= 8U;\n    }\n    while (bytes != 0U) {\n        hash = (hash ^ *p++) * 16777619U;\n        --bytes;\n    }\n    return hash;\n}","static uint32_t fnv1aUpdate(uint32_t hash, const void* data, uint32_t bytes) {\n    const uint8_t* p = (const uint8_t*)data;\n    uint32_t i;\n    if (p == NULL && bytes != 0U) return 0U;\n    for (i = 0U; i < bytes; ++i) {\n        hash ^= p[i];\n        hash *= 16777619U;\n    }\n    return hash;\n}"]],"esp_native_first_frame.c":[["static uint32_t fnv1a32(const void* data, uint32_t length) {\n    const uint8_t* p = (const uint8_t*)data;\n    uint32_t hash = 2166136261U;\n    if (p == NULL && length != 0U) return 0U;\n    /* Byte-exact FNV-1a; no alignment assumption or speculative tail load.\n     * Unroll the hot framebuffer walk, retaining every multiply and byte. */\n    while (length >= 8U) {\n        hash = (hash ^ p[0]) * 16777619U;\n        hash = (hash ^ p[1]) * 16777619U;\n        hash = (hash ^ p[2]) * 16777619U;\n        hash = (hash ^ p[3]) * 16777619U;\n        hash = (hash ^ p[4]) * 16777619U;\n        hash = (hash ^ p[5]) * 16777619U;\n        hash = (hash ^ p[6]) * 16777619U;\n        hash = (hash ^ p[7]) * 16777619U;\n        p += 8U;\n        length -= 8U;\n    }\n    while (length != 0U) {\n        hash = (hash ^ *p++) * 16777619U;\n        --length;\n    }\n    return hash;\n}","static uint32_t fnv1a32(const void* data, uint32_t length) {\n    const uint8_t* p = (const uint8_t*)data;\n    uint32_t hash = 2166136261U;\n    uint32_t i;\n    if (p == NULL && length != 0U) return 0U;\n    for (i = 0U; i < length; ++i) {\n        hash ^= p[i];\n        hash *= 16777619U;\n    }\n    return hash;\n}"]]}.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2X FNV inverse")
        data = data.replace(a, b, 1)
    if name == "esp_native_first_frame.c":
        for newer, older in [["#if DOOMRPG_PERF_WALL_GUARD_MEMO\n    memset(&wallGuardMemo, 0, sizeof(wallGuardMemo));\n#endif\n    frameScratchLogged = 0U;\n}","    frameScratchLogged = 0U;\n}"],["    legacyWallGuards[legacyWallGuardCount].successorSourceOffset = nextSource;\n#if DOOMRPG_PERF_WALL_GUARD_MEMO\n    if (memoEligible) {\n        wallGuardMemo.guard = legacyWallGuards[legacyWallGuardCount];\n        wallGuardMemo.epoch = epoch;\n        wallGuardMemo.mappingsSize = work->mappings.size;\n        wallGuardMemo.wallSize = work->wallTexels.size;\n        wallGuardMemo.count = catalog->textureCount;\n        for (i = 0U; i < catalog->textureCount; ++i)\n            wallGuardMemo.ids[i] = (uint8_t)catalog->textures[i].resourceId;\n    }\n#endif\n    printf(","    legacyWallGuards[legacyWallGuardCount].successorSourceOffset = nextSource;\n    printf("],["#if DOOMRPG_PERF_WALL_GUARD_MEMO\n    const uint32_t epoch = EspAssetPack_verifiedFlashLeaseEpoch();\n    int memoEligible = epoch != 0U && catalog->textureCount <= WALL_MEMO_TEXTURE_LIMIT;\n    for (i = 0U; memoEligible && i < catalog->textureCount; ++i)\n        if (catalog->textures[i].resourceId > UINT8_MAX) memoEligible = 0;\n    if (memoEligible && wallGuardMemo.epoch == epoch &&\n        wallGuardMemo.count == catalog->textureCount &&\n        wallGuardMemo.mappingsSize == work->mappings.size &&\n        wallGuardMemo.wallSize == work->wallTexels.size &&\n        wallGuardMemo.guard.valid == 1U &&\n        wallGuardMemo.guard.logicalId == source->logicalId &&\n        wallGuardMemo.guard.actualId == source->actualId &&\n        wallGuardMemo.guard.sourceOffset == source->sourceTexelOffset) {\n        for (i = 0U; i < catalog->textureCount; ++i)\n            if (wallGuardMemo.ids[i] != catalog->textures[i].resourceId) break;\n        if (i == catalog->textureCount) {\n            legacyWallGuards[legacyWallGuardCount++] = wallGuardMemo.guard;\n            return 1;\n        }\n    }\n#endif\n    if (!EspAssetPack_readRange(&work->mappings, 0U, mappingHeader,\n                                sizeof(mappingHeader))) {","    if (!EspAssetPack_readRange(&work->mappings, 0U, mappingHeader,\n                                sizeof(mappingHeader))) {"],["static uint8_t legacyWallGuardCount;\n#if DOOMRPG_PERF_WALL_GUARD_MEMO\n/* One successful immutable flash result. Exact ordered texture ID witness\n * (no hash collision, no retained catalog pointer), bounded to 64 IDs. */\n#define WALL_MEMO_TEXTURE_LIMIT 64U\nstatic struct {\n    LegacyWallGuard guard;\n    uint32_t epoch, mappingsSize, wallSize;\n    uint16_t count;\n    uint8_t ids[WALL_MEMO_TEXTURE_LIMIT];\n} wallGuardMemo;\n#endif","static uint8_t legacyWallGuardCount;"],["#ifndef DOOMRPG_PERF_WALL_GUARD_MEMO\n#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n#define DOOMRPG_PERF_WALL_GUARD_MEMO 1\n#else\n#define DOOMRPG_PERF_WALL_GUARD_MEMO 0\n#endif\n#endif\n#define PALETTES_HEADER_BYTES 4U","#define PALETTES_HEADER_BYTES 4U"]]:
            a, b = newer.encode("utf-8"), older.encode("utf-8")
            assert data.count(a) == 1, (name, "P2W memo inverse")
            data = data.replace(a, b, 1)
    if name == "esp_native_resident_gameplay.c":
        lamp = "/* Physical busy lamp is a presentation-only consumer of the actual combat\n * input lock. Never borrow the HUD of a full-screen modal. */\nint EspNativeResidentGameplay_turnLampWorld(void) {\n    return EspNativeResidentGameplay_isActive() &&\n           !gameplayState.exitRequested && !automapActive() &&\n           !EspNativeGameplayPlayerDeath_isActive() &&\n           !EspNativeGameplayLevelUp_isActive() &&\n           !EspNativeGameplayTransition_isWaitingStats() &&\n           !EspNativeTransitionPresentation_isLoadingActive() &&\n           !EspNativeGameplayHub_isActive() &&\n           !EspNativeGameplayDialog_isActive() &&\n           !EspNativeGameplayPassword_isActive() &&\n           !EspNativeGameplayPassword_hasPendingCompletion() &&\n           !EspNativeGameplayStore_isActive() &&\n           !EspNativeGameplayStore_closePending() &&\n           !EspNativeGameplaySave_mainSelectorActive() &&\n           !EspNativeGameplaySave_slotSelectorActive() &&\n           !EspNativeGameplayEventChain_isWaiting();\n}\n\nint EspNativeResidentGameplay_turnLampBusy(void) {\n    return EspNativeGameplayMonsterAttackVisual_isBusy() ||\n           EspNativeGameplayMonsterActiveSequence_isBusy();\n}\n\n".encode("utf-8")
        assert data.count(lamp) == 1, (name, "P2V physical lamp inverse")
        data = data.replace(lamp, b"", 1)
    for newer, older in P2_CLEAN_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2-CLEAN wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2T_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2t wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2S_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2s wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2R_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2r wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2Q_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2q wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2P_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2p wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2O_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2o wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2N_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2n wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2L_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2l wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2K_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2k wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2J_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2j wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2I_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2i wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2H_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2h wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2G_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2g wiring inverse")
        data = data.replace(a, b, 1)
    for newer, older in P2F_WIRING_EDITS.get(name, []):
        a, b = newer.encode("utf-8"), older.encode("utf-8")
        assert data.count(a) == 1, (name, "P2f wiring inverse")
        data = data.replace(a, b, 1)
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
