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
