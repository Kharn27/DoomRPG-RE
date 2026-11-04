/* Host regression of the real C native level-exit commit owner.
 * cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
 *   -IESP32/include ESP32/src/esp_map_level_exit_stats.c
 *   ESP32/test/level_exit_completion_host.c -Wl,--gc-sections
 *   -o /tmp/level-exit-completion && /tmp/level-exit-completion
 *
 * The unrelated BSP collector is discarded by gc-sections. This verifies
 * the actual production preflight/commit functions, not duplicated logic.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_level_exit_stats.h"

static uint32_t lastBit;
static uint16_t completed;
static unsigned int markCalls;
static int acceptMark = 1;

int EspNativeGameplayPlayerState_markCompleted(uint32_t bit) {
    ++markCalls;
    lastBit = bit;
    if (!acceptMark || bit == 0U || (bit & ~0x07ffUL) != 0U ||
        (bit & (bit - 1U)) != 0U) return 0;
    completed = (uint16_t)(completed | bit);
    return 1;
}

static EspMapLevelExitStats makeStats(uint8_t mapId, uint8_t showStats,
                                      uint16_t secretsFound,
                                      uint16_t secretsTotal,
                                      uint16_t monstersDead,
                                      uint16_t monstersTotal) {
    EspMapLevelExitStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.loadMapId = mapId;
    stats.showStats = showStats;
    stats.secretsFound = secretsFound;
    stats.secretsTotal = secretsTotal;
    stats.monstersDead = monstersDead;
    stats.monstersTotal = monstersTotal;
    stats.effectFlags = ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME |
                        ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES |
                        ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER |
                        ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR;
    if (showStats && mapId != ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID) {
        stats.completionLevelBit = 1UL << (mapId - 1U);
        stats.markCompleted = 1U;
        stats.markAllSecrets = (uint8_t)(secretsFound == secretsTotal);
        stats.markAllMonsters = (uint8_t)(monstersDead == monstersTotal);
        stats.effectFlags |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED;
        if (stats.markAllSecrets)
            stats.effectFlags |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS;
        if (stats.markAllMonsters)
            stats.effectFlags |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
    }
    return stats;
}

static void rejected(const EspMapLevelExitStats* stats,
                     uint8_t mapId, uint8_t showStats) {
    unsigned int oldCalls = markCalls;
    uint16_t oldCompleted = completed;
    assert(EspMapLevelExitStats_preflightCommit(stats, mapId, showStats) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_INVALID);
    assert(EspMapLevelExitStats_commitCompletion(stats, mapId, showStats) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_INVALID);
    assert(markCalls == oldCalls && completed == oldCompleted);
}

int main(void) {
    EspMapLevelExitStats stats, invalid;

    stats = makeStats(1U, 1U, 2U, 3U, 5U, 5U);
    assert(EspMapLevelExitStats_preflightCommit(&stats, 1U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_READY);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 1U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_APPLIED);
    assert(markCalls == 1U && completed == 1U && lastBit == 1U);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 1U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_APPLIED);
    assert(completed == 1U && markCalls == 2U); /* Idempotent bit. */

    invalid = stats;
    invalid.completionLevelBit = 2U;
    rejected(&invalid, 1U, 1U);
    invalid = stats;
    invalid.markAllSecrets = 1U;
    rejected(&invalid, 1U, 1U);
    invalid = stats;
    invalid.markAllMonsters = 0U;
    rejected(&invalid, 1U, 1U);
    invalid = stats;
    invalid.effectFlags ^= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
    rejected(&invalid, 1U, 1U);
    invalid = stats;
    invalid.secretsFound = 4U;
    rejected(&invalid, 1U, 1U);
    rejected(&stats, 2U, 1U); /* Wrong source gameplay header ID. */
    rejected(&stats, 1U, 0U); /* Forged SHOWSTATS. */
    rejected(&stats, 0U, 1U);
    rejected(&stats, 1U, 2U);
    rejected(NULL, 1U, 1U);

    stats = makeStats(2U, 1U, 0U, 0U, 0U, 0U);
    assert(EspMapLevelExitStats_preflightCommit(&stats, 2U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_SKIPPED);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 2U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_SKIPPED);
    stats = makeStats(3U, 0U, 1U, 2U, 0U, 1U);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 3U, 0U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_SKIPPED);
    assert(completed == 1U && markCalls == 2U);

    stats = makeStats(11U, 1U, 0U, 0U, 0U, 0U);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 11U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_APPLIED);
    assert(completed == (uint16_t)(1U | (1U << 10U)));
    stats = makeStats(12U, 1U, 0U, 0U, 0U, 0U);
    rejected(&stats, 12U, 1U); /* Above V11 completion mask. */
    stats = makeStats(32U, 1U, 0U, 0U, 0U, 0U);
    rejected(&stats, 32U, 1U);

    stats = makeStats(4U, 1U, 0U, 0U, 0U, 0U);
    acceptMark = 0;
    assert(EspMapLevelExitStats_preflightCommit(&stats, 4U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_READY);
    assert(EspMapLevelExitStats_commitCompletion(&stats, 4U, 1U) ==
           ESP_MAP_LEVEL_EXIT_COMMIT_PLAYER_REJECTED);
    assert(completed == (uint16_t)(1U | (1U << 10U)));
    puts("level exit native completion: preflight/commit/showstats/hub/V11/rollback PASS");
    return 0;
}
