#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_FX_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_FX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adopt an already-restored checkpoint monster state without replaying death
 * presentation. Existing dead monsters are marked as historical in the bounded
 * GIB FX owner; monsters that are alive remain eligible for a future real death
 * burst. No gameplay, RNG, topology or renderer state is mutated.
 */
int EspNativeGameplayGibFx_adoptCheckpointState(void);

/* Stateless capture-shaped EV_SPAWNPARTICLES family 3 overlay. The event
 * continuation owns time; this painter owns no lease, RNG or world mutation. */
int EspNativeGameplayGibFx_paintCapture(uint32_t operand, uint32_t ageMs,
    uint32_t durationMs, uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif
