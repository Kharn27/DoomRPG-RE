#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_TRANSITION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_TRANSITION_H

#include <stdint.h>

#include "esp_map_committed_transition.h"
#include "esp_map_level_exit_stats.h"
#include "esp_map_save_route.h"
#include "esp_map_transition_preflight.h"
#include "esp_native_gameplay_input.h"
#include "esp_stats_menu_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayTransitionStatus_e {
    ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE = 1,
    ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY = 2,
    ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX = 3,
    ESP_NATIVE_GAMEPLAY_TRANSITION_UNSUPPORTED = 4,
    ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED = 5,
    ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS = 6
} EspNativeGameplayTransitionStatus;

/*
 * Small durable owner for the non-destructive half of legacy
 * SAVEGAME -> CHANGEMAP. No map-sized payload or source-map pointer is retained.
 *
 * WAIT_STATS is intentionally a real pause boundary: target BSP identity has
 * already been preflighted and the CHANGEMAP pending value has been consumed,
 * but the source resident runtime remains untouched until a later stats-menu
 * milestone acknowledges the pause and explicitly commits the handoff.
 */
typedef struct EspNativeGameplayTransitionState_s {
    EspMapSaveRouteState saveRoute;
    EspMapChangeMapResult changeResult;
    EspMapTransitionPreflightResult targetPreflight;
    EspMapLevelExitStats levelStats;
    EspStatsMenuIntent statsIntent;
    EspMapCommittedTransitionState committed;
    uint32_t sequence;
    uint16_t frontTile;
    uint16_t eventIndex;
    uint8_t saveCommandOffset;
    uint8_t changeCommandOffset;
    uint8_t active;
    uint8_t waitingStats;
} EspNativeGameplayTransitionState;

typedef struct EspNativeGameplayTransitionSelectResult_s {
    uint32_t sequence;
    uint16_t frontTile;
    uint16_t eventIndex;
    uint8_t eligibleCount;
    uint8_t saveCommandOffset;
    uint8_t changeCommandOffset;
    uint8_t targetMapId;
    uint8_t targetGameplayLoadMapId;
    uint8_t showStats;
    uint8_t committedPhase;
} EspNativeGameplayTransitionSelectResult;

void EspNativeGameplayTransition_reset(void);
int EspNativeGameplayTransition_isWaitingStats(void);
const EspNativeGameplayTransitionState* EspNativeGameplayTransition_view(void);

/*
 * Recognize and execute only the bounded SELECT transition family whose
 * eligible commands are exactly SAVEGAME (27) followed by CHANGEMAP (2).
 *
 * A non-transition SELECT returns NOT_APPLICABLE without changing any owner so
 * the existing door/dialog/entity path can run unchanged. A matched transition
 * performs native SAVEGAME route capture, CHANGEMAP pending creation, target
 * BSP preflight, map-derived level-stat collection and committed-transition
 * begin. This milestone supports only showStats transitions and stops at
 * WAIT_STATS with the source map still resident and the PAK logically closed.
 */
EspNativeGameplayTransitionStatus EspNativeGameplayTransition_trySelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayTransitionSelectResult* outResult);

const char* EspNativeGameplayTransition_statusName(
    EspNativeGameplayTransitionStatus status);

#ifdef __cplusplus
}
#endif

#endif
