#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_destructible.h"
#include "esp_native_gameplay_monster_turn.h"

#define DESTRUCTIBLE_TURN_SEQ_TAG 0xd0000000UL

typedef struct DestructibleTurnBridge_s {
    uint16_t eventTile;
    uint16_t lineIndex;
    uint32_t armed;
    uint32_t cancelled;
    uint32_t requested;
    uint8_t pending;
    uint8_t reserved[3];
} DestructibleTurnBridge;

static DestructibleTurnBridge destructibleTurn;

EspNativeGameplayDestructibleStatus
__real_EspNativeGameplayDestructible_executeLineDeath(
    uint16_t eventTile,
    uint16_t expectedLineIndex,
    EspNativeGameplayDestructibleResult* outResult);
int __real_EspNativeGameplayDestructible_rollbackLineDeath(
    const EspNativeGameplayDestructibleResult* result);

/* A successful line-death transaction is still rollback-capable while the
 * action engine composes its remaining presentation/commit work. Arm only a
 * tiny intent here. The intent is converted into a monster-turn request later,
 * when the downstream MonsterTurn view is first observed after action service.
 * If action composition rolls the line death back first, the rollback wrapper
 * cancels the intent exactly. */
EspNativeGameplayDestructibleStatus
__wrap_EspNativeGameplayDestructible_executeLineDeath(
    uint16_t eventTile,
    uint16_t expectedLineIndex,
    EspNativeGameplayDestructibleResult* outResult) {
    EspNativeGameplayDestructibleStatus status =
        __real_EspNativeGameplayDestructible_executeLineDeath(
            eventTile, expectedLineIndex, outResult);

    if (status == ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK && outResult != NULL &&
        outResult->mutated != 0U && outResult->rollbackAvailable != 0U) {
        destructibleTurn.eventTile = eventTile;
        destructibleTurn.lineIndex = expectedLineIndex;
        destructibleTurn.pending = 1U;
        ++destructibleTurn.armed;
        printf("[DESTRUCTIBLETURN] ARM n=%u line=%u eventTile=%u source=player-attack legacyAdvanceTurn=yes rollback=armed request=deferred-until-action-service-closed\n",
               (unsigned int)destructibleTurn.armed,
               (unsigned int)expectedLineIndex,
               (unsigned int)eventTile);
    }
    return status;
}

int __wrap_EspNativeGameplayDestructible_rollbackLineDeath(
    const EspNativeGameplayDestructibleResult* result) {
    int rolledBack = __real_EspNativeGameplayDestructible_rollbackLineDeath(result);
    if (rolledBack && destructibleTurn.pending != 0U && result != NULL &&
        result->lineIndex == destructibleTurn.lineIndex) {
        destructibleTurn.pending = 0U;
        ++destructibleTurn.cancelled;
        printf("[DESTRUCTIBLETURN] CANCEL n=%u line=%u cause=line-death-rollback monsterTurn=request-not-issued\n",
               (unsigned int)destructibleTurn.cancelled,
               (unsigned int)result->lineIndex);
    }
    return rolledBack;
}

/* Called from the existing MonsterTurn-view composition boundary. At that
 * point the action service that armed the line death has returned through all
 * of its rollback edges. requestPassTurn is deliberately reused only as a
 * transport into the already-proven Game_advanceTurn-equivalent producer; the
 * log records the true legacy cause as PLAYER_ATTACK. */
int EspNativeGameplayDestructibleTurn_flush(void) {
    uint32_t sequence;
    if (destructibleTurn.pending == 0U) return 1;

    sequence = DESTRUCTIBLE_TURN_SEQ_TAG | (uint32_t)destructibleTurn.lineIndex;
    if (!EspNativeGameplayMonsterTurn_requestPassTurn(sequence)) {
        printf("[DESTRUCTIBLETURN] DEFER line=%u cause=monster-turn-request-busy pending=retained\n",
               (unsigned int)destructibleTurn.lineIndex);
        return 0;
    }

    destructibleTurn.pending = 0U;
    ++destructibleTurn.requested;
    printf("[DESTRUCTIBLETURN] REQUEST n=%u line=%u eventTile=%u legacyReason=PLAYER_ATTACK transport=PASS_TURN taggedSeq=%08x rollbackWindow=closed monsterTurn=requested\n",
           (unsigned int)destructibleTurn.requested,
           (unsigned int)destructibleTurn.lineIndex,
           (unsigned int)destructibleTurn.eventTile,
           (unsigned int)sequence);
    return 1;
}
