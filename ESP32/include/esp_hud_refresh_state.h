#ifndef DOOMRPG_ESP32_HUD_REFRESH_STATE_H
#define DOOMRPG_ESP32_HUD_REFRESH_STATE_H

#include <stdint.h>

#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_HUD_REFRESH_REASON_POST_SPAWN 1U

typedef enum EspHudRefreshStatus_e {
    ESP_HUD_REFRESH_INVALID = 0,
    ESP_HUD_REFRESH_VIEW_INVALID = 1,
    ESP_HUD_REFRESH_UNSUPPORTED_CONTEXT = 2,
    ESP_HUD_REFRESH_ALREADY_ACTIVE = 3,
    ESP_HUD_REFRESH_VIEW_CONSUME_FAILED = 4,
    ESP_HUD_REFRESH_OK = 5
} EspHudRefreshStatus;

/*
 * Tiny permanent native equivalent of the recovered `Hud.isUpdate = true`
 * dirty write. It owns only the semantic request; actual HUD rendering remains
 * deferred. No legacy Hud object, framebuffer pointer or allocation is owned.
 */
typedef struct EspHudRefreshState_s {
    uint8_t reason;
    uint8_t refreshPending;
    uint8_t routed;
    uint8_t active;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t reserved;
} EspHudRefreshState;

void EspHudRefresh_reset(void);
int EspHudRefresh_isReady(void);
const EspHudRefreshState* EspHudRefresh_view(void);

/* Pure pointer-free translation of one player/view HUD follow-up. */
EspHudRefreshStatus EspHudRefresh_preparePostSpawn(
    const EspPlayerViewState* playerView,
    EspHudRefreshState* outState);

/*
 * Route the live player/view HUD follow-up into the permanent HUD dirty owner.
 * On success only EspPlayerViewState.hudRefreshPending is consumed. Facing,
 * Player_setup and initial tile-enter remain pending.
 */
EspHudRefreshStatus EspHudRefresh_routePostSpawn(void);

#ifdef __cplusplus
}
#endif

#endif
