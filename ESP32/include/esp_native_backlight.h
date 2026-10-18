#ifndef DOOMRPG_ESP32_NATIVE_BACKLIGHT_H
#define DOOMRPG_ESP32_NATIVE_BACKLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware PWM of classic CYD TFT_BL GPIO21, independent of RGB565 pixels.
 * Session-only first milestone: no SD/Config/NVS writes. Never allow a
 * zero-duty setting that would leave the touch-only device blind. */
int Esp32Backlight_isReady(void);
uint8_t Esp32Backlight_percent(void);
int Esp32Backlight_setPercent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
