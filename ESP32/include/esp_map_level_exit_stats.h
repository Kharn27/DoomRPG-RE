#ifndef DOOMRPG_ESP32_MAP_LEVEL_EXIT_STATS_H
#define DOOMRPG_ESP32_MAP_LEVEL_EXIT_STATS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID 2U
#define ESP_MAP_LEVEL_EXIT_LINE_FLAG_SECRET 0x00000008UL

#define ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME   0x01U
#define ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES  0x02U
#define ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER   0x04U
#define ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR    0x08U
#define ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED    0x10U
#define ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS  0x20U
#define ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS 0x40U

typedef enum EspMapLevelExitStatsStatus_e {
    ESP_MAP_LEVEL_EXIT_STATS_INVALID = 0,
    ESP_MAP_LEVEL_EXIT_STATS_NOT_READY = 1,
    ESP_MAP_LEVEL_EXIT_STATS_OK = 2
} EspMapLevelExitStatsStatus;

/*
 * Pure caller-owned projection of the map-derived portion of legacy
 * Player_addLevelStats(). No Player/Menu/Game/Render object is referenced.
 *
 * Secrets are counted from immutable line flag 0x8 plus the native mutable
 * OPEN bit. Monsters are native enemy entities; a cleared ALIVE bit is the
 * native death predicate. The completion-bit recommendations reproduce the
 * legacy `z && loadMapID != 2` gate, including equality on zero totals.
 *
 * Time/move accumulation and the berserker/familiar resets are represented as
 * effect flags only; a later native player-state owner will consume them.
 */
typedef struct EspMapLevelExitStats_s {
    uint32_t completionLevelBit;
    uint16_t secretsFound;
    uint16_t secretsTotal;
    uint16_t monstersDead;
    uint16_t monstersTotal;
    uint8_t loadMapId;
    uint8_t showStats;
    uint8_t markCompleted;
    uint8_t markAllSecrets;
    uint8_t markAllMonsters;
    uint8_t effectFlags;
} EspMapLevelExitStats;

EspMapLevelExitStatsStatus EspMapLevelExitStats_collect(
    uint8_t loadMapId,
    uint8_t showStats,
    EspMapLevelExitStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
