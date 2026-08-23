#ifndef DOOMRPG_ESP32_STATS_MENU_INTENT_H
#define DOOMRPG_ESP32_STATS_MENU_INTENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_STATS_MENU_FIRST_MAP_ID 1U
#define ESP_STATS_MENU_END_GAME_MAP_ID 13U
#define ESP_STATS_MENU_LAST_MAP_ID ESP_STATS_MENU_END_GAME_MAP_ID

#define ESP_STATS_MENU_KIND_NONE 0U
#define ESP_STATS_MENU_KIND_LEVEL 1U
#define ESP_STATS_MENU_KIND_OVERALL 2U

typedef enum EspStatsMenuIntentStatus_e {
    ESP_STATS_MENU_INTENT_INVALID = 0,
    ESP_STATS_MENU_INTENT_NOT_APPLICABLE = 1,
    ESP_STATS_MENU_INTENT_OK = 2
} EspStatsMenuIntentStatus;

/*
 * Pointer-free native projection of the menu writes in the show-stats branch
 * of recovered Game_changeMap().
 *
 * Legacy semantics after Player_addLevelStats(true):
 *   menu->mapNameId = targetMapId;
 *   MenuSystem_setMenu(target == MAP_END_GAME
 *                      ? MENU_MAP_STATS_OVERALL
 *                      : MENU_MAP_STATS);
 *   game->changeMapParam = 0;
 *
 * This value owns only the semantic intent. It never references Menu_t,
 * MenuSystem_t or Game_t and does not render or load a map. Target-map name
 * resolution is intentionally a separate transition/catalog responsibility.
 */
typedef struct EspStatsMenuIntent_s {
    uint8_t targetMapId;
    uint8_t menuKind;
    uint8_t active;
    uint8_t consumePending;
} EspStatsMenuIntent;

void EspStatsMenuIntent_reset(EspStatsMenuIntent* intent);

EspStatsMenuIntentStatus EspStatsMenuIntent_prepare(
    uint8_t targetMapId,
    uint8_t showStats,
    EspStatsMenuIntent* outIntent);

#ifdef __cplusplus
}
#endif

#endif
