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

void __real_EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* doomRpg);

static int atByteBoundary(const Random_t* rand) {
    return rand != NULL &&
           (rand->nextRand + (int)sizeof(byte)) >= RANDTABLESIZE;
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
    EspNativeGameplayMonsterMovementPublish_reset();
    EspNativeGameplayMonsterMovement_reset();
    EspNativeGameplayMonsterPosition_reset();
}

int EspNativeGameplayMonsterMovementProbe_serviceMember(
    struct DoomRPG_s* doomRpgBase,
    const char* trigger,
    uint8_t* outCommitted) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t plannedBefore;
    EspNativeGameplayMonsterMovementPublishResult publish;
    int publishStatus = 0;

    if (outCommitted != NULL) *outCommitted = 0U;
    if (doomRpg == NULL || trigger == NULL ||
        (strcmp(trigger, "NO-IMMEDIATE-ATTACK") != 0 &&
         strcmp(trigger, "RANGED-AI") != 0)) {
        return 0;
    }

    plannedBefore = plannedMovesNow();
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
            __real_EspNativeGameplayMonsterMovement_service(doomRpgBase);
            publishStatus = EspNativeGameplayMonsterMovementPublish_afterProbe(
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
            if (outCommitted != NULL) *outCommitted = publish.committed;
            return publishStatus;
        }

        printf("[MONSTERMOVERNG] DEFER trigger=%s cause=rng-reservation-conflict action=movement-fail-closed\n",
               trigger);
        __real_EspNativeGameplayMonsterMovement_service(doomRpgBase);
        return 0;
    }

    __real_EspNativeGameplayMonsterMovement_service(doomRpgBase);
    publishStatus = EspNativeGameplayMonsterMovementPublish_afterProbe(
        doomRpgBase, trigger, NULL, 0U, plannedBefore, &publish);
    servicePostMoveGoal(doomRpgBase, trigger, &publish);
    if (outCommitted != NULL) *outCommitted = publish.committed;
    return publishStatus;
}

void EspNativeGameplayMonsterMovementProbe_service(struct DoomRPG_s* doomRpgBase) {
    /* The active-list wrapper owns producer ordering. It invokes serviceMember
     * once per selected monster so the one-capture publisher closes position,
     * topology and RNG before the next member is planned. */
    EspNativeGameplayMonsterMovement_service(doomRpgBase);
}
