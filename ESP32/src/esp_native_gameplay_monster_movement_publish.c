#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_frame.h"
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

static void setProjected(uint16_t spriteIndex, int projected) {
    uint32_t word;
    uint32_t mask;
    if (spriteIndex >= ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) return;
    word = (uint32_t)spriteIndex >> 5;
    mask = 1UL << ((uint32_t)spriteIndex & 31U);
    if (projected) projectedBits[word] |= mask;
    else projectedBits[word] &= ~mask;
}

int EspNativeGameplayMonsterMovementPublish_isProjected(uint16_t spriteIndex) {
    uint32_t word;
    uint32_t mask;
    if (spriteIndex >= ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) return 0;
    word = (uint32_t)spriteIndex >> 5;
    mask = 1UL << ((uint32_t)spriteIndex & 31U);
    return (projectedBits[word] & mask) != 0U;
}

void EspNativeGameplayMonsterMovementPublish_beginCycle(void) {
    memset(&publishCapture, 0, sizeof(publishCapture));
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

static int recoveryRedraw(DoomRPG_t* doomRpg,
                          const EspPlayerViewState* player,
                          EspNativeGameplayFrameStats* outFrame) {
    if (outFrame != NULL) memset(outFrame, 0, sizeof(*outFrame));
    return doomRpg != NULL && player != NULL && outFrame != NULL &&
           EspNativeGameplayFrame_renderTurn(
               doomRpg->render, (uint8_t)player->viewAngle, outFrame);
}

int EspNativeGameplayMonsterMovementPublish_afterProbe(
    struct DoomRPG_s* doomRpgBase,
    const char* trigger,
    const struct Random_s* boundarySavedBase,
    uint8_t boundaryPrepared,
    uint32_t plannedMovesBefore,
    EspNativeGameplayMonsterMovementPublishResult* outResult) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const Random_t* boundarySaved = (const Random_t*)boundarySavedBase;
    const EspNativeGameplayMonsterMovementView* movement;
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

    movement = EspNativeGameplayMonsterMovement_view();
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

    replayStart = doomRpg->random;
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
        (void)DoomRPG_randNextByte(&doomRpg->random);
    }
    if (memcmp(doomRpg->random.randTable, replayStart.randTable,
               RANDTABLESIZE) != 0 ||
        doomRpg->random.nextRand != replayStart.nextRand + (int)rngCalls) {
        doomRpg->random = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=rng-replay-not-exact calls=%u mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex,
               (unsigned int)rngCalls);
        return 0;
    }

    if (!EspNativeGameplayMonsterPosition_commitPrepared(
            &publishCapture.before, &publishCapture.after)) {
        doomRpg->random = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=position-commit mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }
    if (!EspMapSpriteTopology_commitPreparedRelink(&relink)) {
        (void)EspNativeGameplayMonsterPosition_rollbackPrepared(
            &publishCapture.after, &publishCapture.before);
        doomRpg->random = replayStart;
        printf("[MONSTERMOVELIVE] DEFER sprite=%u cause=topology-commit rollback=position mutation=no\n",
               (unsigned int)publishCapture.before.spriteIndex);
        return 0;
    }
    setProjected(publishCapture.after.spriteIndex, 1);

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render,
                                           (uint8_t)player->viewAngle,
                                           &frame)) {
        int ownersExact = restorePublishedOwners(&relink, &publishCapture.before,
                                                 &publishCapture.after,
                                                 projectedBefore);
        doomRpg->random = replayStart;
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
        doomRpg->random = replayStart;
        recoveryRendered = recoveryRedraw(doomRpg, player, &recoveryFrame);
        outResult->recoveryRendered = (uint8_t)(recoveryRendered != 0);
        printf("[MONSTERMOVELIVE] ROLLBACK sprite=%u cause=post-render-owner-mismatch ownersExact=%s recoveryRender=%s mutation=no\n",
               (unsigned int)publishCapture.after.spriteIndex,
               ownersExact ? "yes" : "NO",
               recoveryRendered ? "yes" : "NO");
        return 0;
    }

    if (boundaryPrepared != 0U) {
        if (boundarySaved == NULL ||
            !EspNativeRngReplayGuard_commitProbeBoundary(
                &doomRpg->random, boundarySaved, boundaryPrepared, rngCalls)) {
            int ownersExact = restorePublishedOwners(&relink,
                                                     &publishCapture.before,
                                                     &publishCapture.after,
                                                     projectedBefore);
            doomRpg->random = replayStart;
            recoveryRendered = recoveryRedraw(doomRpg, player, &recoveryFrame);
            outResult->recoveryRendered = (uint8_t)(recoveryRendered != 0);
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
    printf("[MONSTERMOVELIVE] COMMIT trigger=%s sprite=%u tile=%u->%u pos=%u,%u->%u,%u rngCalls=%u randomCommitted=yes positionFNV=%08x->%08x topologyFNV=%08x->%08x linkOrder=%u->%u renderer=snap-destination projected=yes frame=%08x presented=%u interpolation=deferred rollback=closed\n",
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
