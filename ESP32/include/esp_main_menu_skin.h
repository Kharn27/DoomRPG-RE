#ifndef DOOMRPG_ESP32_MAIN_MENU_SKIN_H
#define DOOMRPG_ESP32_MAIN_MENU_SKIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only MENU_MAIN's four fixed 54x24 logical plates. No runtime-owned state,
 * no texture/cache, no SDL dependency, no allocation or physical rotation.
 * CONFIRM is available for host proof/future usage but intentionally not
 * presented during the immediate two-tap dispatch (no extra frame latency). */
typedef enum {
    ESP_MAIN_MENU_SKIN_IDLE = 0,
    ESP_MAIN_MENU_SKIN_ARMED = 1,
    ESP_MAIN_MENU_SKIN_CONFIRM = 2
} EspMainMenuSkinState;

/* Shared procedural gunmetal chassis. No text, heap or IO. Strict 160x120
 * framebuffer and bounded rectangle; invalid inputs leave pixels untouched.
 * The same IDLE geometry/palette is used by the loading presentation. */
int EspMainMenuSkin_paintPlate(uint16_t* framebuffer,
                               int stridePixels, int logicalWidth,
                               int logicalHeight, int left, int top,
                               int right, int bottom,
                               EspMainMenuSkinState state);

/* Returns 0 without writes on invalid arguments. On success, changes pixels
 * ONLY inside the selected card rectangle; caller owns the same framebuffer. */
int EspMainMenuSkin_paint(uint16_t* framebuffer,
                          int stridePixels,
                          int logicalWidth,
                          int logicalHeight,
                          int item,
                          EspMainMenuSkinState state);

#ifdef __cplusplus
}
#endif
#endif
