#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
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

void EspNativeGameplayMonsterMovementProbe_reset(void) {
    memset(&compositionOwner, 0, sizeof(compositionOwner));
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
    if (producerAdvanced && doomRpg != NULL && atByteBoundary(&doomRpg->random)) {
        Random_t saved;
        uint8_t prepared = 0U;
        int restoredExact;

        if (EspNativeRngReplayGuard_beginProbeBoundary(&doomRpg->random,
                                                       &saved,
                                                       &prepared)) {
            printf("[MONSTERMOVERNG] ARM trigger=%s next=127->0 prepared=%u liveRandom=temporary-post-refill reservation=persistent\n",
                   trigger, (unsigned int)prepared);
            EspNativeGameplayMonsterMovement_service(doomRpgBase);
            restoredExact = EspNativeRngReplayGuard_endProbeBoundary(
                &doomRpg->random, &saved, prepared);
            printf("[MONSTERMOVERNG] RESTORE trigger=%s randomLiveExact=%s reservation=pending-until-real-byte-draw\n",
                   trigger, restoredExact ? "yes" : "NO");
        }
        else {
            printf("[MONSTERMOVERNG] DEFER trigger=%s cause=rng-reservation-conflict action=movement-fail-closed\n",
                   trigger);
            EspNativeGameplayMonsterMovement_service(doomRpgBase);
        }
    }
    else {
        EspNativeGameplayMonsterMovement_service(doomRpgBase);
    }

    compositionOwner.observedMovementDeferredTurns = turn->movementDeferredTurns;
    compositionOwner.observedNoAttackTurns = turn->noAttackTurns;
}
