#include <stdint.h>
#include <string.h>

#include "esp_player_exit_state.h"

static int statsAreConsistent(const EspMapLevelExitStats* stats) {
    uint8_t expectedEffects;
    uint32_t expectedBit;
    uint8_t allSecrets;
    uint8_t allMonsters;

    if (stats == NULL || stats->loadMapId == 0U || stats->loadMapId > 32U ||
        stats->showStats > 1U || stats->markCompleted > 1U ||
        stats->markAllSecrets > 1U || stats->markAllMonsters > 1U ||
        stats->secretsFound > stats->secretsTotal ||
        stats->monstersDead > stats->monstersTotal ||
        (stats->effectFlags & (uint8_t)~ESP_PLAYER_EXIT_ALL_EFFECTS) != 0U) {
        return 0;
    }

    expectedEffects = ESP_PLAYER_EXIT_BASE_EFFECTS;
    expectedBit = 0U;
    allSecrets = 0U;
    allMonsters = 0U;

    if (stats->showStats != 0U &&
        stats->loadMapId != ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID) {
        expectedBit = 1UL << (stats->loadMapId - 1U);
        allSecrets = (uint8_t)(stats->secretsFound == stats->secretsTotal);
        allMonsters = (uint8_t)(stats->monstersDead == stats->monstersTotal);
        expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED;
        if (allSecrets != 0U) {
            expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS;
        }
        if (allMonsters != 0U) {
            expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
        }

        if (stats->markCompleted != 1U ||
            stats->markAllSecrets != allSecrets ||
            stats->markAllMonsters != allMonsters ||
            stats->completionLevelBit != expectedBit) {
            return 0;
        }
    }
    else if (stats->markCompleted != 0U || stats->markAllSecrets != 0U ||
             stats->markAllMonsters != 0U ||
             stats->completionLevelBit != 0U) {
        return 0;
    }

    return stats->effectFlags == expectedEffects;
}

void EspPlayerExitState_reset(EspPlayerExitState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

EspPlayerExitApplyStatus EspPlayerExitState_apply(
    EspPlayerExitState* state,
    const EspMapLevelExitStats* stats,
    uint32_t elapsedTimeMs,
    uint32_t levelMoves,
    EspPlayerExitApplyResult* outResult) {
    EspPlayerExitState before;

    if (outResult != NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    if (state == NULL || stats == NULL || outResult == NULL ||
        state->familiarActive > 1U) {
        return ESP_PLAYER_EXIT_APPLY_INVALID;
    }
    if (!statsAreConsistent(stats)) {
        return ESP_PLAYER_EXIT_APPLY_INCONSISTENT_STATS;
    }

    before = *state;

    outResult->totalTimeBefore = before.totalTime;
    outResult->totalMovesBefore = before.totalMoves;
    outResult->completionLevelBit = stats->completionLevelBit;
    outResult->effectFlagsApplied = stats->effectFlags;
    outResult->berserkerReset = (uint8_t)(before.berserkerTics != 0U);
    outResult->familiarCleared = before.familiarActive;

    state->totalTime += elapsedTimeMs;
    state->totalMoves += levelMoves;
    state->berserkerTics = 0U;
    state->familiarActive = 0U;

    if (stats->markCompleted != 0U) {
        state->completedLevels |= stats->completionLevelBit;
    }
    if (stats->markAllSecrets != 0U) {
        state->foundSecretsLevels |= stats->completionLevelBit;
    }
    if (stats->markAllMonsters != 0U) {
        state->killedMonstersLevels |= stats->completionLevelBit;
    }

    outResult->totalTimeAfter = state->totalTime;
    outResult->totalMovesAfter = state->totalMoves;
    outResult->completedChanged =
        (uint8_t)(state->completedLevels != before.completedLevels);
    outResult->secretsChanged =
        (uint8_t)(state->foundSecretsLevels != before.foundSecretsLevels);
    outResult->monstersChanged =
        (uint8_t)(state->killedMonstersLevels != before.killedMonstersLevels);

    return ESP_PLAYER_EXIT_APPLY_OK;
}
