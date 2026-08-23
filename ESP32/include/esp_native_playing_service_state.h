#ifndef DOOMRPG_ESP32_NATIVE_PLAYING_SERVICE_STATE_H
#define DOOMRPG_ESP32_NATIVE_PLAYING_SERVICE_STATE_H

#include <stdint.h>

#include "esp_post_load_idle_time_state.h"
#include "esp_post_load_playing_transition_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativePlayingServiceStatus_e {
    ESP_NATIVE_PLAYING_SERVICE_INVALID = 0,
    ESP_NATIVE_PLAYING_SERVICE_PLAYING_INVALID = 1,
    ESP_NATIVE_PLAYING_SERVICE_IDLE_INVALID = 2,
    ESP_NATIVE_PLAYING_SERVICE_INPUT_PENDING = 3,
    ESP_NATIVE_PLAYING_SERVICE_WORLD_NOT_READY = 4,
    ESP_NATIVE_PLAYING_SERVICE_ALREADY_ACTIVE = 5,
    ESP_NATIVE_PLAYING_SERVICE_OK = 6
} EspNativePlayingServiceStatus;

/*
 * Small permanent boundary for the first native PLAYING service iteration.
 *
 * This is deliberately not the renderer and not gameplay. It proves that the
 * hardware-proven native ST_PLAYING + idle-time owners can be consumed by a
 * native loop/dispatch layer while legacy DoomCanvas remains parked at
 * ST_INTRO. The first iteration is accepted only with an empty input queue;
 * gameplay mutation is deferred, while a render/presentation intent is
 * recorded for the next bounded milestone.
 */
typedef struct EspNativePlayingServiceState_s {
    uint8_t nativeState;
    uint8_t serviceOrdinal;
    uint8_t inputCountBefore;
    uint8_t inputConsumed;
    uint8_t gameplayDispatched;
    uint8_t renderIntent;
    uint8_t renderDeferred;
    uint8_t presentationDeferred;
    uint8_t hudIntent;
    uint8_t targetMapId;
    uint8_t active;
    uint8_t reserved;
} EspNativePlayingServiceState;

void EspNativePlayingService_reset(void);
int EspNativePlayingService_isReady(void);
const EspNativePlayingServiceState* EspNativePlayingService_view(void);

EspNativePlayingServiceStatus EspNativePlayingService_prepare(
    const EspPostLoadPlayingTransitionState* playingTransition,
    const EspPostLoadIdleTimeState* idleTime,
    uint8_t inputCountBefore,
    EspNativePlayingServiceState* outState);

EspNativePlayingServiceStatus EspNativePlayingService_route(
    uint8_t inputCountBefore);

#ifdef __cplusplus
}
#endif

#endif
