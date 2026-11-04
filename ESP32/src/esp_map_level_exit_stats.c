#include <stdint.h>
#include <string.h>

#include "esp_map_level_exit_stats.h"
#include "esp_map_line_state.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"

EspMapLevelExitStatsStatus EspMapLevelExitStats_collect(
    uint8_t loadMapId,
    uint8_t showStats,
    EspMapLevelExitStats* outStats) {
    const EspMapRuntimeView* runtime;
    const EspMapLineStateView* lineState;
    const EspMapSpriteTopologyView* topology;
    EspMapLine line;
    uint32_t i;
    uint16_t linkState;
    uint16_t linkOrder;
    uint8_t open;
    uint8_t type;
    uint8_t subtype;
    uint8_t effects;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (outStats == NULL || showStats > 1U || loadMapId == 0U || loadMapId > 32U) {
        return ESP_MAP_LEVEL_EXIT_STATS_INVALID;
    }

    runtime = EspMapRuntime_view();
    lineState = EspMapLineState_view();
    topology = EspMapSpriteTopology_view();
    if (runtime == NULL || lineState == NULL || topology == NULL ||
        lineState->lineCount != runtime->lineCount ||
        topology->spriteCount != runtime->mapSpriteCount) {
        return ESP_MAP_LEVEL_EXIT_STATS_NOT_READY;
    }

    outStats->loadMapId = loadMapId;
    outStats->showStats = showStats;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            memset(outStats, 0, sizeof(*outStats));
            return ESP_MAP_LEVEL_EXIT_STATS_NOT_READY;
        }
        if ((line.flags & ESP_MAP_LEVEL_EXIT_LINE_FLAG_SECRET) == 0U) continue;
        ++outStats->secretsTotal;
        if (!EspMapLineState_getOpen(i, &open)) {
            memset(outStats, 0, sizeof(*outStats));
            return ESP_MAP_LEVEL_EXIT_STATS_NOT_READY;
        }
        if (open != 0U) ++outStats->secretsFound;
    }

    for (i = 0U; i < topology->spriteCount; ++i) {
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            memset(outStats, 0, sizeof(*outStats));
            return ESP_MAP_LEVEL_EXIT_STATS_NOT_READY;
        }
        (void)subtype;
        (void)linkOrder;
        if (type != ESP_MAP_ENTITY_TYPE_ENEMY ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) {
            continue;
        }
        ++outStats->monstersTotal;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) {
            ++outStats->monstersDead;
        }
    }

    effects = ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME |
              ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES |
              ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER |
              ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR;

    if (showStats != 0U && loadMapId != ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID) {
        outStats->completionLevelBit = 1UL << (loadMapId - 1U);
        outStats->markCompleted = 1U;
        outStats->markAllSecrets =
            (uint8_t)(outStats->secretsFound == outStats->secretsTotal);
        outStats->markAllMonsters =
            (uint8_t)(outStats->monstersDead == outStats->monstersTotal);
        effects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED;
        if (outStats->markAllSecrets != 0U) {
            effects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS;
        }
        if (outStats->markAllMonsters != 0U) {
            effects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
        }
    }

    outStats->effectFlags = effects;
    return ESP_MAP_LEVEL_EXIT_STATS_OK;
}

/* The V11 progress suffix stores 11 completion bits + provenance in 2 B.
 * Keep the consumer bound to that proven codec rather than silently dropping
 * upper bits when the C player's compact completion overlay is updated. */
#define ESP_MAP_LEVEL_EXIT_V11_COMPLETION_MASK 0x07ffU

EspMapLevelExitCommitStatus EspMapLevelExitStats_preflightCommit(
    const EspMapLevelExitStats* stats,
    uint8_t sourceGameplayLoadMapId,
    uint8_t showStats) {
    uint8_t expectedEffects;
    uint8_t shouldMark;
    uint32_t expectedBit;

    if (stats == NULL || showStats > 1U ||
        sourceGameplayLoadMapId == 0U || sourceGameplayLoadMapId > 32U ||
        stats->loadMapId != sourceGameplayLoadMapId ||
        stats->showStats != showStats ||
        stats->secretsFound > stats->secretsTotal ||
        stats->monstersDead > stats->monstersTotal) {
        return ESP_MAP_LEVEL_EXIT_COMMIT_INVALID;
    }

    expectedEffects = ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME |
                      ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES |
                      ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER |
                      ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR;
    shouldMark = (uint8_t)(showStats != 0U &&
                            sourceGameplayLoadMapId !=
                                ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID);
    if (!shouldMark) {
        if (stats->completionLevelBit != 0U ||
            stats->markCompleted != 0U ||
            stats->markAllSecrets != 0U ||
            stats->markAllMonsters != 0U ||
            stats->effectFlags != expectedEffects) {
            return ESP_MAP_LEVEL_EXIT_COMMIT_INVALID;
        }
        return ESP_MAP_LEVEL_EXIT_COMMIT_SKIPPED;
    }

    expectedBit = 1UL << (sourceGameplayLoadMapId - 1U);
    if ((expectedBit & ESP_MAP_LEVEL_EXIT_V11_COMPLETION_MASK) == 0U ||
        stats->completionLevelBit != expectedBit ||
        stats->markCompleted != 1U ||
        stats->markAllSecrets !=
            (uint8_t)(stats->secretsFound == stats->secretsTotal) ||
        stats->markAllMonsters !=
            (uint8_t)(stats->monstersDead == stats->monstersTotal)) {
        return ESP_MAP_LEVEL_EXIT_COMMIT_INVALID;
    }

    expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED;
    if (stats->markAllSecrets) {
        expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS;
    }
    if (stats->markAllMonsters) {
        expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
    }
    return stats->effectFlags == expectedEffects
               ? ESP_MAP_LEVEL_EXIT_COMMIT_READY
               : ESP_MAP_LEVEL_EXIT_COMMIT_INVALID;
}

EspMapLevelExitCommitStatus EspMapLevelExitStats_commitCompletion(
    const EspMapLevelExitStats* stats,
    uint8_t sourceGameplayLoadMapId,
    uint8_t showStats) {
    EspMapLevelExitCommitStatus status = EspMapLevelExitStats_preflightCommit(
        stats, sourceGameplayLoadMapId, showStats);
    if (status != ESP_MAP_LEVEL_EXIT_COMMIT_READY) return status;
    return EspNativeGameplayPlayerState_markCompleted(
               stats->completionLevelBit)
               ? ESP_MAP_LEVEL_EXIT_COMMIT_APPLIED
               : ESP_MAP_LEVEL_EXIT_COMMIT_PLAYER_REJECTED;
}
