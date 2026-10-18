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
 * Red breathes through 0,1,2,3,2,1 (960ms), slightly quicker than the former
 * 4*256=1024ms cycle; green races through 0,1,2,3 (576ms).
 * Bounded cadence (~6.25 / 6.94 candidate refreshes/s, not fixed FPS).
 * PAK frames keep their original palette and texels. */
#define ESP_NATIVE_WALL_INDICATOR_RED_MS 160U
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
        static const uint8_t redBreath[6] = {0U, 1U, 2U, 3U, 2U, 1U};
        return redBreath[(tick / ESP_NATIVE_WALL_INDICATOR_RED_MS) % 6U];
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
