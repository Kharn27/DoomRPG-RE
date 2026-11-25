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

def restore_pre_perf_game_p2a(name, data):
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
