#ifndef ESP_NATIVE_WALL_INDICATOR_ANIMATION_H
#define ESP_NATIVE_WALL_INDICATOR_ANIMATION_H
#include <stdint.h>

/* UI presentation of the original immutable four-frame PAK wall resources.
 * Logical 9 is LOCKED/red; 10 is UNLOCKED/green. This animation does NOT
 * decide or mutate which logical texture a line owns. The native line
 * texture-state adapter remains the only authority for that transition.
 *
 * Absolute monotonic wall clock: there is no phase reset on MOVE/TURN, and
 * rendering a fresh player pose cannot extend a blink interval.
 *
 * Red breathes slowly through 0,0,1,2,3,3,2,1: 360ms per step,
 * 720ms resting at either extremum, 2880ms for a complete breath.
 * Green stays alert at 0,1,2,3: 144ms per step, 576ms/cycle.
 * The visibly changing red steps are 2.5x slower than green's, and a
 * complete red breath takes 5 green cycles. No synthetic colors or texels:
 * only the four genuine PAK frames and a bounded, demand-driven redraw. */
#define ESP_NATIVE_WALL_INDICATOR_RED_MS 360U
#define ESP_NATIVE_WALL_INDICATOR_GREEN_MS 144U
#define ESP_NATIVE_WALL_INDICATOR_RED_BIT 0x01U
#define ESP_NATIVE_WALL_INDICATOR_GREEN_BIT 0x02U

static inline uint8_t EspNativeWallIndicator_bit(uint16_t logical) {
    return logical == 9U ? ESP_NATIVE_WALL_INDICATOR_RED_BIT
         : logical == 10U ? ESP_NATIVE_WALL_INDICATOR_GREEN_BIT : 0U;
}
static inline int EspNativeWallIndicator_isAnimated(uint16_t logical) {
    return logical == 9U || logical == 10U;
}
static inline uint8_t EspNativeWallIndicator_phase(uint16_t logical,
                                                  uint32_t nowMs) {
    const uint32_t tick = nowMs + (uint32_t)logical * 3U;
    if (logical == 9U) {
        static const uint8_t redBreath[8] =
            {0U, 0U, 1U, 2U, 3U, 3U, 2U, 1U};
        return redBreath[(tick / ESP_NATIVE_WALL_INDICATOR_RED_MS) % 8U];
    }
    if (logical == 10U) {
        return (uint8_t)((tick / ESP_NATIVE_WALL_INDICATOR_GREEN_MS) & 3U);
    }
    return 0U;
}

/* Last successful world frame carries which colors actually hit the wall
 * raster and when their phase was sampled. Compare phases, NOT elapsed time
 * since the last presentation: any movement may present within a phase, but
 * it must never postpone the next visual transition. */
static inline int EspNativeWallIndicator_changed(uint8_t visibleMask,
                                                 uint32_t lastSampleMs,
                                                 uint32_t nowMs) {
    return ((visibleMask & ESP_NATIVE_WALL_INDICATOR_RED_BIT) != 0U &&
            EspNativeWallIndicator_phase(9U, lastSampleMs) !=
                EspNativeWallIndicator_phase(9U, nowMs)) ||
           ((visibleMask & ESP_NATIVE_WALL_INDICATOR_GREEN_BIT) != 0U &&
            EspNativeWallIndicator_phase(10U, lastSampleMs) !=
                EspNativeWallIndicator_phase(10U, nowMs));
}
#endif
