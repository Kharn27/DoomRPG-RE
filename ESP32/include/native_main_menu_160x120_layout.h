#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_160X120_LAYOUT_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_160X120_LAYOUT_H

/* ESP32/CYD presentation contracts for the real Doom RPG main menu.
 *
 * The legacy fitted constants remain for historical/reference paths. The
 * active MENU_MAIN and Options presentations share the finger-first DASH_*
 * geometry below.
 */
#define DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_WIDTH 108
#define DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_HEIGHT 74
#define DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH 90
#define DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT 62
#define DOOMRPG_ESP32_MAIN_MENU_LOGO_Y 2

#define DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT 4
#define DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y 67
#define DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT 12
#define DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT 12

#define DOOMRPG_ESP32_MAIN_MENU_LAST_ITEM_Y \
    (DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y + \
     ((DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT - 1) * \
      DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT))

#define DOOMRPG_ESP32_MAIN_MENU_CONTENT_BOTTOM \
    (DOOMRPG_ESP32_MAIN_MENU_LAST_ITEM_Y + \
     DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT)

/* Landscape split layout, all coordinates logical RGB565 160x120.
 * Deliberately stretch the 108x74 source logo to a 102x120 tall left
 * banner, anchored at 0,0; the user prefers full height over aspect ratio.
 * Four main-menu touch plates use 54x24 logical (108x48 physical)
 * inside the same rail, without changing the Options 54x20 cards.
 * Single layout source for painting, touch gate and debug hitboxes. */
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH 102
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT 120
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X 0
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y 0

#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT 102
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_RIGHT 159
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP 4
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM 116

#define DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT 104
#define DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT 157
/* Main 54x24 is unchanged. Nested pages have their own deliberately
 * taller hitboxes, shared byte-for-byte between painters and touch. */
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_LEFT 104
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_RIGHT 157
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_WIDTH \
    (DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_RIGHT - \
     DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_LEFT + 1)
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT 24
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_FIRST_TOP 8
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_PITCH 27
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(index) \
    (DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_FIRST_TOP + \
     (index) * DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_PITCH)
#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(index) \
    (DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(index) + \
     DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT - 1)
#define DOOMRPG_ESP32_MAIN_MENU_CARD_FIRST_TOP 13
#define DOOMRPG_ESP32_MAIN_MENU_CARD_HEIGHT 20
#define DOOMRPG_ESP32_MAIN_MENU_CARD_PITCH 25
#define DOOMRPG_ESP32_MAIN_MENU_CARD_TOP(index) \
    (DOOMRPG_ESP32_MAIN_MENU_CARD_FIRST_TOP + \
     (index) * DOOMRPG_ESP32_MAIN_MENU_CARD_PITCH)
#define DOOMRPG_ESP32_MAIN_MENU_CARD_BOTTOM(index) \
    (DOOMRPG_ESP32_MAIN_MENU_CARD_TOP(index) + \
     DOOMRPG_ESP32_MAIN_MENU_CARD_HEIGHT - 1)
#define DOOMRPG_ESP32_MAIN_MENU_DASH_FOOTER_Y 119

/* Native Options root is spacious (3 cards), VIDEO is a separate middle
 * layer (4 cards). Exactly the same rectangles are used by paint and touch.
 * The logo remains x=0..101 and all cards x=104..157. */
#define DOOMRPG_ESP32_OPTIONS_CARD_COUNT 3
#define DOOMRPG_ESP32_VIDEO_CARD_COUNT 4
/* OPTIONS 3x28: y12..39 / 46..73 / 80..107, 6px gaps.
 * VIDEO 4x24: y8..31 / 35..58 / 62..85 / 89..112, 3px gaps.
 * Both keep the logo and the single-column 54px touch rail. */
/* OPTIONS/ORIENTATION 3x32: 8..39,44..75,80..111 (4px gaps).
 * VIDEO/THEME 4x26: 6..31,34..59,62..87,90..115 (2px gaps).
 * Shared paint/touch rects; main cards unchanged. */
#define DOOMRPG_ESP32_OPTIONS_CARD_FIRST_TOP 8
#define DOOMRPG_ESP32_OPTIONS_CARD_HEIGHT 32
#define DOOMRPG_ESP32_OPTIONS_CARD_PITCH 36
#define DOOMRPG_ESP32_VIDEO_CARD_FIRST_TOP 6
#define DOOMRPG_ESP32_VIDEO_CARD_HEIGHT 26
#define DOOMRPG_ESP32_VIDEO_CARD_PITCH 28
#define DOOMRPG_ESP32_OPTIONS_CARD_TOP(index) \
    (DOOMRPG_ESP32_OPTIONS_CARD_FIRST_TOP + \
     (index) * DOOMRPG_ESP32_OPTIONS_CARD_PITCH)
#define DOOMRPG_ESP32_VIDEO_CARD_TOP(index) \
    (DOOMRPG_ESP32_VIDEO_CARD_FIRST_TOP + \
     (index) * DOOMRPG_ESP32_VIDEO_CARD_PITCH)

static inline void DoomRPG_esp32OptionsCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_OPTIONS_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_OPTIONS_CARD_TOP(item) +
                                 DOOMRPG_ESP32_OPTIONS_CARD_HEIGHT - 1;
}
static inline void DoomRPG_esp32VideoCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_VIDEO_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_VIDEO_CARD_TOP(item) +
                                 DOOMRPG_ESP32_VIDEO_CARD_HEIGHT - 1;
}

/* Brightness remains five choices in one narrow rail. 21px is the
 * absolute safe gain without overlapping the 113px available band:
 * y6..26,28..48,50..70,72..92,94..114 with 1px gaps.
 * The orientation and theme pickers have only 3 choices, so they reuse
 * OPTIONS 28px tall plates; every painter and tap maps to the same rect. */
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT 5
#define DOOMRPG_ESP32_ORIENTATION_CARD_COUNT 3
#define DOOMRPG_ESP32_THEME_CARD_COUNT 4
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_FIRST_TOP 6
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_HEIGHT 21
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_PITCH 22
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_TOP(index) \
    (DOOMRPG_ESP32_BRIGHTNESS_CARD_FIRST_TOP + \
     (index) * DOOMRPG_ESP32_BRIGHTNESS_CARD_PITCH)
#define DOOMRPG_ESP32_BRIGHTNESS_CARD_BOTTOM(index) \
    (DOOMRPG_ESP32_BRIGHTNESS_CARD_TOP(index) + \
     DOOMRPG_ESP32_BRIGHTNESS_CARD_HEIGHT - 1)

static inline void DoomRPG_esp32BrightnessCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_BRIGHTNESS_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_BRIGHTNESS_CARD_BOTTOM(item);
}

/* Main painter and touch share 54x24, no logo/rail overlap.
 * Nested layouts are independently centered with no intersecting zones. */
static inline void DoomRPG_esp32IndustrialMainCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(item);
}

/* Child Options retains the original card width and two-tap contract. */
static inline void DoomRPG_esp32MainMenuCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_MAIN_MENU_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_MAIN_MENU_CARD_BOTTOM(item);
}

#endif
