#ifndef DOOMRPG_ESP32_POST_LOAD_EVENT_PARTICLE_CLEANUP_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_EVENT_PARTICLE_CLEANUP_STATE_H

#include <stdint.h>

#include "esp_post_load_flag_cleanup_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadEventParticleCleanupStatus_e {
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_INVALID = 0,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_FLAG_STATE_INVALID = 1,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_EVENTS_NOT_EMPTY = 3,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_PARTICLES_NOT_EMPTY = 4,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_WORLD_NOT_READY = 5,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_ALREADY_ACTIVE = 6,
    ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_OK = 7
} EspPostLoadEventParticleCleanupStatus;

/*
 * Exact caller-order semantic owner for:
 *
 *   doomCanvas->numEvents = 0;
 *   ParticleSystem_freeAllParticles(doomCanvas->particleSystem);
 *   doomCanvas->numEvents = 0;
 *
 * The current native path does not own legacy queued-event payloads or the
 * pointer-heavy ParticleNode_t pool. This milestone is therefore deliberately
 * fail-closed unless both incoming collections are empty. When they are empty,
 * the legacy sequence is an identity cleanup and can be represented exactly by
 * this small pointer-free owner without mutating legacy DoomCanvas/ParticleSystem.
 */
typedef struct EspPostLoadEventParticleCleanupState_s {
    uint8_t numEventsBefore;
    uint8_t numEventsAfterFirstClear;
    uint8_t particleCountBefore;
    uint8_t particleCountAfterClear;
    uint8_t numEventsAfterSecondClear;
    uint8_t targetMapId;
    uint8_t active;
    uint8_t reserved;
} EspPostLoadEventParticleCleanupState;

void EspPostLoadEventParticleCleanup_reset(void);
int EspPostLoadEventParticleCleanup_isReady(void);
const EspPostLoadEventParticleCleanupState*
EspPostLoadEventParticleCleanup_view(void);

EspPostLoadEventParticleCleanupStatus
EspPostLoadEventParticleCleanup_prepare(
    const EspPostLoadFlagCleanupState* flagCleanup,
    uint8_t numEventsBefore,
    uint8_t particleCountBefore,
    EspPostLoadEventParticleCleanupState* outState);

EspPostLoadEventParticleCleanupStatus
EspPostLoadEventParticleCleanup_route(
    uint8_t numEventsBefore,
    uint8_t particleCountBefore);

#ifdef __cplusplus
}
#endif

#endif
