#ifndef DOOMRPG_ESP32_POST_LOAD_PLAYING_TRANSITION_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_PLAYING_TRANSITION_STATE_H

#include <stdint.h>

#include "esp_post_load_view_invalidation_state.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ESP_POST_LOAD_PLAYING_STATE_INTRO = 9,
    ESP_POST_LOAD_PLAYING_STATE_PLAYING = 3,
    ESP_POST_LOAD_PLAYING_SOFTKEY_NONE = 0,
    ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP = 1
};

typedef enum EspPostLoadPlayingTransitionStatus_e {
    ESP_POST_LOAD_PLAYING_TRANSITION_INVALID = 0,
    ESP_POST_LOAD_PLAYING_TRANSITION_VIEW_INVALID = 1,
    ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_PLAYING_TRANSITION_WORLD_NOT_READY = 3,
    ESP_POST_LOAD_PLAYING_TRANSITION_ALREADY_ACTIVE = 4,
    ESP_POST_LOAD_PLAYING_TRANSITION_OK = 5
} EspPostLoadPlayingTransitionStatus;

/*
 * Native semantic owner for the exact ST_INTRO -> ST_PLAYING effects of:
 *
 *   DoomCanvas_setState(doomCanvas, ST_PLAYING);
 *
 * On this caller path the legacy function has four relevant effects:
 *   - state: ST_INTRO -> ST_PLAYING
 *   - restoreSoftKeys is cleared because the state changes
 *   - when !monstersTurn, DoomCanvas_drawSoftKeys("Menu", "Map") is requested
 *   - skipCheckState is forced true
 *
 * DoomCanvas_drawSoftKeys() itself is renderer/UI legacy code. The permanent
 * ESP32 path therefore records its semantic intent and whether presentation
 * would be visible, but never draws or mutates legacy DoomCanvas/Game/Hud.
 */
typedef struct EspPostLoadPlayingTransitionState_s {
    uint8_t stateBefore;
    uint8_t stateAfter;
    uint8_t monstersTurnBefore;
    uint8_t displaySoftKeysBefore;
    uint8_t restoreSoftKeysBefore;
    uint8_t restoreSoftKeysAfter;
    uint8_t skipCheckStateBefore;
    uint8_t skipCheckStateAfter;
    uint8_t softKeyIntent;
    uint8_t softKeyPresentationDeferred;
    uint8_t targetMapId;
    uint8_t active;
} EspPostLoadPlayingTransitionState;

void EspPostLoadPlayingTransition_reset(void);
int EspPostLoadPlayingTransition_isReady(void);
const EspPostLoadPlayingTransitionState* EspPostLoadPlayingTransition_view(void);

EspPostLoadPlayingTransitionStatus EspPostLoadPlayingTransition_prepare(
    const EspPostLoadViewInvalidationState* viewInvalidation,
    uint8_t monstersTurnBefore,
    uint8_t displaySoftKeysBefore,
    uint8_t restoreSoftKeysBefore,
    uint8_t skipCheckStateBefore,
    EspPostLoadPlayingTransitionState* outState);

EspPostLoadPlayingTransitionStatus EspPostLoadPlayingTransition_route(
    uint8_t monstersTurnBefore,
    uint8_t displaySoftKeysBefore,
    uint8_t restoreSoftKeysBefore,
    uint8_t skipCheckStateBefore);

#ifdef __cplusplus
}
#endif

#endif
