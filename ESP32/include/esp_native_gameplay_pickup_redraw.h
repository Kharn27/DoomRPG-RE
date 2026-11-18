#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_REDRAW_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_REDRAW_H

#include <stdint.h>

/*
 * Presentation-only single-slot coalescer. The resource, world-removal and
 * player-state transactions commit in their original order; only the redundant
 * full settled frame can be postponed while the player keeps moving.
 *
 * A normal settled world frame fulfills the pending redraw at no extra cost.
 * One idle redraw is permitted after a quiet window; no heap allocation,
 * thread/ISR, screen copy, or additional gameplay turn is introduced.
 */
#define ESP_PICKUP_REDRAW_QUIET_MS 600U

typedef struct EspPickupRedraw_s {
    uint32_t lastPickupMs;
    uint8_t pending;
} EspPickupRedraw;

static inline void EspPickupRedraw_schedule(EspPickupRedraw* state,
                                            uint32_t nowMs) {
    if (state == 0) return;
    state->lastPickupMs = nowMs;
    state->pending = 1U;
}

static inline int EspPickupRedraw_due(const EspPickupRedraw* state,
                                     uint32_t nowMs) {
    return state != 0 && state->pending != 0U &&
           (uint32_t)(nowMs - state->lastPickupMs) >=
               ESP_PICKUP_REDRAW_QUIET_MS;
}

static inline int EspPickupRedraw_satisfy(EspPickupRedraw* state) {
    if (state == 0 || state->pending == 0U) return 0;
    state->pending = 0U;
    return 1;
}

#endif
