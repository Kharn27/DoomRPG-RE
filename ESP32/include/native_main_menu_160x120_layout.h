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
 * Source logo 108x74 retains aspect ratio: 100x69 floating on black.
 * Finger targets are 54x20 logical / 108x40 physical, four tall cards.
 * One source of truth for painting, touch gate and debug hitboxes. */
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH 100
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT 69
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X 0
#define DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y 25

#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT 102
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_RIGHT 159
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP 4
#define DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM 116

#define DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT 104
#define DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT 157
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

/* Keep the exact 2-tap logical selection/confirmation contract. */
static inline void DoomRPG_esp32MainMenuCardRect(
    int item, int* left, int* top, int* right, int* bottom) {
    if (left != 0) *left = DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT;
    if (right != 0) *right = DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT;
    if (top != 0) *top = DOOMRPG_ESP32_MAIN_MENU_CARD_TOP(item);
    if (bottom != 0) *bottom = DOOMRPG_ESP32_MAIN_MENU_CARD_BOTTOM(item);
}

#endif
