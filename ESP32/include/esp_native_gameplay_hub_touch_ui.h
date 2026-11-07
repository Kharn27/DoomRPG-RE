#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_TOUCH_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Visible touch chrome for the native gameplay HUB. This layer owns no gameplay
 * state and no framebuffer snapshot. It paints logical y=20..119 while the HUB
 * is active; the gameplay HUD underneath is reconstructed on close. It maps
 * the INV / STAT / SYS tabs plus page-local targets. The weapon grid
 * is a standalone panel with BACK, opened from the lower ammo/compass HUD. */
int EspNativeGameplayHubTouchUi_paint(uint16_t* framebuffer,
                                      uint8_t page,
                                      uint8_t selectedRow);

/* Three directly touchable upper-band tabs, to the RIGHT of the preserved
 * 32x20 hand/menu icon. All three remain inside y=2..17, leaving viewport
 * y=20..119 available for content. Geometry is shared by paint and touch. */
#define ESP_NATIVE_HUB_TOP_TAB_TOP 2
#define ESP_NATIVE_HUB_TOP_TAB_BOTTOM 17
#define ESP_NATIVE_HUB_TOP_TAB_COUNT 3
static inline int EspNativeGameplayHubTouchUi_headerTabBounds(
    int tab, int* left, int* right) {
    static const uint8_t starts[ESP_NATIVE_HUB_TOP_TAB_COUNT] = {34U, 76U, 117U};
    static const uint8_t ends[ESP_NATIVE_HUB_TOP_TAB_COUNT] = {74U, 115U, 157U};
    if (tab < 0 || tab >= ESP_NATIVE_HUB_TOP_TAB_COUNT) return 0;
    if (left != 0) *left = starts[tab];
    if (right != 0) *right = ends[tab];
    return 1;
}

/* Draws header tabs ONLY in x34..157/y2..17, never the hand/menu underlay. */
int EspNativeGameplayHubTouchUi_paintHeaderTabs(
    uint16_t* framebuffer, uint8_t page);

/* Shared allocation-free 5x7 face for HUB and full-screen mission reports. */
void EspNativeGameplayHubTouchUi_drawCrispText(uint16_t* framebuffer,
                                             const char* text,
                                             int centerX,
                                             int top,
                                             uint16_t color);

/* Shared allocation-free 3x5 face for compact numeric/result annotations. */
void EspNativeGameplayHubTouchUi_drawMiniText(uint16_t* framebuffer,
                                              const char* text,
                                              int centerX,
                                              int top,
                                              uint16_t color);

struct EspNativeGameplayTouchHit_s;
int EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHit);

/* Read the exact weapon cell targeted by an already-consumed HUB SELECT input.
 * No target queue/owner is added; the permanent input owner remains canonical. */
int EspNativeGameplayHubTouchUi_consumedWeaponTarget(uint8_t* outWeaponId);

/* Resolve the exact tab touched by an already-consumed TURN input. This keeps
 * all three visible HUB tabs directly addressable without passing through
 * the separate Weapons panel. */
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
