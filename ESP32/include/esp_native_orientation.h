#ifndef DOOMRPG_ESP32_NATIVE_ORIENTATION_H
#define DOOMRPG_ESP32_NATIVE_ORIENTATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_ORIENTATION_NORMAL 0U
#define ESP32_ORIENTATION_INVERSE 1U

/* 320x240 physical landscape output in both modes; never rotates the logical
 * 160x120 RGB565 framebuffer or any game/world coordinates. */
static inline int Esp32Orientation_valid(uint8_t orientation) {
    return orientation == ESP32_ORIENTATION_NORMAL ||
           orientation == ESP32_ORIENTATION_INVERSE;
}

/* Pure touch transform after NORMAL raw-XPT calibration, exactly once.
 * The hardware TFT rotation does not transform XPT2046 samples. */
static inline int Esp32Orientation_transformTouch(uint8_t orientation,
                                                  int16_t* x, int16_t* y) {
    if (!Esp32Orientation_valid(orientation) || x == 0 || y == 0 ||
        *x < 0 || *x >= 320 || *y < 0 || *y >= 240) return 0;
    if (orientation == ESP32_ORIENTATION_INVERSE) {
        *x = (int16_t)(319 - *x);
        *y = (int16_t)(239 - *y);
    }
    return 1;
}

/* PlatformVideo alone owns TFT_eSPI setRotation(1/3), gamma and actual state.
 * PlatformInput reads this state; there is no second orientation shadow. */
int Esp32Orientation_isReady(void);
uint8_t Esp32Orientation_get(void);
int Esp32Orientation_set(uint8_t orientation);

#ifdef __cplusplus
}
#endif

#endif
