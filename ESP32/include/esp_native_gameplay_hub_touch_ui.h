#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Visible touch chrome for the native gameplay HUB.
 *
 * This layer owns no gameplay state and no framebuffer snapshot. It paints only
 * inside the HUB world viewport (logical y=20..99) and translates its visible
 * button rectangles back into the existing semantic gameplay action IDs.
 */
int EspNativeGameplayHubTouchUi_paint(uint16_t* framebuffer,
                                      uint8_t page,
                                      uint8_t selectedRow);

struct EspNativeGameplayTouchHit_s;
int EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHit);

#ifdef __cplusplus
}
#endif

#endif
