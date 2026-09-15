#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Visible touch chrome for the native gameplay HUB. This layer owns no gameplay
 * state and no framebuffer snapshot. It paints only logical y=20..99 and maps
 * the INV / WPN / STAT tabs plus page-local touch targets to semantic actions. */
int EspNativeGameplayHubTouchUi_paint(uint16_t* framebuffer,
                                      uint8_t page,
                                      uint8_t selectedRow);

struct EspNativeGameplayTouchHit_s;
int EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHit);

/* Read the exact weapon cell targeted by an already-consumed HUB SELECT input.
 * No target queue/owner is added; the permanent input owner remains canonical. */
int EspNativeGameplayHubTouchUi_consumedWeaponTarget(uint8_t* outWeaponId);

/* Retained for the non-weapon Inventory previous/current/next window. */
int EspNativeGameplayHubTouchUi_consumedSelectTarget(
    uint8_t selectedRow,
    uint8_t entryCount,
    uint8_t* outTargetRow);

#ifdef __cplusplus
}
#endif

#endif
