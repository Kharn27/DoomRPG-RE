#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_THEME_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_THEME_H

#include <stdint.h>

/* Dark industrial HUB palette. Keep bright colors semantic: amber is focus,
 * green is success and red is failure/danger. */
#define ESP_HUB_COLOR_BLACK        0x0000U
#define ESP_HUB_COLOR_BG           0x0841U
#define ESP_HUB_COLOR_PANEL        0x18c3U
#define ESP_HUB_COLOR_PANEL_ALT    0x2945U
#define ESP_HUB_COLOR_STEEL_DARK   0x3186U
#define ESP_HUB_COLOR_STEEL        0x6b4dU
#define ESP_HUB_COLOR_IVORY        0xef5cU
#define ESP_HUB_COLOR_AMBER_DIM    0x7a80U
#define ESP_HUB_COLOR_AMBER        0xfd20U
#define ESP_HUB_COLOR_RED          0xc986U
#define ESP_HUB_COLOR_GREEN        0x4d8bU
#define ESP_HUB_COLOR_BLUE         0x4d1bU
#define ESP_HUB_COLOR_YELLOW       0xffe0U

/* palettes.bin stores the legacy 5-bit red/blue channels opposite to the
 * framebuffer's RGB565 order. All raw palette consumers must cross this one
 * explicit conversion boundary. */
static inline uint16_t EspNativeGameplayHubTheme_sourceToFramebuffer565(
    uint16_t color) {
    return (uint16_t)(((color & 0x001fU) << 11) |
                      (color & 0x07e0U) |
                      ((color & 0xf800U) >> 11));
}

#endif
