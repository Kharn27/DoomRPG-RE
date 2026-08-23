#ifndef DOOMRPG_ESP32_HUD_POST_LOAD_CLEAR_STATE_H
#define DOOMRPG_ESP32_HUD_POST_LOAD_CLEAR_STATE_H

#include <stdint.h>

#include "esp_player_facing_state.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspHudPostLoadClearStatus_e {
    ESP_HUD_POST_LOAD_CLEAR_INVALID = 0,
    ESP_HUD_POST_LOAD_CLEAR_VIEW_INVALID = 1,
    ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID = 2,
    ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_CONTEXT = 3,
    ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_ORDER = 4,
    ESP_HUD_POST_LOAD_CLEAR_ALREADY_ACTIVE = 5,
    ESP_HUD_POST_LOAD_CLEAR_OK = 6
} EspHudPostLoadClearStatus;

/*
 * Pointer-free native ownership of the three HUD message-channel writes that
 * immediately follow DoomCanvas_finishRotation() in the recovered load-map
 * caller:
 *
 *   Hud.msgCount = 0;
 *   Hud.statBarMessage = NULL;
 *   Hud.logMessage[0] = '\0';
 *
 * This state records the semantic result only. It never stores a Hud_t pointer
 * and never mutates the legacy Hud object. The next caller-side operation
 * (Junction Game_givemap / non-Junction uncoverAutomap) remains outside this
 * owner.
 */
typedef struct EspHudPostLoadClearState_s {
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t messageCount;
    uint8_t statBarMessagePresent;
    uint8_t logMessageLength;
    uint8_t cleared;
    uint8_t active;
} EspHudPostLoadClearState;

void EspHudPostLoadClear_reset(void);
int EspHudPostLoadClear_isReady(void);
const EspHudPostLoadClearState* EspHudPostLoadClear_view(void);

/*
 * Pure fresh-map post-finishRotation translation. All PlayerView follow-ups
 * must already be consumed and the durable facing owner must match the same map
 * identity. Invalid/refused input zeroes outState and performs no mutation.
 */
EspHudPostLoadClearStatus EspHudPostLoadClear_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerFacingState* facing,
    EspHudPostLoadClearState* outState);

/* Park the semantic HUD clear exactly once. No other owner is mutated. */
EspHudPostLoadClearStatus EspHudPostLoadClear_route(void);

#ifdef __cplusplus
}
#endif

#endif
