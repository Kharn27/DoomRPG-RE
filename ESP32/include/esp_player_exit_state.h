#ifndef DOOMRPG_ESP32_PLAYER_EXIT_STATE_H
#define DOOMRPG_ESP32_PLAYER_EXIT_STATE_H

#include <stdint.h>

#include "esp_map_level_exit_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_EXIT_BASE_EFFECTS \
    (ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME | \
     ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES | \
     ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER | \
     ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR)

#define ESP_PLAYER_EXIT_ALL_EFFECTS \
    (ESP_PLAYER_EXIT_BASE_EFFECTS | \
     ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED | \
     ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS | \
     ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS)

typedef enum EspPlayerExitApplyStatus_e {
    ESP_PLAYER_EXIT_APPLY_INVALID = 0,
    ESP_PLAYER_EXIT_APPLY_INCONSISTENT_STATS = 1,
    ESP_PLAYER_EXIT_APPLY_OK = 2
} EspPlayerExitApplyStatus;

/*
 * Small pointer-free owner for exactly the Player fields mutated by recovered
 * Player_addLevelStats(). Per-level start time and move count remain owned by
 * the future native gameplay/player core and are supplied explicitly to apply.
 * familiarActive is semantic presence only; no Entity pointer is retained.
 */
typedef struct EspPlayerExitState_s {
    uint32_t totalTime;
    uint32_t totalMoves;
    uint32_t completedLevels;
    uint32_t killedMonstersLevels;
    uint32_t foundSecretsLevels;
    uint32_t berserkerTics;
    uint8_t familiarActive;
    uint8_t reserved[3];
} EspPlayerExitState;

/* Caller-owned proof/result metadata; no heap allocation is performed. */
typedef struct EspPlayerExitApplyResult_s {
    uint32_t totalTimeBefore;
    uint32_t totalTimeAfter;
    uint32_t totalMovesBefore;
    uint32_t totalMovesAfter;
    uint32_t completionLevelBit;
    uint8_t effectFlagsApplied;
    uint8_t completedChanged;
    uint8_t secretsChanged;
    uint8_t monstersChanged;
    uint8_t berserkerReset;
    uint8_t familiarCleared;
    uint8_t reserved[2];
} EspPlayerExitApplyResult;

void EspPlayerExitState_reset(EspPlayerExitState* state);

/*
 * Consume one already-validated native level-exit stats value and apply the
 * corresponding Player_addLevelStats() writes to native state only.
 *
 * elapsedTimeMs is the caller-owned equivalent of `now - player->time` and
 * levelMoves is the caller-owned equivalent of `player->moves`. uint32_t
 * arithmetic deliberately preserves the 32-bit wrap behavior of the original
 * ESP32 integer fields. Invalid/inconsistent input is fail-closed and leaves
 * state unchanged.
 */
EspPlayerExitApplyStatus EspPlayerExitState_apply(
    EspPlayerExitState* state,
    const EspMapLevelExitStats* stats,
    uint32_t elapsedTimeMs,
    uint32_t levelMoves,
    EspPlayerExitApplyResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
