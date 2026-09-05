#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_rng_replay_guard.h"

typedef struct MovementProbeCompositionOwner_s {
    uint32_t sourceArenaFNV1a;
    uint32_t observedMovementDeferredTurns;
    uint32_t observedNoAttackTurns;
    uint8_t active;
    uint8_t reserved[3];
} MovementProbeCompositionOwner;

static MovementProbeCompositionOwner compositionOwner;

static int atByteBoundary(const Random_t* rand) {
    return rand != NULL &&
           (rand->nextRand + (int)sizeof(byte)) >= RANDTABLESIZE;
}

static int exactlyOneProducerAdvanced(
    const EspNativeGameplayMonsterTurnView* turn,
    const char** outTrigger) {
    uint32_t moveDelta;
    uint32_t noAttackDelta;

    if (outTrigger != NULL) *outTrigger = "none";
    if (turn == NULL || outTrigger == NULL || compositionOwner.active == 0U ||
        turn->sourceArenaFNV1a != compositionOwner.sourceArenaFNV1a ||
        turn->movementDeferredTurns < compositionOwner.observedMovementDeferredTurns ||
        turn->noAttackTurns < compositionOwner.observedNoAttackTurns) {
        return 0;
    }

    moveDelta = turn->movementDeferredTurns -
                compositionOwner.observedMovementDeferredTurns;
    noAttackDelta = turn->noAttackTurns - compositionOwner.observedNoAttackTurns;
    if (moveDelta == 1U && noAttackDelta == 0U) {
        *outTrigger = "RANGED-AI";
        return 1;
    }
    if (moveDelta == 0U && noAttackDelta == 1U) {
        *outTrigger = "NO-IMMEDIATE-ATTACK";
        return 1;
    }
    return 0;
}

static uint32_t plannedMovesNow(void) {
    const EspNativeGameplayMonsterMovementView* movement =
        EspNativeGameplayMonsterMovement_view();
    return movement != NULL ? movement->plannedMoves : 0U;
}

static void servicePostMoveGoal(
    struct DoomRPG_s* doomRpg,
    const char* trigger,
    const EspNativeGameplayMonsterMovementPublishResult* publish) {
    if (trigger == NULL || publish == NULL || publish->committed == 0U) return;

    /* The legacy >=217 ranged branch intentionally ends after its successful
     * move. The same-turn post-move attack gate belongs to aiMoveToGoal's melee
     * goal family reached from the no-immediate-attack path. */
    if (strcmp(trigger, "NO-IMMEDIATE-ATTACK") != 0) {
        printf("[MONSTERPOSTMOVE] SKIP trigger=%s sprite=%u tile=%u->%u cause=ranged-ai-branch legacySameTurnAttack=no mutation=no rngConsumed=0\n",
               trigger,
               (unsigned int)publish->spriteIndex,
               (unsigned int)publish->sourceTile,
               (unsigned int)publish->destTile);
        return;
    }

    (void)EspNativeGameplayMonsterTurn_postMoveGoal(
        doomRpg, publish->spriteIndex, publish->sourceTile, publish->destTile);
}

void EspNativeGameplayMonsterMovementProbe_reset(void) {
    memset(&compositionOwner, 0, sizeof(compositionOwner));
    EspNativeGameplayMonsterMovementPublish_reset();
    EspNativeGameplayMonsterMovement_reset();
    EspNativeGameplayMonsterPosition_reset();
}

void EspNativeGameplayMonsterMovementProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterTurnView* turn =
        EspNativeGameplayMonsterTurn_view();
    const char* trigger = "none";
    int producerAdvanced;

    if (turn == NULL || turn->active != 1U || turn->sourceArenaFNV1a == 0U) {
        EspNativeGameplayMonsterMovement_service(doomRpgBase);
        return;
    }

    if (compositionOwner.active == 0U ||
        compositionOwner.sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        memset(&compositionOwner, 0, sizeof(compositionOwner));
        compositionOwner.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        compositionOwner.observedMovementDeferredTurns =
            turn->movementDeferredTurns;
        compositionOwner.observedNoAttackTurns = turn->noAttackTurns;
        compositionOwner.active = 1U;
        EspNativeGameplayMonsterMovement_service(doomRpgBase);
        return;
    }

    producerAdvanced = exactlyOneProducerAdvanced(turn, &trigger);
    if (producerAdvanced && doomRpg != NULL) {
        uint32_t plannedBefore = plannedMovesNow();
        EspNativeGameplayMonsterMovementPublishResult publish;

        EspNativeGameplayMonsterMovementPublish_beginCycle();
        memset(&publish, 0, sizeof(publish));

        if (atByteBoundary(&doomRpg->random)) {
            Random_t saved;
            uint8_t prepared = 0U;
            int restoredExact = 0;

            if (EspNativeRngReplayGuard_beginProbeBoundary(&doomRpg->random,
                                                           &saved,
                                                           &prepared)) {
                printf("[MONSTERMOVERNG] ARM trigger=%s next=127->0 prepared=%u liveRandom=temporary-post-refill reservation=persistent\n",
                       trigger, (unsigned int)prepared);
                EspNativeGameplayMonsterMovement_service(doomRpgBase);
                (void)EspNativeGameplayMonsterMovementPublish_afterProbe(
                    doomRpgBase, trigger, &saved, prepared, plannedBefore, &publish);

                if (prepared != 0U && publish.boundaryClosed == 0U) {
                    restoredExact = EspNativeRngReplayGuard_endProbeBoundary(
                        &doomRpg->random, &saved, prepared);
                    printf("[MONSTERMOVERNG] RESTORE trigger=%s randomLiveExact=%s reservation=pending-until-real-byte-draw\n",
                           trigger, restoredExact ? "yes" : "NO");
                }
                else if (publish.committed != 0U) {
                    printf("[MONSTERMOVERNG] COMMIT trigger=%s rngCalls=%u reservation=consumed-by-live-move randomLive=advanced-exactly\n",
                           trigger, (unsigned int)publish.rngCalls);
                }
                else if (publish.boundaryClosed != 0U) {
                    printf("[MONSTERMOVERNG] ROLLBACK trigger=%s reservation=downgraded-to-replay-lease randomLive=restored-pre-refill\n",
                           trigger);
                }
                servicePostMoveGoal(doomRpgBase, trigger, &publish);
            }
            else {
                printf("[MONSTERMOVERNG] DEFER trigger=%s cause=rng-reservation-conflict action=movement-fail-closed\n",
                       trigger);
                EspNativeGameplayMonsterMovement_service(doomRpgBase);
            }
        }
        else {
            EspNativeGameplayMonsterMovement_service(doomRpgBase);
            (void)EspNativeGameplayMonsterMovementPublish_afterProbe(
                doomRpgBase, trigger, NULL, 0U, plannedBefore, &publish);
            servicePostMoveGoal(doomRpgBase, trigger, &publish);
        }
    }
    else {
        EspNativeGameplayMonsterMovement_service(doomRpgBase);
    }

    compositionOwner.observedMovementDeferredTurns = turn->movementDeferredTurns;
    compositionOwner.observedNoAttackTurns = turn->noAttackTurns;
}
