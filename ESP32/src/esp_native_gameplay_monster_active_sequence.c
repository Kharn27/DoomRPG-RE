#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "doomrpg_log.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_active_sequence.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_player_death.h"

#define ACTIVESEQ_TYPE_ENEMY 1U
#define ACTIVESEQ_SUBTYPE_SPECIAL_AI 10U
#define ACTIVESEQ_SUBTYPE_LIMIT 14U
#define ACTIVESEQ_NO_SPRITE 0xffffU
#define ACTIVESEQ_SPRITE_NO_ACTIVATE 0x01000000UL

typedef struct ActiveMovementSequencer_s {
    uint32_t sourceArenaFNV1a;
    uint32_t actualNoAttackSeen;
    uint32_t actualMovementSeen;
    uint32_t expandedNoAttack;
    uint32_t expandedMovement;
    uint32_t expandedTurns;
    uint32_t deliveredMembers;
    uint32_t deferredTurns;
    uint32_t turnActivationCount;
    uint32_t turnOrdinal;
    uint32_t turnDelivered;
    uint32_t pauseProbe;
    uint8_t active;
    uint8_t turnInProgress;
    uint8_t reserved[2];
} ActiveMovementSequencer;

static ActiveMovementSequencer activeSeq;

int __real_EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex);
int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);

static uint16_t read16le(const uint8_t* bytes) {
    if (bytes == NULL) return 0U;
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

/* Legacy Render_renderSpriteObject() calls Game_activate() as soon as an
 * admitted non-hidden monster sprite reaches the sprite-object renderer, before
 * near-plane/column clipping. The native BSP-visible admission point is the
 * equivalent permanent boundary, so activation is observed here rather than by
 * the old center-ray approximation. */
int __wrap_EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex) {
    const EspMapSpriteTopologyView* topology;
    EspMapSprite sprite;
    uint16_t linkState;
    uint16_t tile;
    uint8_t type;
    uint8_t subtype;
    int visible = __real_EspNativeBspVisibility_mapSpriteVisible(
        state, mapSpriteIndex, outLeafIndex);

    if (!visible || mapSpriteIndex > 0xffffU) return visible;
    topology = EspMapSpriteTopology_view();
    if (topology == NULL || topology->entityTypes == NULL ||
        topology->entitySubTypes == NULL || topology->linkStatesLE == NULL ||
        mapSpriteIndex >= topology->spriteCount) {
        return visible;
    }

    type = topology->entityTypes[mapSpriteIndex];
    subtype = (uint8_t)(topology->entitySubTypes[mapSpriteIndex] & 0x7fU);
    linkState = read16le(&topology->linkStatesLE[mapSpriteIndex * 2U]);
    if (type != ACTIVESEQ_TYPE_ENEMY ||
        (linkState & (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                      ESP_MAP_SPRITE_TOPOLOGY_ALIVE)) !=
            (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
             ESP_MAP_SPRITE_TOPOLOGY_ALIVE) ||
        !__real_EspMapRuntime_getMapSprite(mapSpriteIndex, &sprite) ||
        (sprite.info & ACTIVESEQ_SPRITE_NO_ACTIVATE) != 0U) {
        return visible;
    }

    tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
    (void)EspNativeGameplayMonsterActivation_observeVisible(
        (uint16_t)mapSpriteIndex, subtype, tile);
    return visible;
}

static void clearCompositionOverrides(void) {
    EspNativeGameplayMonsterActivation_clearSelection();
    EspNativeGameplayMonsterActivation_clearTurnCounterOverride();
}

static void primeMovementCounters(struct DoomRPG_s* doomRpg) {
    EspNativeGameplayMonsterActivation_clearSelection();
    EspNativeGameplayMonsterActivation_overrideTurnCounters(
        activeSeq.expandedMovement, activeSeq.expandedNoAttack);
    EspNativeGameplayMonsterMovement_service(doomRpg);
    clearCompositionOverrides();
}

static int callMovementMember(struct DoomRPG_s* doomRpg,
                              uint16_t spriteIndex,
                              int selected,
                              const char* trigger,
                              uint8_t* outCommitted) {
    int status;
    if (outCommitted != NULL) *outCommitted = 0U;
    if (selected) {
        EspNativeGameplayMonsterActivation_selectOnly(spriteIndex);
    }
    else {
        EspNativeGameplayMonsterActivation_clearSelection();
    }
    EspNativeGameplayMonsterActivation_overrideTurnCounters(
        activeSeq.expandedMovement, activeSeq.expandedNoAttack);
    status = EspNativeGameplayMonsterMovementProbe_serviceMember(
        doomRpg, trigger, outCommitted);
    clearCompositionOverrides();
    return status;
}

static void resetSequencer(const EspNativeGameplayMonsterTurnView* actual,
                           struct DoomRPG_s* doomRpg) {
    memset(&activeSeq, 0, sizeof(activeSeq));
    if (actual == NULL) return;
    activeSeq.sourceArenaFNV1a = actual->sourceArenaFNV1a;
    activeSeq.actualNoAttackSeen = actual->noAttackTurns;
    activeSeq.actualMovementSeen = actual->movementDeferredTurns;
    activeSeq.expandedNoAttack = actual->noAttackTurns;
    activeSeq.expandedMovement = actual->movementDeferredTurns;
    activeSeq.active = 1U;

    /* Prime the existing planner's private observed counters before any player
     * turn delta. This call is a no-op gameplay-wise because both synthetic
     * counters equal their current baselines. */
    primeMovementCounters(doomRpg);
    printf("[MONSTERACTIVESEQ] READY arena=%08x ownerBytes=%u activationOrder=first-render-activation planner=selected-member syntheticCounters=movement-private-only publication=ordinal-pause-resume multiAttack=serialized-one-probe-in-flight allocation=no\n",
           (unsigned int)activeSeq.sourceArenaFNV1a,
           (unsigned int)sizeof(activeSeq));
}

/* Expand one legacy monster turn in exact active-list order.
 *
 * The producer now publishes only one turn token. Each active member is then
 * handled independently:
 *   1. probe its immediate Entity_aiThink attack gate;
 *   2. if no immediate attack, run its selected movement transaction;
 *   3. if movement lands on an attack gate, publish that one attack;
 *   4. pause before the next ordinal until Retaliation resolves the probe.
 *
 * This preserves the one-probe-in-flight invariant without starving movers that
 * appear before or after an already-adjacent attacker. Newly activated monsters
 * are not appended mid-turn: turnActivationCount snapshots the active-list
 * prefix when the token begins, matching one bounded Game_monsterAI() pass.
 */
void EspNativeGameplayMonsterActiveSequence_service(struct DoomRPG_s* doomRpg) {
    const EspNativeGameplayMonsterTurnView* actual =
        EspNativeGameplayMonsterTurn_view();
    const EspNativeGameplayMonsterRetaliationView* retaliation;
    uint32_t activationCount;
    uint32_t ordinal;

    if (actual == NULL || actual->active != 1U ||
        actual->sourceArenaFNV1a == 0U) {
        clearCompositionOverrides();
        EspNativeGameplayMonsterMovement_service(doomRpg);
        return;
    }

    if (activeSeq.active == 0U ||
        activeSeq.sourceArenaFNV1a != actual->sourceArenaFNV1a ||
        actual->noAttackTurns < activeSeq.actualNoAttackSeen ||
        actual->movementDeferredTurns < activeSeq.actualMovementSeen) {
        resetSequencer(actual, doomRpg);
        return;
    }

    if (activeSeq.turnInProgress != 0U) {
        if (EspNativeGameplayPlayerDeath_isActive()) {
            printf("[MONSTERACTIVESEQ] TERMINAL turn=%u ordinal=%u/%u probe=%u cause=player-death remaining=discarded sameMonsterTurn=yes\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)activeSeq.turnOrdinal,
                   (unsigned int)activeSeq.turnActivationCount,
                   (unsigned int)activeSeq.pauseProbe);
            activeSeq.turnInProgress = 0U;
            activeSeq.pauseProbe = 0U;
            return;
        }

        if (activeSeq.pauseProbe != 0U) {
            retaliation = EspNativeGameplayMonsterRetaliation_view();
            if (retaliation == NULL ||
                retaliation->lastResolvedProbe < activeSeq.pauseProbe) {
                return;
            }
            printf("[MONSTERACTIVESEQ] RESUME turn=%u nextOrdinal=%u/%u resolvedProbe=%u delivered=%u sameMonsterTurn=yes\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)(activeSeq.turnOrdinal + 1U),
                   (unsigned int)activeSeq.turnActivationCount,
                   (unsigned int)activeSeq.pauseProbe,
                   (unsigned int)activeSeq.turnDelivered);
            activeSeq.pauseProbe = 0U;
        }
    }
    else {
        if (actual->noAttackTurns == activeSeq.actualNoAttackSeen) return;

        if (actual->noAttackTurns != activeSeq.actualNoAttackSeen + 1U) {
            ++activeSeq.deferredTurns;
            printf("[MONSTERACTIVESEQ] DEFER reason=turn-token-gap observed=%u current=%u mutation=no rngConsumed=0\n",
                   (unsigned int)activeSeq.actualNoAttackSeen,
                   (unsigned int)actual->noAttackTurns);
            activeSeq.actualNoAttackSeen = actual->noAttackTurns;
            return;
        }

        activeSeq.actualNoAttackSeen = actual->noAttackTurns;
        activeSeq.actualMovementSeen = actual->movementDeferredTurns;
        ++activeSeq.expandedTurns;
        activeSeq.turnActivationCount =
            EspNativeGameplayMonsterActivation_count();
        activeSeq.turnOrdinal = 0U;
        activeSeq.turnDelivered = 0U;
        activeSeq.pauseProbe = 0U;
        activeSeq.turnInProgress = 1U;

        printf("[MONSTERACTIVESEQ] BEGIN turn=%u reason=%u activeCount=%u order=snapshot-prefix perMember=immediate-attack-then-move sameMonsterTurn=yes\n",
               (unsigned int)activeSeq.expandedTurns,
               (unsigned int)actual->lastReason,
               (unsigned int)activeSeq.turnActivationCount);
    }

    activationCount = activeSeq.turnActivationCount;
    if (EspNativeGameplayMonsterActivation_count() < activationCount) {
        ++activeSeq.deferredTurns;
        activeSeq.turnInProgress = 0U;
        printf("[MONSTERACTIVESEQ] DEFER turn=%u cause=activation-count-regressed snapshot=%u current=%u mutation=no rngConsumed=0\n",
               (unsigned int)activeSeq.expandedTurns,
               (unsigned int)activationCount,
               (unsigned int)EspNativeGameplayMonsterActivation_count());
        return;
    }

    while (activeSeq.turnOrdinal < activationCount) {
        uint16_t spriteIndex = ACTIVESEQ_NO_SPRITE;
        const EspNativeGameplayMonsterRecord* monster;
        EspNativeGameplayMonsterMemberProbeStatus memberStatus;
        const EspNativeGameplayMonsterTurnView* beforeTurn;
        const EspNativeGameplayMonsterTurnView* afterTurn;
        uint32_t attackBefore;
        uint32_t attackAfter;
        uint8_t committed = 0U;
        int transactionStatus = 1;

        ordinal = activeSeq.turnOrdinal;
        if (!EspNativeGameplayMonsterActivation_getOrdered(
                ordinal, &spriteIndex)) {
            ++activeSeq.deferredTurns;
            activeSeq.turnInProgress = 0U;
            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u activeCount=%u cause=activation-order-read mutation=no rngConsumed=0\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)ordinal,
                   (unsigned int)activationCount);
            return;
        }
        ++activeSeq.turnOrdinal;

        monster = EspNativeGameplayMonsterState_find(spriteIndex);
        if (monster == NULL || monster->alive == 0U ||
            monster->subtype >= ACTIVESEQ_SUBTYPE_LIMIT) {
            continue;
        }
        if (monster->subtype == ACTIVESEQ_SUBTYPE_SPECIAL_AI) {
            printf("[MONSTERACTIVESEQ] MEMBER-DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=special-ai-unowned mutation=no rngConsumed=0\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)(ordinal + 1U),
                   (unsigned int)activationCount,
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype);
            continue;
        }

        beforeTurn = EspNativeGameplayMonsterTurn_view();
        attackBefore = beforeTurn != NULL ? beforeTurn->attackProbes : 0U;
        memberStatus = EspNativeGameplayMonsterTurn_probeActiveMember(
            doomRpg, spriteIndex);

        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_INVALID) {
            ++activeSeq.deferredTurns;
            activeSeq.turnInProgress = 0U;
            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=member-probe-failed prefixCommitted=yes mutation=no-additional rngConsumed=0-additional\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)(ordinal + 1U),
                   (unsigned int)activationCount,
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype);
            return;
        }

        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE) {
            ++activeSeq.expandedMovement;
            transactionStatus = callMovementMember(
                doomRpg, spriteIndex, 1, "RANGED-AI", &committed);
        }
        else if (memberStatus ==
                 ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK) {
            ++activeSeq.expandedNoAttack;
            transactionStatus = callMovementMember(
                doomRpg, spriteIndex, 1, "NO-IMMEDIATE-ATTACK", &committed);
        }

        ++activeSeq.turnDelivered;
        ++activeSeq.deliveredMembers;
        afterTurn = EspNativeGameplayMonsterTurn_view();
        attackAfter = afterTurn != NULL ? afterTurn->attackProbes : attackBefore;

        printf("[MONSTERACTIVESEQ] MEMBER turn=%u ordinal=%u/%u sprite=%u subtype=%u decision=%s movement=%s attackProbe=%u->%u publication=%s sameMonsterTurn=yes\n",
               (unsigned int)activeSeq.expandedTurns,
               (unsigned int)(ordinal + 1U),
               (unsigned int)activationCount,
               (unsigned int)spriteIndex,
               (unsigned int)monster->subtype,
               memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED
                   ? "immediate-attack"
                   : (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE
                          ? "ranged-move"
                          : "goal-move"),
               committed != 0U ? "committed" :
               (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED
                    ? "none"
                    : (transactionStatus != 0 ? "none" : "deferred")),
               (unsigned int)attackBefore,
               (unsigned int)attackAfter,
               attackAfter > attackBefore ? "probe-before-next" :
               "closed-before-next");

        if (attackAfter > attackBefore &&
            activeSeq.turnOrdinal < activationCount) {
            activeSeq.pauseProbe = attackAfter;
            printf("[MONSTERACTIVESEQ] PAUSE turn=%u afterOrdinal=%u/%u probe=%u reason=attack-in-flight nextOrdinal=%u sameMonsterTurn=yes\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)activeSeq.turnOrdinal,
                   (unsigned int)activationCount,
                   (unsigned int)activeSeq.pauseProbe,
                   (unsigned int)(activeSeq.turnOrdinal + 1U));
            return;
        }
    }

    if (activeSeq.turnDelivered == 0U) {
        uint8_t committed = 0U;
        ++activeSeq.expandedNoAttack;
        (void)callMovementMember(doomRpg, ACTIVESEQ_NO_SPRITE, 0,
                                 "NO-IMMEDIATE-ATTACK", &committed);
    }

    activeSeq.turnInProgress = 0U;
    activeSeq.pauseProbe = 0U;
    if (activationCount == 0U && activeSeq.turnDelivered == 0U) {
        DRPG_LOGT("[MONSTERACTIVESEQ] COMPLETE turn=%u reason=%u activeCount=%u delivered=%u sameMonsterTurn=yes ordered=yes publication=serialized-per-member multiAttack=one-probe-at-a-time\n",
                  (unsigned int)activeSeq.expandedTurns,
                  (unsigned int)actual->lastReason,
                  (unsigned int)activationCount,
                  (unsigned int)activeSeq.turnDelivered);
    }
    else {
        DRPG_LOGI("[MONSTERACTIVESEQ] COMPLETE turn=%u reason=%u activeCount=%u delivered=%u sameMonsterTurn=yes ordered=yes publication=serialized-per-member multiAttack=one-probe-at-a-time\n",
                  (unsigned int)activeSeq.expandedTurns,
                  (unsigned int)actual->lastReason,
                  (unsigned int)activationCount,
                  (unsigned int)activeSeq.turnDelivered);
    }
}
