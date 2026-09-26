#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Visible touch chrome for the native gameplay HUB. This layer owns no gameplay
 * state and no framebuffer snapshot. It paints logical y=20..119 while the HUB
 * is active; the gameplay HUD underneath is reconstructed on close. It maps
 * the INV / WPN / STAT / SYS tabs plus page-local touch targets to semantic
 * actions. */
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

/* Resolve the exact tab touched by an already-consumed TURN input. This keeps
 * all four tabs directly addressable even when the destination is two pages
 * away in the cyclic keyboard navigation order. */
int EspNativeGameplayHubTouchUi_consumedPageTarget(uint8_t* outPage);

/* Resolve the exact row touched in the four-line Inventory window. Physical
 * up/down actions have no row coordinate and keep cyclic navigation. */
int EspNativeGameplayHubTouchUi_consumedInventoryTarget(
    uint8_t selectedRow,
    uint8_t entryCount,
    uint8_t* outTargetRow);

#ifdef __cplusplus
}
#endif

#endif
