#ifndef DOOMRPG_ESP32_POST_LOAD_IDLE_TIME_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_IDLE_TIME_STATE_H

#include <stdint.h>

#include "esp_post_load_playing_transition_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadIdleTimeStatus_e {
    ESP_POST_LOAD_IDLE_TIME_INVALID = 0,
    ESP_POST_LOAD_IDLE_TIME_PLAYING_INVALID = 1,
    ESP_POST_LOAD_IDLE_TIME_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_IDLE_TIME_WORLD_NOT_READY = 3,
    ESP_POST_LOAD_IDLE_TIME_ALREADY_ACTIVE = 4,
    ESP_POST_LOAD_IDLE_TIME_OK = 5
} EspPostLoadIdleTimeStatus;

/*
 * Exact caller-order semantic owner for the final fresh-map load tail write:
 *
 *   doomCanvas->idleTime = doomCanvas->time + 8000;
 *
 * The permanent ESP32 path owns only the temporal result. It does not mutate
 * legacy DoomCanvas_t, dispatch gameplay, render, present, or allocate.
 */
typedef struct EspPostLoadIdleTimeState_s {
    int32_t timeBefore;
    int32_t idleTimeBefore;
    int32_t idleTimeAfter;
    uint8_t targetMapId;
    uint8_t active;
    uint8_t reserved0;
    uint8_t reserved1;
} EspPostLoadIdleTimeState;

void EspPostLoadIdleTime_reset(void);
int EspPostLoadIdleTime_isReady(void);
const EspPostLoadIdleTimeState* EspPostLoadIdleTime_view(void);

EspPostLoadIdleTimeStatus EspPostLoadIdleTime_prepare(
    const EspPostLoadPlayingTransitionState* playingTransition,
    int32_t timeBefore,
    int32_t idleTimeBefore,
    EspPostLoadIdleTimeState* outState);

EspPostLoadIdleTimeStatus EspPostLoadIdleTime_route(
    int32_t timeBefore,
    int32_t idleTimeBefore);

#ifdef __cplusplus
}
#endif

#endif
