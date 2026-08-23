#ifndef DOOMRPG_ESP32_POST_LOAD_VIEW_INVALIDATION_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_VIEW_INVALIDATION_STATE_H

#include <stdint.h>

#include "esp_post_load_event_particle_cleanup_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadViewInvalidationStatus_e {
    ESP_POST_LOAD_VIEW_INVALIDATION_INVALID = 0,
    ESP_POST_LOAD_VIEW_INVALIDATION_CLEANUP_INVALID = 1,
    ESP_POST_LOAD_VIEW_INVALIDATION_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_VIEW_INVALIDATION_WORLD_NOT_READY = 3,
    ESP_POST_LOAD_VIEW_INVALIDATION_ALREADY_ACTIVE = 4,
    ESP_POST_LOAD_VIEW_INVALIDATION_OK = 5
} EspPostLoadViewInvalidationStatus;

/*
 * Exact caller-order semantic owner for:
 *
 *   doomCanvas->isUpdateView = true;
 *
 * The permanent ESP32 path records only the redraw-request state transition.
 * It does not mutate legacy DoomCanvas_t, render a frame, enter ST_PLAYING or
 * update idleTime. The temporary hardware probe supplies the incoming scalar
 * read-only until a broader native presentation-state owner exists.
 */
typedef struct EspPostLoadViewInvalidationState_s {
    uint8_t isUpdateViewBefore;
    uint8_t isUpdateViewAfter;
    uint8_t targetMapId;
    uint8_t active;
} EspPostLoadViewInvalidationState;

void EspPostLoadViewInvalidation_reset(void);
int EspPostLoadViewInvalidation_isReady(void);
const EspPostLoadViewInvalidationState* EspPostLoadViewInvalidation_view(void);

EspPostLoadViewInvalidationStatus EspPostLoadViewInvalidation_prepare(
    const EspPostLoadEventParticleCleanupState* cleanup,
    uint8_t isUpdateViewBefore,
    EspPostLoadViewInvalidationState* outState);

EspPostLoadViewInvalidationStatus EspPostLoadViewInvalidation_route(
    uint8_t isUpdateViewBefore);

#ifdef __cplusplus
}
#endif

#endif
