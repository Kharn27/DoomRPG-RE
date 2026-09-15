#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_content.h"
#include "esp_native_gameplay_hub_nonweapon.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_hub_weapon_grid.h"
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

#define HUB_UI_INV_LEFT 7
#define HUB_UI_INV_TOP 21
#define HUB_UI_INV_RIGHT 50
#define HUB_UI_INV_BOTTOM 33
#define HUB_UI_WPN_LEFT 58
#define HUB_UI_WPN_TOP 21
#define HUB_UI_WPN_RIGHT 100
#define HUB_UI_WPN_BOTTOM 33
#define HUB_UI_STATUS_LEFT 108
#define HUB_UI_STATUS_TOP 21
#define HUB_UI_STATUS_RIGHT 151
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
    static const uint8_t I[5] = {7U, 2U, 2U, 2U, 7U};
    static const uint8_t N[5] = {5U, 7U, 7U, 7U, 5U};
    static const uint8_t V[5] = {5U, 5U, 5U, 5U, 2U};
    static const uint8_t W[5] = {5U, 5U, 7U, 7U, 5U};
    static const uint8_t P[5] = {6U, 5U, 6U, 4U, 4U};
    static const uint8_t S[5] = {7U, 4U, 7U, 1U, 7U};
    static const uint8_t T[5] = {7U, 2U, 2U, 2U, 2U};
    static const uint8_t A[5] = {2U, 5U, 7U, 5U, 5U};
    const uint8_t* source = NULL;
    switch (c) {
    case 'I': source = I; break;
    case 'N': source = N; break;
    case 'V': source = V; break;
    case 'W': source = W; break;
    case 'P': source = P; break;
    case 'S': source = S; break;
    case 'T': source = T; break;
    case 'A': source = A; break;
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
    x = centerX - width / 2;
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
    drawMiniText(framebuffer, label, (left + right) / 2,
                 top + 2, 2, HUB_UI_WHITE);
}

static void drawInventoryCards(uint16_t* framebuffer) {
    static const int tops[3] = {33, 46, 59};
    static const int bottoms[3] = {46, 59, 72};
    int row;
    for (row = 0; row < 3; ++row) {
        uint16_t color = row == 1 ? HUB_UI_WHITE : HUB_UI_DIM_BLUE;
        drawRect(framebuffer, HUB_UI_ROW_LEFT, tops[row],
                 HUB_UI_ROW_RIGHT, bottoms[row], color);
        if (row == 1) {
            int y;
            for (y = tops[row] + 3; y <= tops[row] + 10; ++y) {
                putPixel(framebuffer, HUB_UI_ROW_LEFT + 1, y, HUB_UI_BLUE);
            }
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

    fillRect(framebuffer, 4, HUB_UI_ROW0_TOP, 156, HUB_UI_ROW0_BOTTOM, HUB_UI_BLACK);
    fillRect(framebuffer, 4, HUB_UI_ROW1_TOP, 156, HUB_UI_ROW1_BOTTOM, HUB_UI_BLACK);
    fillRect(framebuffer, 4, HUB_UI_ROW2_TOP, 156, HUB_UI_ROW2_BOTTOM, HUB_UI_BLACK);

    ok = formatEntryLine(line, sizeof(line), ' ', &previous) &&
         drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW0_TOP, &stats);
    ok = formatEntryLine(line, sizeof(line), '>', &current) &&
         drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW1_TOP, &stats) && ok;
    ok = formatEntryLine(line, sizeof(line), ' ', &next) &&
         drawDoomText(&font, framebuffer, line, 4, HUB_UI_ROW2_TOP, &stats) && ok;
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
    drawStatusCards(framebuffer);
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
