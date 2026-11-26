#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "esp_native_root_state.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "esp_native_process_rng_owner.h"

#include "doomrpg_log.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_frame.h"
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
#include "esp_perf_move_visibility.h"
#endif
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
#include "esp_asset_pack.h"
#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_audio_entrance_music.h"
#include "esp_native_door_animator.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_gib_fx.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_level_up.h"
#include "esp_native_gameplay_monster_attack_visual.h"
#include "esp_native_gameplay_move_events.h"
#include "esp_native_gameplay_pickup_miniature.h"
#include "esp_native_gameplay_player_pain_visual.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_present_gate.h"
#include "esp_native_gameplay_transition.h"
#include "esp_native_transition_presentation.h"
#include "esp_native_resident_gameplay.h"
#include "esp_native_sprite_renderer.h"
#include <esp_timer.h>
#endif
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_player_view_state.h"

#define PUBLISH_MOVE_TILE_SIZE 64
#define PUBLISH_MOVE_WEAPON_COUNT 19U
#define PUBLISH_MOVE_SPECIAL_AI 10U
#define PUBLISH_PROJECT_WORDS \
    ((ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT + 31U) / 32U)

typedef struct MovementPublishCapture_s {
    EspNativeGameplayMonsterPositionRecord before;
    EspNativeGameplayMonsterPositionRecord after;
    uint8_t active;
    uint8_t reserved[3];
} MovementPublishCapture;

static MovementPublishCapture publishCapture;
static uint32_t projectedBits[PUBLISH_PROJECT_WORDS];

/* Exact CombatEntity.c subtype -> primary/alternate weapon table. */
static const uint8_t monsterAttacks[28] = {
    2U, 3U, 12U, 13U, 4U, 4U, 15U, 12U, 13U, 14U, 13U, 12U, 15U, 13U,
    15U, 14U, 7U, 12U, 7U, 3U, 15U, 15U, 16U, 17U, 7U, 17U, 12U, 13U
};

/* Only rangeMin is needed to recover the exact aiThink RNG-call count. */
static const uint8_t monsterRangeMin[PUBLISH_MOVE_WEAPON_COUNT] = {
    0U, 0U, 5U, 2U, 3U, 0U, 0U, 8U, 0U, 0U,
    0U, 0U, 0U, 0U, 0U, 3U, 3U, 2U, 0U
};

static const uint8_t monsterWeaponValid[PUBLISH_MOVE_WEAPON_COUNT] = {
    0U, 0U, 1U, 1U, 1U, 0U, 0U, 1U, 0U, 0U,
    0U, 0U, 1U, 1U, 1U, 1U, 1U, 1U, 0U
};

static int projectionRecordIndex(uint16_t spriteIndex, uint32_t* outIndex) {
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    uint32_t lo = 0U, hi;

    if (outIndex != NULL) *outIndex = 0U;
    if (monsters == NULL || monsters->records == NULL || outIndex == NULL ||
        monsters->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) {
        return 0;
    }
    /* MonsterState is strictly sprite-index ordered on construction and
     * validated restore. The ordinal remains the original record index. */
    hi = monsters->count;
    while (lo < hi) {
        const uint32_t mid = lo + ((hi - lo) >> 1);
        const uint16_t current = monsters->records[mid].spriteIndex;
        if (current == spriteIndex) {
            *outIndex = mid;
            return 1;
        }
        if (current < spriteIndex) lo = mid + 1U;
        else hi = mid;
    }
    return 0;
}

static void setProjected(uint16_t spriteIndex, int projected) {
    uint32_t index;
    uint32_t word;
    uint32_t mask;
    if (!projectionRecordIndex(spriteIndex, &index)) return;
    word = index >> 5;
    mask = 1UL << (index & 31U);
    if (projected) projectedBits[word] |= mask;
    else projectedBits[word] &= ~mask;
}

int EspNativeGameplayMonsterMovementPublish_isProjected(uint16_t spriteIndex) {
    uint32_t index;
    uint32_t word;
    uint32_t mask;
    if (!projectionRecordIndex(spriteIndex, &index)) return 0;
    word = index >> 5;
    mask = 1UL << (index & 31U);
    return (projectedBits[word] & mask) != 0U;
}

int EspNativeGameplayMonsterMovementPublish_adoptCheckpointPositions(void) {
    const EspNativeGameplayMonsterPositionView* positions =
        EspNativeGameplayMonsterPosition_view();
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;
    uint32_t projected = 0U;

    memset(projectedBits, 0, sizeof(projectedBits));
    if (runtime == NULL || monsters == NULL || positions == NULL ||
        positions->records == NULL || monsters->records == NULL ||
        positions->count != monsters->count ||
        positions->sourceArenaFNV1a != runtime->arenaFNV1a ||
        monsters->sourceArenaFNV1a != runtime->arenaFNV1a) {
        return 0;
    }

    for (i = 0U; i < positions->count; ++i) {
        const EspNativeGameplayMonsterPositionRecord* position =
            &positions->records[i];
        EspMapSprite raw;

        if (position->spriteIndex >= runtime->mapSpriteCount ||
            !EspMapRuntime_getMapSprite(position->spriteIndex, &raw)) {
            memset(projectedBits, 0, sizeof(projectedBits));
            return 0;
        }

        /*
         * With the projection bitset cleared, the sprite overlay returns the
         * immutable BSP coordinates here. A monster that was moved in the saved
         * session therefore differs from raw x/y and must be projected from
         * MonsterPosition on the very first resume frame. If it moved away and
         * later returned exactly to spawn, projection is visually equivalent
         * and the next committed movement will set the bit again.
         */
        if ((uint16_t)raw.x != position->worldX ||
            (uint16_t)raw.y != position->worldY) {
            setProjected(position->spriteIndex, 1);
            ++projected;
        }
    }

    printf("[MONSTERMOVELIVE] CHECKPOINT-PROJECTION arena=%08x monsters=%u projected=%u source=restored-position-v9 inference=raw-bsp-delta firstFrame=exact\n",
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)positions->count,
           (unsigned int)projected);
    return 1;
}

void EspNativeGameplayMonsterMovementPublish_beginCycle(void) {
    memset(&publishCapture, 0, sizeof(publishCapture));
}


int EspNativeGameplayMonsterMovementPublish_afterProbe(
    struct DoomRPG_s* doomRpgBase,
    const char* trigger,
    const struct Random_s* boundarySavedBase,
    uint8_t boundaryPrepared,
    uint32_t plannedMovesBefore,
    EspNativeGameplayMonsterMovementPublishResult* outResult) {
    return EspNativeGameplayMonsterMovementPublish_afterProbeWithView(
        doomRpgBase,
        trigger,
        boundarySavedBase,
        boundaryPrepared,
        plannedMovesBefore,
        EspNativeGameplayMonsterMovement_view(),
        outResult);
}

void EspNativeGameplayMonsterMovementPublish_reset(void) {
    EspNativeGameplayMonsterMovementPublish_beginCycle();
    memset(projectedBits, 0, sizeof(projectedBits));
}

void EspNativeGameplayMonsterMovementPublish_capturePrepared(
    const EspNativeGameplayMonsterPositionRecord* before,
    const EspNativeGameplayMonsterPositionRecord* after) {
    if (before == NULL || after == NULL ||
        before->spriteIndex != after->spriteIndex ||
        before->tileIndex == after->tileIndex) {
        return;
    }
    publishCapture.before = *before;
    publishCapture.after = *after;
    publishCapture.active = 1U;
}

static int movementRngCalls(const char* trigger,
                            const EspNativeGameplayMonsterRecord* monster,
                            const EspPlayerViewState* player,
                            const EspNativeGameplayMonsterPositionRecord* before,
                            uint8_t* outCalls) {
    uint8_t weaponId;
    uint32_t calls = 1U; /* aiGoal_MOVE visit choice */
    int i7;
    int64_t dx;
    int64_t dy;
    int64_t distance;
    int64_t closeDistance;

    if (outCalls != NULL) *outCalls = 0U;
    if (trigger == NULL || monster == NULL || player == NULL || before == NULL ||
        outCalls == NULL || monster->subtype >= 14U ||
        monster->subtype == PUBLISH_MOVE_SPECIAL_AI) {
        return 0;
    }

    weaponId = monsterAttacks[(uint32_t)monster->subtype * 2U +
                              (monster->alternateAttack != 0U ? 1U : 0U)];
    if (weaponId >= PUBLISH_MOVE_WEAPON_COUNT ||
        monsterWeaponValid[weaponId] == 0U) {
        return 0;
    }

    i7 = (1 + (int)monsterRangeMin[weaponId]) / 2;
    if (strcmp(trigger, "RANGED-AI") == 0) {
        if (i7 == 0) return 0;
        ++calls; /* aiThink rand >= 217 movement decision */
    }
    else if (strcmp(trigger, "NO-IMMEDIATE-ATTACK") != 0) {
        return 0;
    }

    dx = (int64_t)before->worldX - player->destX;
    dy = (int64_t)before->worldY - player->destY;
    distance = dx * dx + dy * dy;
    closeDistance = (int64_t)(i7 * PUBLISH_MOVE_TILE_SIZE) *
                    (int64_t)(i7 * PUBLISH_MOVE_TILE_SIZE);
    if (distance <= closeDistance &&
        (before->worldX == (uint16_t)player->destX ||
         before->worldY == (uint16_t)player->destY)) {
        ++calls; /* close-bias rand < 38 */
    }

    if (calls == 0U || calls > 3U) return 0;
    *outCalls = (uint8_t)calls;
    return 1;
}

static int restorePublishedOwners(
    const EspMapSpriteTopologyRelink* relink,
    const EspNativeGameplayMonsterPositionRecord* before,
    const EspNativeGameplayMonsterPositionRecord* after,
    int projectedBefore) {
    int topologyExact;
    int positionExact;

    setProjected(before->spriteIndex, projectedBefore);
    topologyExact = EspMapSpriteTopology_rollbackPreparedRelink(relink);
    positionExact = EspNativeGameplayMonsterPosition_rollbackPrepared(after, before);
    return topologyExact && positionExact;
}

#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
/* Evaluate every read-only owner instead of hiding later blockers behind
 * short-circuit evaluation. The bit mask preserves the P2b admission rule. */
static uint32_t moveFrameBlockers(void) {
    const uint32_t now = DoomRPG_GetUpTimeMS();
    uint32_t mask = 0U;
    if (!EspNativeResidentGameplay_isActive() || !EspNativeResidentGameplay_ownsWorldPresent()) mask |= ESP_PERF_MOVE_BLOCK_WORLD;
    if (EspNativeResidentGameplay_isAutomapActive()) mask |= ESP_PERF_MOVE_BLOCK_AUTOMAP;
    if (EspNativeGameplayHub_isActive()) mask |= ESP_PERF_MOVE_BLOCK_HUB;
    if (EspNativeGameplayControls_isActive()) mask |= ESP_PERF_MOVE_BLOCK_TOUCH;
    if (EspNativeGameplayLevelUp_isActive() || EspNativeGameplayTransition_isWaitingStats() || EspNativeTransitionPresentation_isLoadingActive()) mask |= ESP_PERF_MOVE_BLOCK_MODAL;
    if (EspNativeDoorAnimator_hasPendingFrames()) mask |= ESP_PERF_MOVE_BLOCK_DOOR;
    if (EspNativeGameplayMonsterAttackVisual_isBusy()) mask |= ESP_PERF_MOVE_BLOCK_ATTACK;
    if (EspNativePlayerPainVisual_isActive(now) || EspNativePlayerPainVisual_expiryDue(now)) mask |= ESP_PERF_MOVE_BLOCK_PAIN;
    /* A visible indicator is not a pending phase change. Its absolute-clock
     * deadline is checked separately at reuse admission, not witness seeding. */
    if (EspNativeGameplayPresentGate_isArmed()) mask |= ESP_PERF_MOVE_BLOCK_PRESENT;
    if (EspAssetPack_isOpen()) mask |= ESP_PERF_MOVE_BLOCK_PAK;
    if (!EspNativeGameplayMoveEvents_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_MOVE_EVENT;
    if (!EspNativeGameplayActionEngine_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_FEEDBACK;
    if (!EspNativeGameplayGibFx_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_FX;
    if (!EspPickupMini_moveFrameQuiet()) mask |= ESP_PERF_MOVE_BLOCK_PICKUP;
    return mask;
}
#endif

static int recoveryRedraw(DoomRPG_t* doomRpg,
                          const EspPlayerViewState* player,
                          EspNativeGameplayFrameStats* outFrame) {
    if (outFrame != NULL) memset(outFrame, 0, sizeof(*outFrame));
    return doomRpg != NULL && player != NULL && outFrame != NULL &&
           EspNativeGameplayFrame_renderTurn(
               EspNativeRootOwners_live()->render, (uint8_t)player->viewAngle, outFrame);
}

int EspNativeGameplayMonsterMovementPublish_afterProbeWithView(
    struct DoomRPG_s* doomRpgBase,
    const char* trigger,
    const struct Random_s* boundarySavedBase,
    uint8_t boundaryPrepared,
    uint32_t plannedMovesBefore,
    const EspNativeGameplayMonsterMovementView* movement,
    EspNativeGameplayMonsterMovementPublishResult* outResult) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const Random_t* boundarySaved = (const Random_t*)boundarySavedBase;
    const EspNativeGameplayMonsterRecord* monster;
    const EspNativeGameplayMonsterPositionRecord* currentPosition;
    const EspPlayerViewState* player;
    const EspMapSpriteTopologyView* topology;
    EspMapSpriteTopologyRelink relink;
    EspNativeGameplayFrameStats frame;
    EspNativeGameplayFrameStats recoveryFrame;
    Random_t replayStart;
    uint8_t rngCalls;
    uint8_t type;
    uint8_t subtype;
    uint16_t linkState;
    uint16_t linkOrder;
    uint32_t i;
    int projectedBefore;
    int recoveryRendered = 0;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (doomRpg == NULL || outResult == NULL) return 0;

    if (movement == NULL || movement->plannedMoves == plannedMovesBefore) {
        return 1;
    }
    if (movement->plannedMoves != plannedMovesBefore + 1U ||
        publishCapture.active != 1U ||
        movement->lastSpriteIndex != publishCapture.before.spriteIndex ||
        movement->lastSourceTile != publishCapture.before.tileIndex ||
        movement->lastDestTile != publishCapture.after.tileIndex) {
        printf("[MONSTERMOVELIVE] DEFER cause=probe-sequence-or-capture-mismatch mutation=no\n");
        return 0;
    }

    outResult->spriteIndex = publishCapture.before.spriteIndex;
    outResult->sourceTile = publishCapture.before.tileIndex;
    outResult->destTile = publishCapture.after.tileIndex;

    currentPosition = EspNativeGameplayMonsterPosition_find(
        publishCapture.before.spriteIndex);
    monster = EspNativeGameplayMonsterState_find(publishCapture.before.spriteIndex);
    player = EspPlayerView_view();
    topology = EspMapSpriteTopology_view();
    if (currentPosition == NULL || monster == NULL || player == NULL ||
        topology == NULL ||
        memcmp(currentPosition, &publishCapture.before,
               sizeof(publishCapture.before)) != 0 ||
        player->active != 1U || player->viewX != player->destX ||
        player->viewY != player->destY || player->viewAngle != player->destAngle ||
        !EspMapSpriteTopology_getEntity(publishCapture.before.spriteIndex,
                                        &type, &subtype,
                                        &linkState, &linkOrder) ||
        type != ESP_MAP_ENTITY_TYPE_ENEMY || subtype != monster->subtype ||
        (linkState & (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                      ESP_MAP_SPRITE_TOPOLOGY_ALIVE)) !=
            (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
             ESP_MAP_SPRITE_TOPOLOGY_ALIVE) ||
        (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) !=
            publishCapture.before.tileIndex ||
        linkOrder == 0U ||
        !movementRngCalls(trigger, monster, player, &publishCapture.before,
                          &rngCalls)) {
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=publication-preflight mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }

    replayStart = (*EspNativeProcessRng_live());
    if (replayStart.nextRand < 0 ||
        replayStart.nextRand + (int)rngCalls >= RANDTABLESIZE) {
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=rng-replay-would-cross-boundary next=%d calls=%u mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex,
               replayStart.nextRand, (unsigned int)rngCalls);
        return 0;
    }

    if (!EspMapSpriteTopology_prepareRelink(
            publishCapture.before.spriteIndex,
            publishCapture.after.tileIndex, &relink) ||
        relink.sourceTile != publishCapture.before.tileIndex ||
        relink.destTile != publishCapture.after.tileIndex) {
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=topology-relink-preflight mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }

    projectedBefore = EspNativeGameplayMonsterMovementPublish_isProjected(
        publishCapture.before.spriteIndex);
    outResult->positionFNVBefore = EspNativeGameplayMonsterPosition_fingerprint();
    outResult->topologyFNVBefore = topology->stateFNV1a;

    for (i = 0U; i < rngCalls; ++i) {
        (void)DoomRPG_randNextByte(EspNativeProcessRng_live());
    }
    if (memcmp(EspNativeProcessRng_live()->randTable, replayStart.randTable,
               RANDTABLESIZE) != 0 ||
        EspNativeProcessRng_live()->nextRand != replayStart.nextRand + (int)rngCalls) {
        (*EspNativeProcessRng_live()) = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=rng-replay-not-exact calls=%u mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex,
               (unsigned int)rngCalls);
        return 0;
    }

    if (!EspNativeGameplayMonsterPosition_commitPrepared(
            &publishCapture.before, &publishCapture.after)) {
        (*EspNativeProcessRng_live()) = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=position-commit mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }
    if (!EspMapSpriteTopology_commitPreparedRelink(&relink)) {
        (void)EspNativeGameplayMonsterPosition_rollbackPrepared(
            &publishCapture.after, &publishCapture.before);
        (*EspNativeProcessRng_live()) = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=topology-commit rollback=position mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }
    setProjected(publishCapture.after.spriteIndex, 1);

    memset(&frame, 0, sizeof(frame));
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
    /* First goal always renders. Only the synchronous ThreeGoal chain
     * may borrow the original settled-frame witness for at most two goals. */
    const uint32_t blockersBefore = moveFrameBlockers();
    const uint32_t nowMs = (uint32_t)(esp_timer_get_time()/1000LL);
    const uint32_t blockers = blockersBefore |
        (EspNativeFirstFrame_indicatorDue(nowMs) ? ESP_PERF_MOVE_BLOCK_INDICATOR : 0U) |
        (EspNativeSpriteRenderer_ambientDue(nowMs) ? ESP_PERF_MOVE_BLOCK_AMBIENT : 0U);
    const int continuation = movement != EspNativeGameplayMonsterMovement_view();
    const EspMapRuntimeView* shadowRuntime = EspMapRuntime_view();
    const EspMapLineStateView* shadowLines = EspMapLineState_viewBase();
    const EspMapLineTextureStateView* shadowTextures = EspMapLineTextureState_view();
    const EspMapAutomapStateView* shadowMap = EspMapAutomapState_view();
    const uint32_t shadowActivation = EspNativeGameplayMonsterActivation_count();
    const uint32_t shadowAutomap = shadowMap != NULL ? shadowMap->stateFNV1a : 0U;
    EspPerfMoveVisibilityContext shadowContext;
    memset(&shadowContext, 0, sizeof(shadowContext));
    const int shadowContextReady = shadowRuntime != NULL &&
        shadowLines != NULL && shadowTextures != NULL;
    if (shadowContextReady) {
        shadowContext.arena = shadowRuntime->arenaFNV1a;
        shadowContext.lineFNV = shadowLines->stateFNV1a;
        shadowContext.textureFNV = shadowTextures->stateFNV1a;
        shadowContext.x = player->viewX;
        shadowContext.y = player->viewY;
        shadowContext.z = player->viewZ;
        shadowContext.angle = player->viewAngle;
    }
    if (!continuation)
        EspPerfMoveVisibility_semanticBegin(
            shadowContextReady ? &shadowContext : NULL, blockers,
            publishCapture.before.spriteIndex,
            publishCapture.before.worldX, publishCapture.before.worldY,
            publishCapture.after.worldX, publishCapture.after.worldY, nowMs,
            shadowAutomap, shadowActivation,
            EspNativeGameplayPlayerState_fingerprint());
    if (!continuation)
        (void)EspPerfMoveVisibility_firstShadowBegin(
            shadowContextReady ? &shadowContext : NULL, blockers,
            publishCapture.before.spriteIndex,
            publishCapture.before.worldX, publishCapture.before.worldY,
            publishCapture.after.worldX, publishCapture.after.worldY, nowMs);
    const int reused = EspPerfMoveVisibility_tryReuse(
        continuation,
        blockers,
        publishCapture.before.spriteIndex,
        publishCapture.before.worldX, publishCapture.before.worldY,
        publishCapture.after.worldX, publishCapture.after.worldY, nowMs);
    if (reused) {
        EspEntranceMusic_pumpGameplay();
    } else {
        EspPerfMoveVisibility_begin(publishCapture.before.spriteIndex,
            publishCapture.before.worldX, publishCapture.before.worldY,
            publishCapture.after.worldX, publishCapture.after.worldY, blockersBefore == 0U);
    }
    if (!reused &&
#else
    if (
#endif
        !EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                           (uint8_t)player->viewAngle,
                                           &frame)) {
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
        if (!continuation) {
            (void)EspPerfMoveVisibility_semanticEnd(0, 0U, 0U, 1, 0, 0U, 0U);
            EspPerfMoveVisibility_firstShadowEnd(0, 0U, 0U, 1, 0);
        }
        EspPerfMoveVisibility_end(0);
        EspPerfMoveVisibility_invalidate();
#endif
        int ownersExact = restorePublishedOwners(&relink, &publishCapture.before,
                                                 &publishCapture.after,
                                                 projectedBefore);
        (*EspNativeProcessRng_live()) = replayStart;
        recoveryRendered = recoveryRedraw(doomRpg, player, &recoveryFrame);
        outResult->recoveryRendered = (uint8_t)(recoveryRendered != 0);
        printf("[MONSTERMOVELIVE] ROLLBACK sprite=%u tile=%u->%u ownersExact=%s randomExact=yes projectionExact=yes recoveryRender=%s interpolation=deferred mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex,
               (unsigned int)publishCapture.before.tileIndex,
               (unsigned int)publishCapture.after.tileIndex,
               ownersExact ? "yes" : "NO",
               recoveryRendered ? "yes" : "NO");
        return 0;
    }

    topology = EspMapSpriteTopology_view();
    currentPosition = EspNativeGameplayMonsterPosition_find(
        publishCapture.after.spriteIndex);
    if (topology == NULL || currentPosition == NULL ||
        memcmp(currentPosition, &publishCapture.after,
               sizeof(publishCapture.after)) != 0 ||
        !EspNativeGameplayMonsterMovementPublish_isProjected(
            publishCapture.after.spriteIndex) ||
        !EspMapSpriteTopology_getEntity(publishCapture.after.spriteIndex,
                                        &type, &subtype,
                                        &linkState, &linkOrder) ||
        (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) !=
            publishCapture.after.tileIndex ||
        linkOrder != relink.linkOrderAfter) {
        int ownersExact = restorePublishedOwners(&relink, &publishCapture.before,
                                                 &publishCapture.after,
                                                 projectedBefore);
        (*EspNativeProcessRng_live()) = replayStart;
        recoveryRendered = recoveryRedraw(doomRpg, player, &recoveryFrame);
        outResult->recoveryRendered = (uint8_t)(recoveryRendered != 0);
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
        EspPerfMoveVisibility_invalidate();
#endif
        printf("[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=post-render-owner-mismatch ownersExact=%s recoveryRender=%s mutation=no\n",
               (unsigned int)publishCapture.after.spriteIndex,
               ownersExact ? "yes" : "NO",
               recoveryRendered ? "yes" : "NO");
        return 0;
    }

    if (boundaryPrepared != 0U) {
        if (boundarySaved == NULL ||
            !EspNativeRngReplayGuard_commitProbeBoundary(
                EspNativeProcessRng_live(), boundarySaved, boundaryPrepared, rngCalls)) {
            int ownersExact = restorePublishedOwners(&relink,
                                                     &publishCapture.before,
                                                     &publishCapture.after,
                                                     projectedBefore);
            (*EspNativeProcessRng_live()) = replayStart;
            recoveryRendered = recoveryRedraw(doomRpg, player, &recoveryFrame);
            outResult->recoveryRendered = (uint8_t)(recoveryRendered != 0);
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
            EspPerfMoveVisibility_invalidate();
#endif
            printf("[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=rng-boundary-close ownersExact=%s recoveryRender=%s mutation=no\n",
                   (unsigned int)publishCapture.after.spriteIndex,
                   ownersExact ? "yes" : "NO",
                   recoveryRendered ? "yes" : "NO");
            return 0;
        }
        outResult->boundaryClosed = 1U;
    }

    topology = EspMapSpriteTopology_view();
    outResult->positionFNVAfter = EspNativeGameplayMonsterPosition_fingerprint();
    outResult->topologyFNVAfter = topology != NULL ? topology->stateFNV1a : 0U;
    outResult->rngCalls = rngCalls;
    outResult->committed = 1U;
#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
    /* Seed/re-arm only after all topology/position/RNG checks have closed. */
    if (reused) EspPerfMoveVisibility_commitReuse(moveFrameBlockers() == 0U);
    else {
        const int quietAfter = moveFrameBlockers() == 0U;
        const EspMapAutomapStateView* afterMap = EspMapAutomapState_view();
        const uint32_t afterActivation = EspNativeGameplayMonsterActivation_count();
        const uint32_t afterPlayer = EspNativeGameplayPlayerState_fingerprint();
        const int sideEffects = shadowMap == NULL || afterMap == NULL ||
            shadowAutomap != afterMap->stateFNV1a || shadowActivation != afterActivation;
        if (!continuation) {
            (void)EspPerfMoveVisibility_semanticEnd(frame.finalPresented,
                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects,
                quietAfter, afterPlayer, frame.totalMicros);
            EspPerfMoveVisibility_firstShadowEnd(frame.finalPresented,
                frame.frameBeforeFNV, frame.frameAfterFNV, sideEffects, quietAfter);
        }
        EspPerfMoveVisibility_end(frame.finalPresented && quietAfter ? 1 : 2);
        EspPerfMoveVisibility_rememberContext(
            frame.finalPresented && shadowContextReady ? &shadowContext : NULL);
        EspPerfMoveVisibility_semanticRemember(
            afterMap != NULL ? afterMap->stateFNV1a : 0U, afterActivation,
            afterPlayer, frame.frameAfterFNV,
            frame.finalPresented && quietAfter && shadowContextReady && afterMap != NULL);
    }
#endif
    DRPG_LOGT("[MONSTERMOVELIVE] COMMIT trigger=%s sprite=%u tile=%u->%u pos=%u,%u->%u,%u rngCalls=%u randomCommitted=yes positionFNV=%08x->%08x topologyFNV=%08x->%08x linkOrder=%u->%u renderer=snap-destination projected=yes frame=%08x presented=%u interpolation=deferred rollback=closed\n",
           trigger,
           (unsigned int)publishCapture.before.spriteIndex,
           (unsigned int)publishCapture.before.tileIndex,
           (unsigned int)publishCapture.after.tileIndex,
           (unsigned int)publishCapture.before.worldX,
           (unsigned int)publishCapture.before.worldY,
           (unsigned int)publishCapture.after.worldX,
           (unsigned int)publishCapture.after.worldY,
           (unsigned int)rngCalls,
           (unsigned int)outResult->positionFNVBefore,
           (unsigned int)outResult->positionFNVAfter,
           (unsigned int)outResult->topologyFNVBefore,
           (unsigned int)outResult->topologyFNVAfter,
           (unsigned int)relink.linkOrderBefore,
           (unsigned int)relink.linkOrderAfter,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
    return 1;
}
