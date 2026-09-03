#ifndef DOOMRPG_ESP32_NATIVE_RNG_REPLAY_GUARD_H
#define DOOMRPG_ESP32_NATIVE_RNG_REPLAY_GUARD_H

#include <SDL.h>
#include <stdint.h>

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Temporarily materialize the next post-refill Random_t table for a bounded
 * probe without consuming the live Random_t. If the live state is already at
 * the byte-table boundary, the hidden legacy generator advances at most once
 * and that exact table is reserved for the same live Random_t pointer until its
 * next real DoomRPG_randNextByte() boundary replay.
 *
 * Returns 1 on success. outPrepared is 1 only when liveRandom was temporarily
 * replaced with the reserved post-refill state and must later be restored by
 * EspNativeRngReplayGuard_endProbeBoundary() or committed by
 * EspNativeRngReplayGuard_commitProbeBoundary().
 */
int EspNativeRngReplayGuard_beginProbeBoundary(Random_t* liveRandom,
                                               Random_t* outSaved,
                                               uint8_t* outPrepared);

/* Restore the exact pre-probe Random_t. Returns 1 when the temporary post-refill
 * state remained byte-exact throughout the probe. */
int EspNativeRngReplayGuard_endProbeBoundary(Random_t* liveRandom,
                                             const Random_t* saved,
                                             uint8_t prepared);

/*
 * Close a prepared boundary as a real committed consumer after exactly
 * consumedBytes bytes were consumed from the reserved post-refill table.
 * This clears the persistent reservation without regenerating the hidden table.
 */
int EspNativeRngReplayGuard_commitProbeBoundary(Random_t* liveRandom,
                                                const Random_t* saved,
                                                uint8_t prepared,
                                                uint32_t consumedBytes);

#ifdef __cplusplus
}
#endif

#endif
