#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_content.h"
#include "esp_native_gameplay_hub_nonweapon.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_hub_weapon_grid.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_config.h"

#define HUB_UI_TOP 20
#define HUB_UI_BOTTOM 119
#define HUB_UI_LEFT 0
#define HUB_UI_RIGHT 159

#define HUB_UI_BLACK ESP_HUB_COLOR_BG
#define HUB_UI_PANEL ESP_HUB_COLOR_PANEL
#define HUB_UI_PANEL_ALT ESP_HUB_COLOR_PANEL_ALT
#define HUB_UI_DIM ESP_HUB_COLOR_STEEL_DARK
#define HUB_UI_STEEL ESP_HUB_COLOR_STEEL
#define HUB_UI_TEXT ESP_HUB_COLOR_IVORY
#define HUB_UI_FOCUS ESP_HUB_COLOR_AMBER
#define HUB_UI_GOOD ESP_HUB_COLOR_GREEN
#define HUB_UI_DANGER ESP_HUB_COLOR_RED
#define HUB_UI_ARMOR ESP_HUB_COLOR_BLUE

#define HUB_UI_FONT_NAME "a.bmp"
#define HUB_UI_FONT_WIDTH 9U
#define HUB_UI_FONT_HEIGHT 12U
#define HUB_UI_FONT_ADVANCE 7
#define HUB_UI_FONT_SOURCE_WIDTH 144U
#define HUB_UI_FONT_SOURCE_HEIGHT 72U
#define HUB_UI_FONT_TRANSPARENT 1U

#define HUB_UI_INV_LEFT 2
#define HUB_UI_INV_TOP 21
#define HUB_UI_INV_RIGHT 39
#define HUB_UI_INV_BOTTOM 33
#define HUB_UI_WPN_LEFT 41
#define HUB_UI_WPN_TOP 21
#define HUB_UI_WPN_RIGHT 78
#define HUB_UI_WPN_BOTTOM 33
#define HUB_UI_STATUS_LEFT 80
#define HUB_UI_STATUS_TOP 21
#define HUB_UI_STATUS_RIGHT 117
#define HUB_UI_STATUS_BOTTOM 33
#define HUB_UI_SYSTEM_LEFT 119
#define HUB_UI_SYSTEM_TOP 21
#define HUB_UI_SYSTEM_RIGHT 157
#define HUB_UI_SYSTEM_BOTTOM 33

#define HUB_UI_ROW_LEFT 2
#define HUB_UI_ROW_RIGHT 157
#define HUB_UI_ROW_TOUCH_RIGHT 157
#define HUB_UI_ROW0_TOP 36
#define HUB_UI_ROW0_BOTTOM 61
#define HUB_UI_ROW1_TOP 64
#define HUB_UI_ROW1_BOTTOM 89
#define HUB_UI_ROW2_TOP 92
#define HUB_UI_ROW2_BOTTOM 117

static int inside(int x, int y, int left, int top, int right, int bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < HUB_UI_LEFT || x > HUB_UI_RIGHT ||
        y < HUB_UI_TOP || y > HUB_UI_BOTTOM) return;
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void fillRect(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    if (framebuffer == NULL || left > right || top > bottom) return;
    if (left < HUB_UI_LEFT) left = HUB_UI_LEFT;
    if (right > HUB_UI_RIGHT) right = HUB_UI_RIGHT;
    if (top < HUB_UI_TOP) top = HUB_UI_TOP;
    if (bottom > HUB_UI_BOTTOM) bottom = HUB_UI_BOTTOM;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) {
            framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
        }
    }
}

static void drawRect(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        putPixel(framebuffer, x, top, color);
        putPixel(framebuffer, x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        putPixel(framebuffer, left, y, color);
        putPixel(framebuffer, right, y, color);
    }
}

static int miniRows(char c, uint8_t rows[5]) {
    static const uint8_t digits[10][5] = {
        {7U, 5U, 5U, 5U, 7U}, {2U, 6U, 2U, 2U, 7U},
        {7U, 1U, 7U, 4U, 7U}, {7U, 1U, 7U, 1U, 7U},
        {5U, 5U, 7U, 1U, 1U}, {7U, 4U, 7U, 1U, 7U},
        {7U, 4U, 7U, 5U, 7U}, {7U, 1U, 2U, 2U, 2U},
        {7U, 5U, 7U, 5U, 7U}, {7U, 5U, 7U, 1U, 7U}
    };
    static const uint8_t letters[26][5] = {
        {2U, 5U, 7U, 5U, 5U}, {6U, 5U, 6U, 5U, 6U},
        {7U, 4U, 4U, 4U, 7U}, {6U, 5U, 5U, 5U, 6U},
        {7U, 4U, 6U, 4U, 7U}, {7U, 4U, 6U, 4U, 4U},
        {7U, 4U, 5U, 5U, 7U}, {5U, 5U, 7U, 5U, 5U},
        {7U, 2U, 2U, 2U, 7U}, {1U, 1U, 1U, 5U, 7U},
        {5U, 5U, 6U, 5U, 5U}, {4U, 4U, 4U, 4U, 7U},
        {5U, 7U, 7U, 5U, 5U}, {5U, 7U, 7U, 7U, 5U},
        {7U, 5U, 5U, 5U, 7U}, {6U, 5U, 6U, 4U, 4U},
        {7U, 5U, 5U, 7U, 1U}, {6U, 5U, 6U, 5U, 5U},
        {7U, 4U, 7U, 1U, 7U}, {7U, 2U, 2U, 2U, 2U},
        {5U, 5U, 5U, 5U, 7U}, {5U, 5U, 5U, 5U, 2U},
        {5U, 5U, 7U, 7U, 5U}, {5U, 5U, 2U, 5U, 5U},
        {5U, 5U, 2U, 2U, 2U}, {7U, 1U, 2U, 4U, 7U}
    };
    static const uint8_t slash[5] = {1U, 1U, 2U, 4U, 4U};
    const uint8_t* source;

    if (c >= '0' && c <= '9') source = digits[c - '0'];
    else if (c >= 'A' && c <= 'Z') source = letters[c - 'A'];
    else if (c == '/') source = slash;
    else return 0;
    memcpy(rows, source, 5U);
    return 1;
}

static int miniTextWidth(const char* text, int scale) {
    int count = 0;
    if (text == NULL || scale <= 0) return 0;
    while (*text++ != '\0') ++count;
    if (count == 0) return 0;
    return count * (3 * scale) + (count - 1) * scale;
}

static void drawMiniTextAt(uint16_t* framebuffer,
                           const char* text,
                           int x,
                           int top,
                           int scale,
                           uint16_t color) {
    if (framebuffer == NULL || text == NULL || scale <= 0) return;
    while (*text != '\0') {
        uint8_t rows[5];
        int row;
        if (miniRows(*text, rows)) {
            for (row = 0; row < 5; ++row) {
                int column;
                for (column = 0; column < 3; ++column) {
                    if ((rows[row] & (uint8_t)(1U << (2 - column))) != 0U) {
                        fillRect(framebuffer,
                                 x + column * scale,
                                 top + row * scale,
                                 x + column * scale + scale - 1,
                                 top + row * scale + scale - 1,
                                 color);
                    }
                }
            }
        }
        x += 4 * scale;
        ++text;
    }
}

static void drawCrispText(uint16_t* framebuffer,
                          const char* text,
                          int centerX,
                          int top,
                          uint16_t color);

static void drawTab(uint16_t* framebuffer,
                    int left,
                    int top,
                    int right,
                    int bottom,
                    const char* label,
                    int selected) {
    fillRect(framebuffer, left, top, right, bottom,
             selected ? HUB_UI_PANEL_ALT : HUB_UI_PANEL);
    drawRect(framebuffer, left, top, right, bottom,
             selected ? HUB_UI_FOCUS : HUB_UI_STEEL);
    drawCrispText(framebuffer, label, (left + right) / 2,
                  top + 3, selected ? HUB_UI_FOCUS : HUB_UI_TEXT);
}

static void drawInventoryCards(uint16_t* framebuffer) {
    static const int tops[3] = {
        HUB_UI_ROW0_TOP, HUB_UI_ROW1_TOP, HUB_UI_ROW2_TOP
    };
    static const int bottoms[3] = {
        HUB_UI_ROW0_BOTTOM, HUB_UI_ROW1_BOTTOM, HUB_UI_ROW2_BOTTOM
    };
    int row;
    for (row = 0; row < 3; ++row) {
        fillRect(framebuffer, HUB_UI_ROW_LEFT, tops[row],
                 HUB_UI_ROW_RIGHT, bottoms[row],
                 row == 1 ? HUB_UI_PANEL_ALT : HUB_UI_BLACK);
        drawRect(framebuffer, HUB_UI_ROW_LEFT, tops[row],
                 HUB_UI_ROW_RIGHT, bottoms[row],
                 row == 1 ? HUB_UI_FOCUS : HUB_UI_DIM);
        if (row == 1) {
            int y;
            for (y = tops[row] + 2; y <= bottoms[row] - 2; ++y) {
                putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, y, HUB_UI_FOCUS);
                putPixel(framebuffer, HUB_UI_ROW_LEFT + 2, y, HUB_UI_FOCUS);
            }
        }
    }
}

static void drawMiniTextRight(uint16_t* framebuffer,
                              const char* text,
                              int right,
                              int top,
                              uint16_t color) {
    const int width = miniTextWidth(text, 1);
    if (width <= 0) return;
    drawMiniTextAt(framebuffer, text, right - width + 1, top, 1, color);
}

#define HUB_UI_GLYPH7(a, b, c, d, e, f, g) \
    (((uint64_t)(a) << 30) | ((uint64_t)(b) << 25) | \
     ((uint64_t)(c) << 20) | ((uint64_t)(d) << 15) | \
     ((uint64_t)(e) << 10) | ((uint64_t)(f) << 5) | (uint64_t)(g))

static uint64_t tabLetterBits(char c) {
    switch (c) {
    case 'A': return HUB_UI_GLYPH7(14, 17, 17, 31, 17, 17, 17);
    case 'I': return HUB_UI_GLYPH7(31, 4, 4, 4, 4, 4, 31);
    case 'N': return HUB_UI_GLYPH7(17, 25, 21, 19, 17, 17, 17);
    case 'P': return HUB_UI_GLYPH7(30, 17, 17, 30, 16, 16, 16);
    case 'S': return HUB_UI_GLYPH7(15, 16, 16, 14, 1, 1, 30);
    case 'T': return HUB_UI_GLYPH7(31, 4, 4, 4, 4, 4, 4);
    case 'V': return HUB_UI_GLYPH7(17, 17, 17, 17, 17, 10, 4);
    case 'W': return HUB_UI_GLYPH7(17, 17, 17, 21, 21, 21, 10);
    case 'X': return HUB_UI_GLYPH7(17, 17, 10, 4, 10, 17, 17);
    case 'Y': return HUB_UI_GLYPH7(17, 17, 10, 4, 4, 4, 4);
    default: return 0U;
    }
}

#undef HUB_UI_GLYPH7

static int crispRows(char c, uint8_t rows[7]) {
    static const uint8_t digits[10][7] = {
        {14U, 17U, 19U, 21U, 25U, 17U, 14U},
        {4U, 12U, 4U, 4U, 4U, 4U, 14U},
        {14U, 17U, 1U, 2U, 4U, 8U, 31U},
        {30U, 1U, 1U, 14U, 1U, 1U, 30U},
        {2U, 6U, 10U, 18U, 31U, 2U, 2U},
        {31U, 16U, 16U, 30U, 1U, 1U, 30U},
        {14U, 16U, 16U, 30U, 17U, 17U, 14U},
        {31U, 1U, 2U, 4U, 8U, 8U, 8U},
        {14U, 17U, 17U, 14U, 17U, 17U, 14U},
        {14U, 17U, 17U, 15U, 1U, 1U, 14U}
    };
    static const uint8_t slash[7] = {1U, 1U, 2U, 4U, 8U, 16U, 16U};
    const uint8_t* source = NULL;
    uint64_t bits;
    int row;

    if (c >= '0' && c <= '9') source = digits[c - '0'];
    else if (c == '/') source = slash;
    if (source != NULL) {
        memcpy(rows, source, 7U);
        return 1;
    }

    bits = tabLetterBits(c);
    if (bits == 0U) return 0;
    for (row = 0; row < 7; ++row) {
        rows[row] = (uint8_t)((bits >> ((6 - row) * 5)) & 31U);
    }
    return 1;
}

static void drawCrispText(uint16_t* framebuffer,
                          const char* text,
                          int centerX,
                          int top,
                          uint16_t color) {
    const int length = text != NULL ? (int)strlen(text) : 0;
    int x;

    if (framebuffer == NULL || length <= 0) return;
    x = centerX - ((length * 6 - 1) / 2);
    while (*text != '\0') {
        uint8_t rows[7];
        int row;
        if (!crispRows(*text, rows)) return;
        for (row = 0; row < 7; ++row) {
            int column;
            for (column = 0; column < 5; ++column) {
                if ((rows[row] & (uint8_t)(1U << (4 - column))) != 0U) {
                    putPixel(framebuffer, x + column, top + row, color);
                }
            }
        }
        x += 6;
        ++text;
    }
}

static void drawStatusMetricCard(uint16_t* framebuffer,
                                 int left,
                                 int right,
                                 const char* label,
                                 const char* value,
                                 uint8_t current,
                                 uint8_t maximum,
                                 uint16_t gaugeColor) {
    const int gaugeTop = 37;
    const int gaugeBottom = 50;
    const int gaugeHeight = gaugeBottom - gaugeTop + 1;
    int filled = 0;

    fillRect(framebuffer, left, 35, right, 52, HUB_UI_PANEL);
    drawRect(framebuffer, left, 35, right, 52, HUB_UI_DIM);
    fillRect(framebuffer, left + 2, gaugeTop,
             left + 3, gaugeBottom, HUB_UI_DIM);
    if (maximum != 0U) {
        uint8_t bounded = current > maximum ? maximum : current;
        filled = ((int)bounded * gaugeHeight + (int)maximum - 1) /
                 (int)maximum;
    }
    if (filled > 0) {
        fillRect(framebuffer, left + 2, gaugeBottom - filled + 1,
                 left + 3, gaugeBottom, gaugeColor);
    }
    drawMiniTextAt(framebuffer, label, left + 7, 37, 1, HUB_UI_STEEL);
    drawCrispText(framebuffer, value, (left + right) / 2, 44, HUB_UI_TEXT);
}

static void drawStatusProgress(uint16_t* framebuffer,
                               int left,
                               int right,
                               int top,
                               uint32_t value,
                               uint32_t maximum) {
    uint32_t filled = 0U;
    const int width = right - left + 1;
    fillRect(framebuffer, left, top, right, top + 1, HUB_UI_DIM);
    if (maximum != 0U) {
        uint64_t bounded = value > maximum ? maximum : value;
        filled = (uint32_t)((bounded * (uint64_t)width) / maximum);
    }
    if (filled != 0U) {
        fillRect(framebuffer, left, top,
                 left + (int)filled - 1, top + 1, HUB_UI_FOCUS);
    }
}

static void drawStatusAttribute(uint16_t* framebuffer,
                                const char* label,
                                uint8_t value,
                                int left,
                                int right,
                                int top) {
    char number[4];
    snprintf(number, sizeof(number), "%u", (unsigned int)value);
    drawMiniTextAt(framebuffer, label, left, top, 1, HUB_UI_STEEL);
    drawMiniTextRight(framebuffer, number, right, top, HUB_UI_TEXT);
}

static void drawStatusKeyCard(uint16_t* framebuffer,
                              int left,
                              int top,
                              uint16_t color) {
    fillRect(framebuffer, left, top, left + 9, top + 5, HUB_UI_BLACK);
    drawRect(framebuffer, left, top, left + 9, top + 5, color);
    fillRect(framebuffer, left + 2, top + 2, left + 3, top + 3, color);
    fillRect(framebuffer, left + 5, top + 2, left + 7, top + 2, color);
    putPixel(framebuffer, left + 7, top + 3, color);
    putPixel(framebuffer, left + 8, top + 4, HUB_UI_BLACK);
}

static void drawStatusKeys(uint16_t* framebuffer, uint32_t keys) {
    static const uint16_t colors[4] = {
        ESP_HUB_COLOR_GREEN,
        ESP_HUB_COLOR_YELLOW,
        ESP_HUB_COLOR_BLUE,
        ESP_HUB_COLOR_RED
    };
    int cursor = 105;
    int key;
    uint32_t visibleKeys = keys & 0x0fU;

    drawMiniTextAt(framebuffer, "KEY", 85, 109, 1, HUB_UI_STEEL);
    if (visibleKeys == 0U) {
        drawMiniTextRight(framebuffer, "NONE", 152, 109, HUB_UI_DIM);
        return;
    }
    for (key = 0; key < 4; ++key) {
        if ((visibleKeys & (1UL << key)) == 0U) continue;
        drawStatusKeyCard(framebuffer, cursor, 108, colors[key]);
        cursor += 12;
    }
}

static int paintStatusDashboard(
    uint16_t* framebuffer,
    const EspNativeGameplayPlayerState* player) {
    char value[24];
    uint8_t health;
    uint8_t maxHealth;
    uint8_t armor;
    uint8_t maxArmor;
    uint8_t defense;
    uint8_t strength;
    uint8_t agility;
    uint8_t accuracy;
    uint16_t healthAccent;

    if (framebuffer == NULL || player == NULL || player->active != 1U) return 0;
    health = (uint8_t)(player->param1 & 0xffU);
    maxHealth = (uint8_t)((player->param1 >> 8) & 0xffU);
    armor = (uint8_t)((player->param1 >> 16) & 0xffU);
    maxArmor = (uint8_t)((player->param1 >> 24) & 0xffU);
    defense = (uint8_t)(player->param2 & 0xffU);
    strength = (uint8_t)((player->param2 >> 8) & 0xffU);
    agility = (uint8_t)((player->param2 >> 16) & 0xffU);
    accuracy = (uint8_t)((player->param2 >> 24) & 0xffU);
    healthAccent = maxHealth != 0U &&
                           (uint16_t)health * 4U <= (uint16_t)maxHealth
                       ? HUB_UI_DANGER
                       : HUB_UI_GOOD;

    snprintf(value, sizeof(value), "%u/%u",
             (unsigned int)health, (unsigned int)maxHealth);
    drawStatusMetricCard(framebuffer, 2, 78, "HP", value,
                         health, maxHealth, healthAccent);
    snprintf(value, sizeof(value), "%u/%u",
             (unsigned int)armor, (unsigned int)maxArmor);
    drawStatusMetricCard(framebuffer, 81, 157, "ARMOR", value,
                         armor, maxArmor, HUB_UI_ARMOR);

    fillRect(framebuffer, 2, 57, 157, 74, HUB_UI_PANEL_ALT);
    drawRect(framebuffer, 2, 57, 157, 74, HUB_UI_DIM);
    drawMiniTextAt(framebuffer, "LEVEL", 7, 61, 1, HUB_UI_STEEL);
    snprintf(value, sizeof(value), "%u", (unsigned int)player->level);
    drawMiniTextRight(framebuffer, value, 48, 61, HUB_UI_TEXT);
    drawMiniTextAt(framebuffer, "XP", 55, 61, 1, HUB_UI_STEEL);
    snprintf(value, sizeof(value), "%lu/%lu",
             (unsigned long)player->currentXP,
             (unsigned long)player->nextLevelXP);
    drawMiniTextRight(framebuffer, value, 153, 61, HUB_UI_TEXT);
    drawStatusProgress(framebuffer, 55, 153, 71,
                       player->currentXP, player->nextLevelXP);

    fillRect(framebuffer, 2, 78, 157, 101, HUB_UI_BLACK);
    drawRect(framebuffer, 2, 78, 157, 101, HUB_UI_DIM);
    fillRect(framebuffer, 79, 80, 80, 99, HUB_UI_DIM);
    fillRect(framebuffer, 4, 89, 155, 89, HUB_UI_DIM);
    drawStatusAttribute(framebuffer, "DEF", defense, 7, 72, 81);
    drawStatusAttribute(framebuffer, "STR", strength, 86, 151, 81);
    drawStatusAttribute(framebuffer, "AGI", agility, 7, 72, 93);
    drawStatusAttribute(framebuffer, "ACC", accuracy, 86, 151, 93);

    fillRect(framebuffer, 2, 105, 157, 117, HUB_UI_PANEL);
    drawRect(framebuffer, 2, 105, 157, 117, HUB_UI_DIM);
    drawMiniTextAt(framebuffer, "CRED", 7, 109, 1, HUB_UI_STEEL);
    snprintf(value, sizeof(value), "%lu", (unsigned long)player->credits);
    drawMiniTextRight(framebuffer, value, 74, 109, HUB_UI_TEXT);
    drawStatusKeys(framebuffer, player->keys);
    printf("[HUBSTAT] FRAME layout=compact-read-only font=3x5 vitals=two-card-gauges hpGauge=green-or-red armorGauge=blue xp=progress attributes=2x2 keys=owned-color-cards keyMask=%08lx touchTargets=tab-only mutation=no turn=no\n",
           (unsigned long)player->keys);
    return 1;
}

static int drawDoomText(const EspNativeIndexedBmp* font,
                        uint16_t* framebuffer,
                        const char* text,
                        int x,
                        int y,
                        EspNativeIndexedBmpStats* stats) {
    const unsigned char* p = (const unsigned char*)text;
    if (font == NULL || framebuffer == NULL || text == NULL || stats == NULL ||
        y < HUB_UI_TOP || y + (int)HUB_UI_FONT_HEIGHT > HUB_UI_BOTTOM + 1) {
        return 0;
    }
    while (*p != '\0') {
        uint8_t c = *p++;
        if (c == ' ') {
            x += HUB_UI_FONT_ADVANCE;
            continue;
        }
        if (c < 33U || c > 127U) return 0;
        {
            uint8_t glyph = (uint8_t)(c - 33U);
            uint16_t sourceX = (uint16_t)(HUB_UI_FONT_WIDTH * (glyph & 0x0fU));
            uint16_t sourceY = (uint16_t)(HUB_UI_FONT_HEIGHT * (glyph >> 4));
            if (EspNativeIndexedBmp_blit(font,
                                         framebuffer,
                                         DOOMRPG_LOGICAL_WIDTH,
                                         DOOMRPG_LOGICAL_HEIGHT,
                                         sourceX,
                                         sourceY,
                                         HUB_UI_FONT_WIDTH,
                                         HUB_UI_FONT_HEIGHT,
                                         (int16_t)x,
                                         (int16_t)y,
                                         HUB_UI_FONT_TRANSPARENT,
                                         stats) != ESP_NATIVE_INDEXED_BMP_OK) {
                return 0;
            }
        }
        x += HUB_UI_FONT_ADVANCE;
        if (x >= DOOMRPG_LOGICAL_WIDTH) break;
    }
    return 1;
}

static int formatEntryLine(char* line,
                           uint32_t capacity,
                           char marker,
                           const EspNativeGameplayHubInventoryEntry* entry) {
    int written;
    if (line == NULL || capacity < 2U || entry == NULL ||
        entry->name[0] == '\0' || entry->value[0] == '\0') return 0;
    written = snprintf(line, capacity, "%c%s %s", marker, entry->name, entry->value);
    return written >= 0 && (uint32_t)written < capacity;
}

static int paintInventoryLabels(uint16_t* framebuffer, uint8_t selectedEntry) {
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    EspNativeGameplayPlayerState player;
    EspNativeGameplayHubInventoryEntry previous;
    EspNativeGameplayHubInventoryEntry current;
    EspNativeGameplayHubInventoryEntry next;
    EspNativeIndexedBmp font;
    EspNativeIndexedBmpStats stats;
    char line[40];
    uint8_t count;
    uint8_t previousIndex;
    uint8_t nextIndex;
    int ok;

    memset(&player, 0, sizeof(player));
    memset(&previous, 0, sizeof(previous));
    memset(&current, 0, sizeof(current));
    memset(&next, 0, sizeof(next));
    memset(&font, 0, sizeof(font));
    memset(&stats, 0, sizeof(stats));
    memset(line, 0, sizeof(line));

    if (framebuffer == NULL || view == NULL || view->active == 0U ||
        !EspAssetPack_isOpen() ||
        !EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
        return 0;
    }
    count = EspNativeGameplayHubNonWeapon_entryCount(&player);
    if (count == 0U || selectedEntry >= count) return 0;

    if (view->opens == 1U && view->paints == 0U &&
        !EspNativeGameplayHubContent_probeInventoryList()) return 0;

    previousIndex = (uint8_t)((selectedEntry + count - 1U) % count);
    nextIndex = (uint8_t)((selectedEntry + 1U) % count);
    if (!EspNativeGameplayHubNonWeapon_entryAt(&player, previousIndex, &previous) ||
        !EspNativeGameplayHubNonWeapon_entryAt(&player, selectedEntry, &current) ||
        !EspNativeGameplayHubNonWeapon_entryAt(&player, nextIndex, &next) ||
        EspNativeIndexedBmp_open(HUB_UI_FONT_NAME, &font, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        font.width != HUB_UI_FONT_SOURCE_WIDTH ||
        font.height != HUB_UI_FONT_SOURCE_HEIGHT) return 0;

    fillRect(framebuffer, 5, HUB_UI_ROW0_TOP + 2, 155, HUB_UI_ROW0_BOTTOM - 1,
             HUB_UI_BLACK);
    fillRect(framebuffer, 5, HUB_UI_ROW1_TOP + 2, 155, HUB_UI_ROW1_BOTTOM - 1,
             HUB_UI_PANEL_ALT);
    fillRect(framebuffer, 5, HUB_UI_ROW2_TOP + 2, 155, HUB_UI_ROW2_BOTTOM - 1,
             HUB_UI_BLACK);

    ok = formatEntryLine(line, sizeof(line), ' ', &previous) &&
         drawDoomText(&font, framebuffer, line, 7, HUB_UI_ROW0_TOP + 7, &stats);
    ok = formatEntryLine(line, sizeof(line), '>', &current) &&
         drawDoomText(&font, framebuffer, line, 7, HUB_UI_ROW1_TOP + 7, &stats) && ok;
    ok = formatEntryLine(line, sizeof(line), ' ', &next) &&
         drawDoomText(&font, framebuffer, line, 7, HUB_UI_ROW2_TOP + 7, &stats) && ok;
    if (!ok || !EspAssetPack_isOpen()) return 0;

    printf("[HUBINV] FRAME entries=%u selected=%u prev=%u/%s/\"%s\"/\"%s\" current=%u/%s/\"%s\"/\"%s\" next=%u/%s/\"%s\"/\"%s\" weapons=dedicated-grid persistentListBytes=0 visibleEntryBytes=%u fontReads=%u fontBytes=%u packOwnership=preserved-open mutation=no turn=no\n",
           (unsigned int)count,
           (unsigned int)selectedEntry,
           (unsigned int)previousIndex,
           EspNativeGameplayHubContent_inventoryKindName(previous.kind),
           previous.name, previous.value,
           (unsigned int)selectedEntry,
           EspNativeGameplayHubContent_inventoryKindName(current.kind),
           current.name, current.value,
           (unsigned int)nextIndex,
           EspNativeGameplayHubContent_inventoryKindName(next.kind),
           next.name, next.value,
           (unsigned int)(sizeof(previous) + sizeof(current) + sizeof(next)),
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead);
    return 1;
}

int EspNativeGameplayHubTouchUi_paint(uint16_t* framebuffer,
                                      uint8_t page,
                                      uint8_t selectedRow) {
    EspNativeGameplayPlayerState player;
    uint8_t count;
    if (framebuffer == NULL || page >= ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT) return 0;

    fillRect(framebuffer, 1, 21, 158, 33, HUB_UI_PANEL);
    drawTab(framebuffer, HUB_UI_INV_LEFT, HUB_UI_INV_TOP,
            HUB_UI_INV_RIGHT, HUB_UI_INV_BOTTOM, "INV",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY);
    drawTab(framebuffer, HUB_UI_WPN_LEFT, HUB_UI_WPN_TOP,
            HUB_UI_WPN_RIGHT, HUB_UI_WPN_BOTTOM, "WPN",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS);
    drawTab(framebuffer, HUB_UI_STATUS_LEFT, HUB_UI_STATUS_TOP,
            HUB_UI_STATUS_RIGHT, HUB_UI_STATUS_BOTTOM, "STAT",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS);
    drawTab(framebuffer, HUB_UI_SYSTEM_LEFT, HUB_UI_SYSTEM_TOP,
            HUB_UI_SYSTEM_RIGHT, HUB_UI_SYSTEM_BOTTOM, "SYS",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM);

    memset(&player, 0, sizeof(player));
    if (!EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) return 0;

    if (page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) {
        count = EspNativeGameplayHubNonWeapon_entryCount(&player);
        if (count == 0U || selectedRow >= count) return 0;
        drawInventoryCards(framebuffer);
        return paintInventoryLabels(framebuffer, selectedRow);
    }
    if (page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS) {
        if (selectedRow >= ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT) return 0;
        return EspNativeGameplayHubWeaponGrid_paint(framebuffer, &player, selectedRow);
    }
    if (page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) return 1;
    return paintStatusDashboard(framebuffer, &player);
}

static void setHit(EspNativeGameplayTouchHit* hit,
                   uint8_t action,
                   uint8_t zone,
                   uint8_t left,
                   uint8_t top,
                   uint8_t right,
                   uint8_t bottom) {
    hit->action = action;
    hit->zone = zone;
    hit->left = left;
    hit->top = top;
    hit->right = right;
    hit->bottom = bottom;
}

static uint8_t zoneForAction(uint8_t action) {
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        return ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        return ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK;
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
        return ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT;
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
        return ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT;
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
        return ESP_NATIVE_GAMEPLAY_ZONE_SELECT;
    default:
        return ESP_NATIVE_GAMEPLAY_ZONE_NONE;
    }
}

static uint8_t tabAction(uint8_t currentPage, uint8_t targetPage) {
    if (currentPage == targetPage || currentPage >= ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT ||
        targetPage >= ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NONE;
    }
    return (uint8_t)(((currentPage + 1U) % ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT) ==
                             targetPage
                         ? ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT
                         : ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT);
}

static int classifyTab(const EspNativeGameplayHubView* view,
                       int logicalX,
                       int logicalY,
                       uint8_t targetPage,
                       int left,
                       int top,
                       int right,
                       int bottom,
                       EspNativeGameplayTouchHit* outHit) {
    uint8_t action;
    if (!inside(logicalX, logicalY, left, top, right, bottom)) return 0;
    if (view->page == targetPage) return -1;
    action = tabAction(view->page, targetPage);
    if (action == ESP_NATIVE_GAMEPLAY_ACTION_NONE) return -1;
    setHit(outHit, action, zoneForAction(action),
           (uint8_t)left, (uint8_t)top, (uint8_t)right, (uint8_t)bottom);
    return 1;
}

int EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHitBase) {
    EspNativeGameplayTouchHit* outHit = (EspNativeGameplayTouchHit*)outHitBase;
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    EspNativeGameplayPlayerState player;
    uint8_t count;
    int tab;

    if (outHit == NULL || view == NULL || view->active == 0U) return 0;
    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) return 0;
    if (logicalY < HUB_UI_TOP || logicalY > HUB_UI_BOTTOM) return 0;
    memset(outHit, 0, sizeof(*outHit));

    tab = classifyTab(view, logicalX, logicalY,
                      ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY,
                      HUB_UI_INV_LEFT, HUB_UI_INV_TOP,
                      HUB_UI_INV_RIGHT, HUB_UI_INV_BOTTOM, outHit);
    if (tab != 0) return tab;
    tab = classifyTab(view, logicalX, logicalY,
                      ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS,
                      HUB_UI_WPN_LEFT, HUB_UI_WPN_TOP,
                      HUB_UI_WPN_RIGHT, HUB_UI_WPN_BOTTOM, outHit);
    if (tab != 0) return tab;
    tab = classifyTab(view, logicalX, logicalY,
                      ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS,
                      HUB_UI_STATUS_LEFT, HUB_UI_STATUS_TOP,
                      HUB_UI_STATUS_RIGHT, HUB_UI_STATUS_BOTTOM, outHit);
    if (tab != 0) return tab;
    tab = classifyTab(view, logicalX, logicalY,
                      ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM,
                      HUB_UI_SYSTEM_LEFT, HUB_UI_SYSTEM_TOP,
                      HUB_UI_SYSTEM_RIGHT, HUB_UI_SYSTEM_BOTTOM, outHit);
    if (tab != 0) return tab;

    if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS) {
        uint8_t weapon;
        uint8_t left;
        uint8_t top;
        uint8_t right;
        uint8_t bottom;
        if (!EspNativeGameplayHubWeaponGrid_hitTest(logicalX, logicalY, &weapon)) {
            return -1;
        }
        EspNativeGameplayHubWeaponGrid_cellBounds(
            weapon, &left, &top, &right, &bottom);
        setHit(outHit, ESP_NATIVE_GAMEPLAY_ACTION_SELECT,
               ESP_NATIVE_GAMEPLAY_ZONE_SELECT,
               left, top, right, bottom);
        return 1;
    }

    if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) {
        uint8_t action;
        uint8_t top;
        uint8_t bottom;
        memset(&player, 0, sizeof(player));
        if (!EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
            return -1;
        }
        count = EspNativeGameplayHubNonWeapon_entryCount(&player);
        if (count == 0U || view->selectedRow >= count ||
            logicalX < HUB_UI_ROW_LEFT || logicalX > HUB_UI_ROW_TOUCH_RIGHT) {
            return -1;
        }
        if (logicalY >= HUB_UI_ROW0_TOP && logicalY <= HUB_UI_ROW0_BOTTOM) {
            action = ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
            top = HUB_UI_ROW0_TOP;
            bottom = HUB_UI_ROW0_BOTTOM;
        }
        else if (logicalY >= HUB_UI_ROW1_TOP && logicalY <= HUB_UI_ROW1_BOTTOM) {
            action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
            top = HUB_UI_ROW1_TOP;
            bottom = HUB_UI_ROW1_BOTTOM;
        }
        else if (logicalY >= HUB_UI_ROW2_TOP && logicalY <= HUB_UI_ROW2_BOTTOM) {
            action = ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK;
            top = HUB_UI_ROW2_TOP;
            bottom = HUB_UI_ROW2_BOTTOM;
        }
        else {
            return -1;
        }
        setHit(outHit, action, zoneForAction(action),
               HUB_UI_ROW_LEFT, top, HUB_UI_ROW_TOUCH_RIGHT, bottom);
        return 1;
    }

    return -1;
}

int EspNativeGameplayHubTouchUi_consumedWeaponTarget(uint8_t* outWeaponId) {
    const EspNativeGameplayInputState* input = EspNativeGameplayInput_peek();
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    if (outWeaponId == NULL || input == NULL || view == NULL ||
        view->active == 0U || view->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS ||
        input->active == 0U || input->pending != 0U ||
        input->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        input->zone != ESP_NATIVE_GAMEPLAY_ZONE_SELECT) return 0;
    return EspNativeGameplayHubWeaponGrid_hitTest(
        input->logicalX, input->logicalY, outWeaponId);
}

int EspNativeGameplayHubTouchUi_consumedPageTarget(uint8_t* outPage) {
    const EspNativeGameplayInputState* input = EspNativeGameplayInput_peek();
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    if (outPage == NULL || input == NULL || view == NULL ||
        view->active == 0U || input->active == 0U || input->pending != 0U ||
        (input->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT &&
         input->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT)) return 0;
    if (inside(input->logicalX, input->logicalY,
               HUB_UI_INV_LEFT, HUB_UI_INV_TOP,
               HUB_UI_INV_RIGHT, HUB_UI_INV_BOTTOM)) {
        *outPage = ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY;
        return 1;
    }
    if (inside(input->logicalX, input->logicalY,
               HUB_UI_WPN_LEFT, HUB_UI_WPN_TOP,
               HUB_UI_WPN_RIGHT, HUB_UI_WPN_BOTTOM)) {
        *outPage = ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS;
        return 1;
    }
    if (inside(input->logicalX, input->logicalY,
               HUB_UI_STATUS_LEFT, HUB_UI_STATUS_TOP,
               HUB_UI_STATUS_RIGHT, HUB_UI_STATUS_BOTTOM)) {
        *outPage = ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS;
        return 1;
    }
    if (inside(input->logicalX, input->logicalY,
               HUB_UI_SYSTEM_LEFT, HUB_UI_SYSTEM_TOP,
               HUB_UI_SYSTEM_RIGHT, HUB_UI_SYSTEM_BOTTOM)) {
        *outPage = ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM;
        return 1;
    }
    return 0;
}

int EspNativeGameplayHubTouchUi_consumedSelectTarget(
    uint8_t selectedRow,
    uint8_t entryCount,
    uint8_t* outTargetRow) {
    const EspNativeGameplayInputState* input = EspNativeGameplayInput_peek();
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    uint8_t target;
    if (outTargetRow == NULL || entryCount == 0U || selectedRow >= entryCount ||
        input == NULL || view == NULL || view->active == 0U ||
        view->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY ||
        input->active == 0U || input->pending != 0U ||
        input->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        input->zone != ESP_NATIVE_GAMEPLAY_ZONE_SELECT ||
        input->logicalX < HUB_UI_ROW_LEFT ||
        input->logicalX > HUB_UI_ROW_TOUCH_RIGHT) return 0;

    if (input->logicalY >= HUB_UI_ROW0_TOP && input->logicalY <= HUB_UI_ROW0_BOTTOM) {
        target = (uint8_t)((selectedRow + entryCount - 1U) % entryCount);
    }
    else if (input->logicalY >= HUB_UI_ROW1_TOP && input->logicalY <= HUB_UI_ROW1_BOTTOM) {
        target = selectedRow;
    }
    else if (input->logicalY >= HUB_UI_ROW2_TOP && input->logicalY <= HUB_UI_ROW2_BOTTOM) {
        target = (uint8_t)((selectedRow + 1U) % entryCount);
    }
    else {
        return 0;
    }
    *outTargetRow = target;
    return 1;
}
