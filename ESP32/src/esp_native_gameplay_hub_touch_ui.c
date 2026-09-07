#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_content.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_config.h"

#define HUB_UI_TOP 20
#define HUB_UI_BOTTOM 99
#define HUB_UI_LEFT 0
#define HUB_UI_RIGHT 159

#define HUB_UI_BLACK 0x0000U
#define HUB_UI_PANEL 0x0008U
#define HUB_UI_DIM_BLUE 0x0010U
#define HUB_UI_BLUE 0x001fU
#define HUB_UI_WHITE 0xffffU

#define HUB_UI_FONT_NAME "a.bmp"
#define HUB_UI_FONT_WIDTH 9U
#define HUB_UI_FONT_HEIGHT 12U
#define HUB_UI_FONT_ADVANCE 7
#define HUB_UI_FONT_SOURCE_WIDTH 144U
#define HUB_UI_FONT_SOURCE_HEIGHT 72U
#define HUB_UI_FONT_TRANSPARENT 1U

/* With the redundant viewport X removed, keep the two page tabs centered while
 * retaining the hardware-proven widths, so touch feedback stays below the
 * permanent 512-edit bound. */
#define HUB_UI_INV_LEFT 21
#define HUB_UI_INV_TOP 21
#define HUB_UI_INV_RIGHT 64
#define HUB_UI_INV_BOTTOM 33

#define HUB_UI_STATUS_LEFT 67
#define HUB_UI_STATUS_TOP 21
#define HUB_UI_STATUS_RIGHT 134
#define HUB_UI_STATUS_BOTTOM 33

#define HUB_UI_ROW_LEFT 2
#define HUB_UI_ROW_RIGHT 157
#define HUB_UI_ROW_TOUCH_RIGHT 105
#define HUB_UI_ROW0_TOP 34
#define HUB_UI_ROW0_BOTTOM 45
#define HUB_UI_ROW1_TOP 47
#define HUB_UI_ROW1_BOTTOM 58
#define HUB_UI_ROW2_TOP 60
#define HUB_UI_ROW2_BOTTOM 71

static int inside(int x, int y, int left, int top, int right, int bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < HUB_UI_LEFT || x > HUB_UI_RIGHT ||
        y < HUB_UI_TOP || y > HUB_UI_BOTTOM) {
        return;
    }
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
    static const uint8_t I[5] = {7U, 2U, 2U, 2U, 7U};
    static const uint8_t N[5] = {5U, 7U, 7U, 7U, 5U};
    static const uint8_t V[5] = {5U, 5U, 5U, 5U, 2U};
    static const uint8_t S[5] = {7U, 4U, 7U, 1U, 7U};
    static const uint8_t T[5] = {7U, 2U, 2U, 2U, 2U};
    static const uint8_t A[5] = {2U, 5U, 7U, 5U, 5U};
    static const uint8_t U[5] = {5U, 5U, 5U, 5U, 7U};
    const uint8_t* source = NULL;

    switch (c) {
    case 'I': source = I; break;
    case 'N': source = N; break;
    case 'V': source = V; break;
    case 'S': source = S; break;
    case 'T': source = T; break;
    case 'A': source = A; break;
    case 'U': source = U; break;
    default: return 0;
    }
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

static void drawMiniText(uint16_t* framebuffer,
                         const char* text,
                         int centerX,
                         int top,
                         int scale,
                         uint16_t color) {
    int x;
    int width;
    if (framebuffer == NULL || text == NULL || scale <= 0) return;
    width = miniTextWidth(text, scale);
    x = centerX - (width / 2);
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

static void drawTab(uint16_t* framebuffer,
                    int left,
                    int top,
                    int right,
                    int bottom,
                    const char* label,
                    int selected) {
    fillRect(framebuffer, left, top, right, bottom,
             selected ? HUB_UI_DIM_BLUE : HUB_UI_BLACK);
    drawRect(framebuffer, left, top, right, bottom,
             selected ? HUB_UI_WHITE : HUB_UI_BLUE);
    drawMiniText(framebuffer,
                 label,
                 (left + right) / 2,
                 top + 2,
                 2,
                 HUB_UI_WHITE);
}

static void drawInventoryCards(uint16_t* framebuffer, uint8_t selectedRow) {
    static const int tops[3] = {33, 46, 59};
    static const int bottoms[3] = {46, 59, 72};
    int row;
    for (row = 0; row < 3; ++row) {
        uint16_t color = (selectedRow == (uint8_t)row) ? HUB_UI_WHITE
                                                       : HUB_UI_DIM_BLUE;
        drawRect(framebuffer,
                 HUB_UI_ROW_LEFT,
                 tops[row],
                 HUB_UI_ROW_RIGHT,
                 bottoms[row],
                 color);
        if (selectedRow == (uint8_t)row) {
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 3, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 4, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 5, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 6, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 7, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 8, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 9, HUB_UI_BLUE);
            putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, tops[row] + 10, HUB_UI_BLUE);
        }
    }
}

static void drawStatusCards(uint16_t* framebuffer) {
    static const int tops[5] = {33, 46, 59, 72, 85};
    static const int bottoms[5] = {46, 59, 72, 85, 98};
    int row;
    for (row = 0; row < 5; ++row) {
        drawRect(framebuffer, 2, tops[row], 157, bottoms[row], HUB_UI_DIM_BLUE);
    }
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
            uint16_t sourceX =
                (uint16_t)(HUB_UI_FONT_WIDTH * (glyph & 0x0fU));
            uint16_t sourceY =
                (uint16_t)(HUB_UI_FONT_HEIGHT * (glyph >> 4));
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

static int paintInventoryLabels(uint16_t* framebuffer, uint8_t selectedRow) {
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    EspNativeGameplayPlayerState player;
    EspNativeGameplayHubContent content;
    EspNativeIndexedBmp font;
    EspNativeIndexedBmpStats stats;
    const char* ammoLabel;
    char line[32];
    int ok = 1;

    memset(&player, 0, sizeof(player));
    memset(&content, 0, sizeof(content));
    memset(&font, 0, sizeof(font));
    memset(&stats, 0, sizeof(stats));
    memset(line, 0, sizeof(line));

    if (framebuffer == NULL || view == NULL || view->active == 0U ||
        !EspAssetPack_isOpen() ||
        !EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
        return 0;
    }

    /* Strict first-ever paint witness. It proves the exact legacy type/subtype
     * reverse mapping and all historical names without adding a probe flag or
     * persistent string owner: opens==1/paints==0 is already-owned HUB state. */
    if (view->opens == 1U && view->paints == 0U &&
        !EspNativeGameplayHubContent_probeCatalog()) {
        return 0;
    }

    if (!EspNativeGameplayHubContent_snapshot(&player, &content) ||
        !EspAssetPack_isOpen()) {
        return 0;
    }
    ammoLabel = EspNativeGameplayHubContent_ammoLabel(content.weaponAmmoType);
    if (ammoLabel == NULL ||
        EspNativeIndexedBmp_open(HUB_UI_FONT_NAME, &font, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        font.width != HUB_UI_FONT_SOURCE_WIDTH ||
        font.height != HUB_UI_FONT_SOURCE_HEIGHT) {
        return 0;
    }

    /* Remove only the old prototype numeric text inside each proven card. Card
     * borders/touch geometry stay unchanged, then the original Doom RPG font
     * draws data-driven names on top. */
    fillRect(framebuffer, 3, HUB_UI_ROW0_TOP, 156, HUB_UI_ROW0_BOTTOM, HUB_UI_BLACK);
    fillRect(framebuffer, 3, HUB_UI_ROW1_TOP, 156, HUB_UI_ROW1_BOTTOM, HUB_UI_BLACK);
    fillRect(framebuffer, 3, HUB_UI_ROW2_TOP, 156, HUB_UI_ROW2_BOTTOM, HUB_UI_BLACK);

    snprintf(line, sizeof(line), "%c%s",
             selectedRow == 0U ? '>' : ' ', content.weaponName);
    ok = drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW0_TOP, &stats) && ok;

    if (content.weaponAmmoUsage == 0U) {
        snprintf(line, sizeof(line), "%cAmmo --",
                 selectedRow == 1U ? '>' : ' ');
    }
    else {
        snprintf(line, sizeof(line), "%c%s %02u",
                 selectedRow == 1U ? '>' : ' ',
                 ammoLabel,
                 (unsigned int)content.weaponAmmoValue);
    }
    ok = drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW1_TOP, &stats) && ok;

    if (content.hasItem) {
        snprintf(line, sizeof(line), "%c%s x%02u",
                 selectedRow == 2U ? '>' : ' ',
                 content.itemName,
                 (unsigned int)content.firstItemCount);
    }
    else {
        snprintf(line, sizeof(line), "%cItems none",
                 selectedRow == 2U ? '>' : ' ');
    }
    ok = drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW2_TOP, &stats) && ok;

    if (!ok || !EspAssetPack_isOpen()) return 0;

    printf("[HUBCONTENT] FRAME weapon=%u name=\"%s\" ammoType=%u ammoLabel=\"%s\" ammo=%u usage=%u item=%s%s%s count=%u selected=%u transientBytes=%u persistentNameBytes=0 fontReads=%u fontBytes=%u packOwnership=preserved-open mutation=no turn=no\n",
           (unsigned int)player.weapon,
           content.weaponName,
           (unsigned int)content.weaponAmmoType,
           ammoLabel,
           (unsigned int)content.weaponAmmoValue,
           (unsigned int)content.weaponAmmoUsage,
           content.hasItem ? "\"" : "none",
           content.hasItem ? content.itemName : "",
           content.hasItem ? "\"" : "",
           (unsigned int)content.firstItemCount,
           (unsigned int)selectedRow,
           (unsigned int)sizeof(content),
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead);
    return 1;
}

int EspNativeGameplayHubTouchUi_paint(uint16_t* framebuffer,
                                      uint8_t page,
                                      uint8_t selectedRow) {
    if (framebuffer == NULL || page >= ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT ||
        selectedRow >= 3U) {
        return 0;
    }

    /* The close affordance now lives in the real top-left MENU zone. The HUB
     * viewport only needs page tabs and page content. */
    fillRect(framebuffer, 1, 21, 158, 33, HUB_UI_PANEL);
    drawTab(framebuffer,
            HUB_UI_INV_LEFT,
            HUB_UI_INV_TOP,
            HUB_UI_INV_RIGHT,
            HUB_UI_INV_BOTTOM,
            "INV",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY);
    drawTab(framebuffer,
            HUB_UI_STATUS_LEFT,
            HUB_UI_STATUS_TOP,
            HUB_UI_STATUS_RIGHT,
            HUB_UI_STATUS_BOTTOM,
            "STATUS",
            page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS);

    if (page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) {
        drawInventoryCards(framebuffer, selectedRow);
        if (!paintInventoryLabels(framebuffer, selectedRow)) return 0;
    }
    else {
        drawStatusCards(framebuffer);
    }
    return 1;
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

static uint8_t actionToRow(uint8_t selectedRow, uint8_t targetRow) {
    if (selectedRow == targetRow) return ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
    if ((uint8_t)((selectedRow + 1U) % 3U) == targetRow) {
        return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK;
    }
    return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
}

int EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHitBase) {
    EspNativeGameplayTouchHit* outHit = (EspNativeGameplayTouchHit*)outHitBase;
    const EspNativeGameplayHubView* view = EspNativeGameplayHub_view();
    uint8_t action;
    uint8_t targetRow;

    if (outHit == NULL || view == NULL || view->active == 0U) return 0;
    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }

    /* y=0..19 intentionally falls through to the permanent top-HUD mapping:
     * the existing x=0..31 MENU zone is now visibly decorated while HUB is
     * active. The HUB exclusively owns every touch inside y=20..99. */
    if (logicalY < HUB_UI_TOP || logicalY > HUB_UI_BOTTOM) return 0;
    memset(outHit, 0, sizeof(*outHit));

    if (inside(logicalX, logicalY,
               HUB_UI_INV_LEFT, HUB_UI_INV_TOP,
               HUB_UI_INV_RIGHT, HUB_UI_INV_BOTTOM)) {
        if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) return -1;
        setHit(outHit,
               ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT,
               ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT,
               HUB_UI_INV_LEFT, HUB_UI_INV_TOP,
               HUB_UI_INV_RIGHT, HUB_UI_INV_BOTTOM);
        return 1;
    }

    if (inside(logicalX, logicalY,
               HUB_UI_STATUS_LEFT, HUB_UI_STATUS_TOP,
               HUB_UI_STATUS_RIGHT, HUB_UI_STATUS_BOTTOM)) {
        if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) return -1;
        setHit(outHit,
               ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT,
               ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT,
               HUB_UI_STATUS_LEFT, HUB_UI_STATUS_TOP,
               HUB_UI_STATUS_RIGHT, HUB_UI_STATUS_BOTTOM);
        return 1;
    }

    if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) {
        if (logicalY >= HUB_UI_ROW0_TOP && logicalY <= HUB_UI_ROW0_BOTTOM) {
            targetRow = 0U;
        }
        else if (logicalY >= HUB_UI_ROW1_TOP && logicalY <= HUB_UI_ROW1_BOTTOM) {
            targetRow = 1U;
        }
        else if (logicalY >= HUB_UI_ROW2_TOP && logicalY <= HUB_UI_ROW2_BOTTOM) {
            targetRow = 2U;
        }
        else {
            return -1;
        }

        if (logicalX < HUB_UI_ROW_LEFT || logicalX > HUB_UI_ROW_TOUCH_RIGHT) {
            return -1;
        }
        action = actionToRow(view->selectedRow, targetRow);
        setHit(outHit,
               action,
               zoneForAction(action),
               HUB_UI_ROW_LEFT,
               targetRow == 0U ? HUB_UI_ROW0_TOP
                   : (targetRow == 1U ? HUB_UI_ROW1_TOP : HUB_UI_ROW2_TOP),
               HUB_UI_ROW_TOUCH_RIGHT,
               targetRow == 0U ? HUB_UI_ROW0_BOTTOM
                   : (targetRow == 1U ? HUB_UI_ROW1_BOTTOM : HUB_UI_ROW2_BOTTOM));
        return 1;
    }

    return -1;
}
