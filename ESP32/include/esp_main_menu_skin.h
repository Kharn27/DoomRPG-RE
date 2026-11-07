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

/* ROM-only glyph palettes, persisted in the native 12-byte SD config.
 * No RAM framebuffer copy and no change to the industrial metal chassis. */
#define ESP_MAIN_MENU_THEME_CLASSIC 0U
#define ESP_MAIN_MENU_THEME_INFERNO 1U
#define ESP_MAIN_MENU_THEME_PHOSPHORE 2U
uint8_t EspMainMenuSkin_getTheme(void);
int EspMainMenuSkin_setTheme(uint8_t theme);
uint16_t EspMainMenuSkin_letterTone(int row, int rows);

/* Theme-matched line/seam color for the industrial plates and Options
 * ornaments. CLASSIC is pixel-identical to the historical copper palette;
 * INFERNO has warm ember seams and PHOSPHORE mint/green seams. Returns 0
 * on an invalid plate state, without mutating theme or framebuffer. */
uint16_t EspMainMenuSkin_accentTone(EspMainMenuSkinState state);

/* Six-row loading progress ink, independently colored by the selected theme.
 * CLASSIC stays byte-identical to the original amber gauge. Bounds checked,
 * ROM-only (no cache or extra framebuffer). */
uint16_t EspMainMenuSkin_progressTone(int row, int rows);

/* Shared 160x120 native-menu accents for HUB/SYSTEM/fullscreen reports.
 * CLASSIC returns the historical amber, muted amber and ivory ink;
 * INFERNO hot red, PHOSPHORE mint. Pure, allocation-free theme reads.
 * Gameplay health/armor and danger semantics are not palette-swapped. */
uint16_t EspMainMenuSkin_uiAccent(void);
uint16_t EspMainMenuSkin_uiAccentDim(void);
uint16_t EspMainMenuSkin_uiInk(void);

/* Paint all 9 original glyph rows (including A/E/H crossbars). Long
 * words sample three columns, never omit rows. No shift, shadow or IO. */
int EspMainMenuSkin_paintText(uint16_t* framebuffer, int stride,
                              int width, int height,
                              int left, int top, int right, int bottom,
                              const char* label, int selected, int enabled);

/* Non-clickable upper-bulkhead stencil inspired by YOU DIED:
 * small steel rules, side hazard hatches, thin lower signal; NO plate rim.
 * Its central x=28..131/y=10..26 area is text-safe for the 16-character
 * MISSION COMPLETE 5x7 title plus its second-line label. Paints only
 * x=9..150 / y=7..27, allocation-free; invalid framebuffer is untouched. */
int EspMainMenuSkin_paintModalTitleStencil(uint16_t* framebuffer,
                                           int stridePixels,
                                           int logicalWidth,
                                           int logicalHeight);

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
