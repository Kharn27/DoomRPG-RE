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

/* After the permanent input owner consumes a HUB SELECT tap, recover which of
 * the three visible previous/current/next cards was actually touched. This is
 * a read-only interpretation of the already-consumed compact input state; it
 * adds no queue or target owner. Returns 1 with an exact bounded entry index,
 * otherwise 0 so callers fail closed to the centered semantic selection. */
int EspNativeGameplayHubTouchUi_consumedSelectTarget(
    uint8_t selectedRow,
    uint8_t entryCount,
    uint8_t* outTargetRow);

#ifdef __cplusplus
}
#endif

#endif
