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
