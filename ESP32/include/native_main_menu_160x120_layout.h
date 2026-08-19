#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_160X120_LAYOUT_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_160X120_LAYOUT_H

/* ESP32/CYD-specific presentation contract for the real Doom RPG main menu.
 *
 * The original 108x74 logo plus four 12-pixel-high menu rows cannot fit in a
 * 160x120 logical framebuffer without overlap/overflow. Keep the original font,
 * hand cursor and menu model, but scale only the logo and move the rows upward.
 *
 * These constants are intentionally shared so the next touch-input increment can
 * derive hit zones from exactly the same final item geometry.
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

#endif
