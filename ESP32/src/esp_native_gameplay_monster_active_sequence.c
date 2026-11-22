#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "esp_native_root_state.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "doomrpg_log.h"
#include "esp_native_door_animator.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_active_sequence.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_subtype10_policy.h"
#include "esp_native_process_rng_owner.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_gameplay_frame.h"
#include "esp_player_view_state.h"

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

int EspNativeGameplayMonsterActiveSequence_isBusy(void) {
    return activeSeq.active == 1U && activeSeq.turnInProgress != 0U;
}

static uint16_t read16le(const uint8_t* bytes) {
    if (bytes == NULL) return 0U;
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

/* Legacy Render_renderSpriteObject() calls Game_activate() as soon as an
 * admitted non-hidden monster sprite reaches the sprite-object renderer, before
 * near-plane/column clipping. The native BSP-visible admission point is the
 * equivalent permanent boundary, so activation is observed here rather than by
 * the old center-ray approximation. */
int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex) {
    const EspMapSpriteTopologyView* topology;
    EspMapSprite sprite;
    uint16_t linkState;
    uint16_t tile;
    uint8_t type;
    uint8_t subtype;
    int visible = EspNativeBspVisibility_mapSpriteVisibleBase(
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
        !EspMapRuntime_getMapSpriteBase(mapSpriteIndex, &sprite) ||
        (sprite.info & ACTIVESEQ_SPRITE_NO_ACTIVATE) != 0U) {
        return visible;
    }

    tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
    (void)EspNativeGameplayMonsterActivation_observeVisible(
        (uint16_t)mapSpriteIndex, subtype, tile);
    return visible;
}


/* P1 admission: inspect the original single <84 byte without consuming it.
 * When the byte crosses a refill boundary, borrow the already-proven RNG
 * reservation and restore the live Random_t before leaving preflight.
 * This preview deliberately does NOT invent an inactive-monster list order.
 * If at least one viable corpse is found, defer rather than revive the wrong
 * corpse or publish a half-owned weapon18/animation/activation transaction. */
typedef enum Subtype10Admission_e {
    SUBTYPE10_NOT_READY = 0,
    SUBTYPE10_FALLBACK = 1,
    SUBTYPE10_RAISE_DEFER = 2
} Subtype10Admission;

static Subtype10Admission subtype10AdmitFallback(
    uint16_t attackerSprite, uint8_t* outRoll, uint16_t* outCorpse) {
    Random_t* live = EspNativeProcessRng_live();
    Random_t saved;
    uint8_t prepared = 0U;
    uint8_t roll;
    const EspNativeGameplayMonsterView* monsters;
    const EspNativeGameplayMonsterPositionRecord* attackerPos;
    const EspMapSpriteTopologyView* topology;
    uint32_t i;

    if (outRoll != NULL) *outRoll = 0U;
    if (outCorpse != NULL) *outCorpse = ACTIVESEQ_NO_SPRITE;
    if (live == NULL || outRoll == NULL || outCorpse == NULL ||
        live->nextRand < 0 || live->nextRand >= RANDTABLESIZE ||
        !EspNativeRngReplayGuard_beginProbeBoundary(
            live, &saved, &prepared)) {
        return SUBTYPE10_NOT_READY;
    }
    /* beginProbeBoundary() exposes the reserved new table for this preview. */
    if (live->nextRand < 0 || live->nextRand >= RANDTABLESIZE) {
        if (prepared != 0U) {
            (void)EspNativeRngReplayGuard_endProbeBoundary(
                live, &saved, prepared);
        }
        return SUBTYPE10_NOT_READY;
    }
    roll = live->randTable[live->nextRand];
    if (prepared != 0U &&
        !EspNativeRngReplayGuard_endProbeBoundary(live, &saved, prepared)) {
        return SUBTYPE10_NOT_READY;
    }
    *outRoll = roll;
    if (!EspNativeSubtype10_wantsRaise(roll)) {
        return SUBTYPE10_FALLBACK;
    }

    monsters = EspNativeGameplayMonsterState_view();
    topology = EspMapSpriteTopology_view();
    attackerPos = EspNativeGameplayMonsterPosition_find(attackerSprite);
    if (monsters == NULL || monsters->records == NULL ||
        topology == NULL || attackerPos == NULL ||
        topology->spriteCount > 1024U) {
        return SUBTYPE10_NOT_READY;
    }

    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* corpse = &monsters->records[i];
        const EspNativeGameplayMonsterPositionRecord* pos;
        int32_t dx;
        int32_t dy;
        uint8_t visual = 0U;
        uint8_t occupied = 0U;
        uint32_t j;

        if (corpse->alive != 0U || corpse->spriteIndex == attackerSprite) {
            continue;
        }
        pos = EspNativeGameplayMonsterPosition_find(corpse->spriteIndex);
        if (pos == NULL || !EspMapSpriteTopology_getVisualState(
                corpse->spriteIndex, &visual)) {
            return SUBTYPE10_NOT_READY;
        }
        dx = (int32_t)pos->worldX - (int32_t)attackerPos->worldX;
        dy = (int32_t)pos->worldY - (int32_t)attackerPos->worldY;
        if (!EspNativeSubtype10_candidateEligible(
                dx, dy, 1U, 0U, 0U)) {
            continue;
        }
        /* The native renderer knows hidden gib vs corpse, but P1 cannot
         * establish every legacy 0x1000000/0x2000000 bit (notably after LOAD).
         * A nearby non-hidden death/corpse must therefore be deferred. */
        if ((visual & 0x80U) != 0U) continue;
        if ((visual & 0x0fU) != 2U && (visual & 0x0fU) != 4U) {
            return SUBTYPE10_NOT_READY;
        }

        /* Exact 16263 *entity* occupancy mask; links/line overlays not
         * represented by this topology remain conservatively unsupported.
         * This is only a negative gate, never authority to resurrect. */
        for (j = 0U; j < topology->spriteCount; ++j) {
            uint8_t type = 0U, subtype = 0U;
            uint16_t linkState = 0U, linkOrder = 0U;
            if (!EspMapSpriteTopology_getEntity(
                    j, &type, &subtype, &linkState, &linkOrder)) {
                return SUBTYPE10_NOT_READY;
            }
            if (type < 16U &&
                (ESP_NATIVE_SUBTYPE10_RAISE_OCCUPANCY_MASK &
                 (1U << type)) != 0U &&
                (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U &&
                (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) ==
                    pos->tileIndex) {
                occupied = 1U;
                break;
            }
        }
        if (EspNativeSubtype10_candidateEligible(
                dx, dy, 1U, 0U, occupied)) {
            *outCorpse = corpse->spriteIndex;
            return SUBTYPE10_RAISE_DEFER;
        }
    }
    return SUBTYPE10_FALLBACK;
}

static void clearCompositionOverrides(void) {
    EspNativeGameplayMonsterActivation_clearSelection();
    EspNativeGameplayMonsterActivation_clearTurnCounterOverride();
}

static void releaseDeferredDoorVisual(struct DoomRPG_s* doomRpgBase,
                                      const char* reason) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* player;
    EspNativeGameplayFrameStats frame;
    uint8_t held = EspNativeDoorAnimator_deferredCloseCount();
    uint8_t released;

    if (held == 0U) return;
    released = EspNativeDoorAnimator_releaseDeferredCloses();
    if (released == 0U) return;

    player = EspPlayerView_view();
    if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL || player == NULL ||
        player->active != 1U || player->viewX != player->destX ||
        player->viewY != player->destY ||
        player->viewAngle != player->destAngle) {
        printf("[DOORANIM] POST-MONSTER-DEFER lines=%u reason=%s redraw=next-world-frame logical=closed\n",
               (unsigned int)released,
               reason != NULL ? reason : "turn-end");
        return;
    }

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(
            EspNativeRootOwners_live()->render, (uint8_t)player->viewAngle, &frame)) {
        printf("[DOORANIM] POST-MONSTER-FAILED lines=%u reason=%s logical=closed recovery=stable-next-frame\n",
               (unsigned int)released,
               reason != NULL ? reason : "turn-end");
        return;
    }
    printf("[DOORANIM] POST-MONSTER-COMPLETE lines=%u reason=%s frame=%08x presented=%u logical=closed visual=closed\n",
           (unsigned int)released,
           reason != NULL ? reason : "turn-end",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
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
 *   5. yield after a fully processed non-attacking live member, allowing one
 *      compositor service between members without a wall-clock delay.
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
    uint32_t handledThisService = 0U;

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
            releaseDeferredDoorVisual(doomRpg, "player-death");
            return;
        }

        if (activeSeq.pauseProbe != 0U) {
            retaliation = EspNativeGameplayMonsterRetaliation_view();
            if (retaliation == NULL ||
                retaliation->lastResolvedProbe < activeSeq.pauseProbe) {
                return;
            }
            /* The previous retaliation is committed; only that gameplay
             * barrier matters. The directional overlay is an asynchronous
             * compositor lease and MUST NOT extend the monster's turn.
             * Its FIFO already captured the exact attacking sprite/probe. */
            DRPG_LOGT("[MONSTERACTIVESEQ] RESUME turn=%u nextOrdinal=%u/%u resolvedProbe=%u delivered=%u sameMonsterTurn=yes\n",
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
            releaseDeferredDoorVisual(doomRpg, "turn-token-gap");
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
        releaseDeferredDoorVisual(doomRpg, "activation-count-regressed");
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
        uint8_t subtype10Prepared = 0U;
        uint8_t subtype10OldAlt = 0U;
        uint8_t subtype10Roll = 0U;
        uint16_t subtype10Corpse = ACTIVESEQ_NO_SPRITE;
        Random_t subtype10Before;
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
            releaseDeferredDoorVisual(doomRpg, "activation-order-read");
            return;
        }
        ++activeSeq.turnOrdinal;

        monster = EspNativeGameplayMonsterState_find(spriteIndex);
        if (monster == NULL || monster->alive == 0U ||
            monster->subtype >= ACTIVESEQ_SUBTYPE_LIMIT) {
            continue;
        }
        if (monster->subtype == ACTIVESEQ_SUBTYPE_SPECIAL_AI) {
            Subtype10Admission admission = subtype10AdmitFallback(
                spriteIndex, &subtype10Roll, &subtype10Corpse);
            if (admission != SUBTYPE10_FALLBACK) {
                printf("[MONSTERSUB10] %s turn=%u ordinal=%u/%u sprite=%u corpse=%u rollPreview=%u rngConsumed=0 mutation=no reason=%s\n",
                       admission == SUBTYPE10_RAISE_DEFER
                           ? "RAISE-DEFER" : "DEFER",
                       (unsigned int)activeSeq.expandedTurns,
                       (unsigned int)(ordinal + 1U),
                       (unsigned int)activationCount,
                       (unsigned int)spriteIndex,
                       (unsigned int)subtype10Corpse,
                       (unsigned int)subtype10Roll,
                       admission == SUBTYPE10_RAISE_DEFER
                           ? "weapon18-revive-transaction-unowned"
                           : "rng-or-corpse-preflight-not-ready");
                continue;
            }
            /* Legacy consumes this byte BEFORE toggling the alternate
             * attack flag, even if no inactive monster is reanimable. */
            subtype10Before = *EspNativeProcessRng_live();
            if (DoomRPG_randNextByte(EspNativeProcessRng_live()) !=
                    subtype10Roll) {
                *EspNativeProcessRng_live() = subtype10Before;
                printf("[MONSTERSUB10] DEFER sprite=%u reason=preview-live-roll-mismatch rngRollback=yes mutation=no\n",
                       (unsigned int)spriteIndex);
                continue;
            }
            subtype10OldAlt = monster->alternateAttack;
            if (!EspNativeGameplayMonsterState_toggleAlternateAttack(
                    spriteIndex, subtype10OldAlt)) {
                *EspNativeProcessRng_live() = subtype10Before;
                printf("[MONSTERSUB10] DEFER sprite=%u reason=alternate-state-not-owned rngRollback=yes mutation=no\n",
                       (unsigned int)spriteIndex);
                continue;
            }
            subtype10Prepared = 1U;
            DRPG_LOGI("[MONSTERSUB10] FALLBACK turn=%u ordinal=%u/%u sprite=%u roll=%u raiseRoll=%u eligibleCorpse=none rngCalls=1 alt=%u->%u next=ordinary-ai\n",
                      (unsigned int)activeSeq.expandedTurns,
                      (unsigned int)(ordinal + 1U),
                      (unsigned int)activationCount,
                      (unsigned int)spriteIndex,
                      (unsigned int)subtype10Roll,
                      (unsigned int)EspNativeSubtype10_wantsRaise(subtype10Roll),
                      (unsigned int)subtype10OldAlt,
                      (unsigned int)(subtype10OldAlt ^ 1U));
        }

        beforeTurn = EspNativeGameplayMonsterTurn_view();
        attackBefore = beforeTurn != NULL ? beforeTurn->attackProbes : 0U;
        memberStatus = EspNativeGameplayMonsterTurn_probeActiveMember(
            doomRpg, spriteIndex);

        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_INVALID) {
            if (subtype10Prepared != 0U) {
                /* Member-probe never published: undo the special-prefix
                 * mutable record and live logical RNG before aborting. */
                const int altRestored =
                    EspNativeGameplayMonsterState_toggleAlternateAttack(
                        spriteIndex, (uint8_t)(subtype10OldAlt ^ 1U));
                *EspNativeProcessRng_live() = subtype10Before;
                printf("[MONSTERSUB10] ROLLBACK sprite=%u altExact=%s rngRestored=yes reason=member-probe-invalid\n",
                       (unsigned int)spriteIndex,
                       altRestored ? "yes" : "NO");
            }
            ++activeSeq.deferredTurns;
            activeSeq.turnInProgress = 0U;
            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=member-probe-failed prefixCommitted=yes mutation=no-additional rngConsumed=0-additional\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)(ordinal + 1U),
                   (unsigned int)activationCount,
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype);
            releaseDeferredDoorVisual(doomRpg, "member-probe-failed");
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

        /* Keep failed publication visible; successful members are TRACE-only. */
#if DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_TRACE
        if (!transactionStatus)
#endif
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

        if (attackAfter > attackBefore) {
            if (activeSeq.turnOrdinal < activationCount) {
                activeSeq.pauseProbe = attackAfter;
                DRPG_LOGT("[MONSTERACTIVESEQ] PAUSE turn=%u afterOrdinal=%u/%u probe=%u reason=attack-in-flight nextOrdinal=%u sameMonsterTurn=yes\n",
                       (unsigned int)activeSeq.expandedTurns,
                       (unsigned int)activeSeq.turnOrdinal,
                       (unsigned int)activationCount,
                       (unsigned int)activeSeq.pauseProbe,
                       (unsigned int)(activeSeq.turnOrdinal + 1U));
                return;
            }
            if (EspNativeDoorAnimator_isHoldingDeferredClose()) {
                activeSeq.pauseProbe = attackAfter;
                DRPG_LOGT("[MONSTERACTIVESEQ] PAUSE-FINAL turn=%u ordinal=%u/%u probe=%u reason=deferred-door-waits-final-attack sameMonsterTurn=yes\n",
                       (unsigned int)activeSeq.expandedTurns,
                       (unsigned int)activeSeq.turnOrdinal,
                       (unsigned int)activationCount,
                       (unsigned int)activeSeq.pauseProbe);
                return;
            }
        }
        /* One complete LIVE member per service. Preserve the same immutable
         * active-list prefix, ordinal, counter overrides and RNG/turn state.
         * Never yield inside a member or in the middle of attack publication.
         * The caller's compositor can now run between movement members. */
        ++handledThisService;
        if (EspNativeGameplayMonsterActiveSequence_shouldYield(
                handledThisService, activationCount - activeSeq.turnOrdinal)) {
            DRPG_LOGT("[MONSTERACTIVESEQ] YIELD turn=%u nextOrdinal=%u/%u handled=%u budget=%u mode=cooperative-loop-task turn=still-locked\n",
                      (unsigned int)activeSeq.expandedTurns,
                      (unsigned int)(activeSeq.turnOrdinal + 1U),
                      (unsigned int)activationCount,
                      (unsigned int)handledThisService,
                      (unsigned int)ESP_NATIVE_GAMEPLAY_ACTIVESEQ_MEMBER_BUDGET);
            return;
        }
    }

    /* If an activation-order prefix exists but every member was skipped
     * (typically defeated monsters, whose first-render activation is retained
     * for checkpoint parity), there is no current monster to move. Do not
     * replay a synthetic unselected planner: it reports a misleading
     * active-order-not-owned despite having no living member to dispatch.
     * Preserve the zero-activation counter synchronization only. Unsupported
     * living subtypes remain explicitly MEMBER-DEFER above, not masked. */
    if (activeSeq.turnDelivered == 0U && activationCount == 0U) {
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
    releaseDeferredDoorVisual(doomRpg, "monster-turn-complete");
}
