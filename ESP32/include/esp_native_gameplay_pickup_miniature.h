#ifndef DOOMRPG_ESP32_PICKUP_MINIATURE_H
#define DOOMRPG_ESP32_PICKUP_MINIATURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PICKUP_MINI_SIZE 20U
#define ESP_PICKUP_MINI_PIXELS (ESP_PICKUP_MINI_SIZE * ESP_PICKUP_MINI_SIZE)
#define ESP_PICKUP_MINI_MASK_BYTES ((ESP_PICKUP_MINI_PIXELS + 7U) / 8U)
#define ESP_PICKUP_MINI_FIFO_CAP 16U

/* Presentation-only RGB565 lift for the tiny 20x20 pickup glyph. Approx.
 * +50% per nonzero channel, saturating at native channel maximum. Black
 * outline, source hue, transparent mask and the world renderer stay intact.
 * Integer-only: evaluated once per decoded icon pixel, never per blink. */
static inline uint16_t EspPickupMini_brighten565(uint16_t source) {
    uint32_t r = (uint32_t)(source >> 11);
    uint32_t g = (uint32_t)((source >> 5) & 63U);
    uint32_t b = (uint32_t)(source & 31U);
    r = (r * 3U + 1U) / 2U;
    g = (g * 3U + 1U) / 2U;
    b = (b * 3U + 1U) / 2U;
    if (r > 31U) r = 31U;
    if (g > 63U) g = 63U;
    if (b > 31U) b = 31U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Match the visual identity of the pickup, not the immutable container
 * record. Destroyed crates keep their original BSP sprite but the native
 * crate overlay substitutes a new effective entity-def tile in the renderer.
 * Monster drops pass their def tile as sourceLogical with transformed=0.
 * CROSS even IDs use the actual art frame at logical-1. Cosmetic fail-closed. */
static inline int EspPickupMini_resolveVisualLogical(
    uint16_t sourceLogical,
    uint16_t effectiveDefTile,
    int transformed,
    uint16_t* outLogical) {
    uint16_t logical = transformed ? effectiveDefTile : sourceLogical;
    if (outLogical == 0 || logical >= 256U) return 0;
    if (logical >= 82U && logical <= 90U && (logical & 1U) == 0U)
        --logical;
    *outLogical = logical;
    return 1;
}

/* Cosmetic-only FIFO; all calls are on the native gameplay loop task. */
void EspPickupMini_reset(void);
void EspPickupMini_enqueue(uint16_t sourceLogical);
void EspPickupMini_markFreshFrame(void);
void EspPickupMini_beforePresent(uint32_t nowMs);
void EspPickupMini_afterPresent(uint32_t nowMs);
int EspPickupMini_due(uint32_t nowMs);

#ifdef __cplusplus
}
#endif
#endif
