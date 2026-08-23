#include <string.h>

#include "esp_stats_menu_intent.h"

void EspStatsMenuIntent_reset(EspStatsMenuIntent* intent) {
    if (intent != NULL) memset(intent, 0, sizeof(*intent));
}

EspStatsMenuIntentStatus EspStatsMenuIntent_prepare(
    uint8_t targetMapId,
    uint8_t showStats,
    EspStatsMenuIntent* outIntent) {
    if (outIntent != NULL) memset(outIntent, 0, sizeof(*outIntent));

    if (outIntent == NULL || showStats > 1U ||
        targetMapId < ESP_STATS_MENU_FIRST_MAP_ID ||
        targetMapId > ESP_STATS_MENU_LAST_MAP_ID) {
        return ESP_STATS_MENU_INTENT_INVALID;
    }

    if (showStats == 0U) {
        return ESP_STATS_MENU_INTENT_NOT_APPLICABLE;
    }

    outIntent->targetMapId = targetMapId;
    outIntent->menuKind =
        (uint8_t)(targetMapId == ESP_STATS_MENU_END_GAME_MAP_ID
                      ? ESP_STATS_MENU_KIND_OVERALL
                      : ESP_STATS_MENU_KIND_LEVEL);
    outIntent->active = 1U;
    outIntent->consumePending = 1U;
    return ESP_STATS_MENU_INTENT_OK;
}
