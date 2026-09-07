#include <stddef.h>
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
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define HUB_FONT_NAME "a.bmp"
#define HUB_FACE_NAME "p.bmp"
#define HUB_FONT_WIDTH 9U
#define HUB_FONT_HEIGHT 12U
#define HUB_FONT_ADVANCE 7
#define HUB_FONT_SOURCE_WIDTH 144U
#define HUB_FONT_SOURCE_HEIGHT 72U
#define HUB_FONT_TRANSPARENT 1U
#define HUB_TOP_Y 20U
#define HUB_HEIGHT 80U
#define HUB_BOTTOM_Y (HUB_TOP_Y + HUB_HEIGHT)
#define HUB_LAST_Y (HUB_BOTTOM_Y - 1U)
#define HUB_BAND_ROWS 20U
#define HUB_BAND_PIXELS (DOOMRPG_LOGICAL_WIDTH * HUB_BAND_ROWS)

#define HUB_MENU_LEFT 0U
#define HUB_MENU_TOP 0U
#define HUB_MENU_WIDTH 32U
#define HUB_MENU_HEIGHT 20U
#define HUB_MENU_RIGHT (HUB_MENU_LEFT + HUB_MENU_WIDTH - 1U)
#define HUB_MENU_BOTTOM (HUB_MENU_TOP + HUB_MENU_HEIGHT - 1U)
#define HUB_MENU_PIXELS (HUB_MENU_WIDTH * HUB_MENU_HEIGHT)
#define HUB_FACE_FRAMES 1U
#define HUB_FACE_FRAME 0U
#define HUB_MENU_BG 0x0000U
#define HUB_MENU_BORDER 0xffffU
#define HUB_MENU_INNER 0x001fU

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native gameplay hub is defined for the 160x120 logical framebuffer"
#endif

typedef struct EspNativeGameplayHubMenuOverlay_s {
    uint16_t underlay[HUB_MENU_PIXELS];
    uint32_t baselineZoneFNV;
    uint32_t paintedZoneFNV;
    uint32_t baselineHudBandsFNV;
    uint8_t active;
    uint8_t reserved[3];
} EspNativeGameplayHubMenuOverlay;

static EspNativeGameplayHubView hub;
static EspNativeGameplayHubMenuOverlay menuOverlay;

static uint32_t fnv1aUpdate(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv1a32(const void* data, uint32_t bytes) {
    return fnv1aUpdate(2166136261U, data, bytes);
}

static int framebufferReady(void) {
    return Esp32PlatformVideo_framebuffer() != NULL &&
           Esp32PlatformVideo_framebufferSizeBytes() ==
               (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                   sizeof(uint16_t);
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

static uint32_t hudBandsFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t hash = 2166136261U;
    if (!framebufferReady()) return 0U;
    hash = fnv1aUpdate(hash, framebuffer,
                       HUB_BAND_PIXELS * (uint32_t)sizeof(uint16_t));
    return fnv1aUpdate(
        hash,
        framebuffer + HUB_BOTTOM_Y * DOOMRPG_LOGICAL_WIDTH,
        HUB_BAND_PIXELS * (uint32_t)sizeof(uint16_t));
}

static uint32_t hudProtectedFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t hash = 2166136261U;
    uint32_t y;
    if (!framebufferReady()) return 0U;

    for (y = 0U; y < HUB_BAND_ROWS; ++y) {
        hash = fnv1aUpdate(
            hash,
            framebuffer + y * DOOMRPG_LOGICAL_WIDTH + HUB_MENU_WIDTH,
            (DOOMRPG_LOGICAL_WIDTH - HUB_MENU_WIDTH) *
                (uint32_t)sizeof(uint16_t));
    }
    return fnv1aUpdate(
        hash,
        framebuffer + HUB_BOTTOM_Y * DOOMRPG_LOGICAL_WIDTH,
        HUB_BAND_PIXELS * (uint32_t)sizeof(uint16_t));
}

static uint32_t menuZoneFNV(const uint16_t* framebuffer) {
    uint32_t hash = 2166136261U;
    uint32_t y;
    if (framebuffer == NULL) return 0U;
    for (y = 0U; y < HUB_MENU_HEIGHT; ++y) {
        hash = fnv1aUpdate(
            hash,
            framebuffer + y * DOOMRPG_LOGICAL_WIDTH + HUB_MENU_LEFT,
            HUB_MENU_WIDTH * (uint32_t)sizeof(uint16_t));
    }
    return hash;
}

static int menuOverlayCapture(uint16_t* framebuffer) {
    uint32_t y;
    if (framebuffer == NULL || menuOverlay.active) return 0;
    memset(&menuOverlay, 0, sizeof(menuOverlay));
    menuOverlay.baselineHudBandsFNV = hudBandsFNV();
    if (menuOverlay.baselineHudBandsFNV == 0U) return 0;

    for (y = 0U; y < HUB_MENU_HEIGHT; ++y) {
        memcpy(menuOverlay.underlay + y * HUB_MENU_WIDTH,
               framebuffer + y * DOOMRPG_LOGICAL_WIDTH + HUB_MENU_LEFT,
               HUB_MENU_WIDTH * sizeof(uint16_t));
    }
    menuOverlay.baselineZoneFNV =
        fnv1a32(menuOverlay.underlay, sizeof(menuOverlay.underlay));
    if (menuOverlay.baselineZoneFNV == 0U) {
        memset(&menuOverlay, 0, sizeof(menuOverlay));
        return 0;
    }
    menuOverlay.active = 1U;
    return 1;
}

static int menuOverlayRestore(uint16_t* framebuffer) {
    uint32_t y;
    uint32_t zoneFNV;
    uint32_t bandsFNV;
    if (!menuOverlay.active) return 1;
    if (framebuffer == NULL) return 0;

    for (y = 0U; y < HUB_MENU_HEIGHT; ++y) {
        memcpy(framebuffer + y * DOOMRPG_LOGICAL_WIDTH + HUB_MENU_LEFT,
               menuOverlay.underlay + y * HUB_MENU_WIDTH,
               HUB_MENU_WIDTH * sizeof(uint16_t));
    }

    zoneFNV = menuZoneFNV(framebuffer);
    bandsFNV = hudBandsFNV();
    if (zoneFNV != menuOverlay.baselineZoneFNV ||
        bandsFNV != menuOverlay.baselineHudBandsFNV) {
        return 0;
    }
    menuOverlay.active = 0U;
    menuOverlay.paintedZoneFNV = 0U;
    return 1;
}

static void menuPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < (int)HUB_MENU_LEFT ||
        x > (int)HUB_MENU_RIGHT || y < (int)HUB_MENU_TOP ||
        y > (int)HUB_MENU_BOTTOM) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void menuFill(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) {
            menuPixel(framebuffer, x, y, color);
        }
    }
}

static void menuRect(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        menuPixel(framebuffer, x, top, color);
        menuPixel(framebuffer, x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        menuPixel(framebuffer, left, y, color);
        menuPixel(framebuffer, right, y, color);
    }
}

static int paintMenuButton(const EspNativeIndexedBmp* faces,
                           uint16_t* framebuffer,
                           EspNativeIndexedBmpStats* stats) {
    uint16_t faceHeight;
    int destinationX;
    int destinationY;

    if (!menuOverlay.active || faces == NULL || framebuffer == NULL ||
        stats == NULL || faces->width == 0U ||
        faces->height == 0U || faces->height % HUB_FACE_FRAMES != 0U) {
        return 0;
    }

    faceHeight = (uint16_t)(faces->height / HUB_FACE_FRAMES);
    if (faces->width > HUB_MENU_WIDTH || faceHeight > HUB_MENU_HEIGHT ||
        HUB_FACE_FRAME >= HUB_FACE_FRAMES) {
        return 0;
    }

    menuFill(framebuffer, 0, 0, 31, 19, HUB_MENU_BG);
    menuRect(framebuffer, 0, 0, 31, 19, HUB_MENU_BORDER);
    menuRect(framebuffer, 1, 1, 30, 18, HUB_MENU_INNER);

    destinationX = ((int)HUB_MENU_WIDTH - (int)faces->width) / 2;
    destinationY = ((int)HUB_MENU_HEIGHT - (int)faceHeight) / 2;

    if (EspNativeIndexedBmp_blit(
            faces,
            framebuffer,
            DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_HEIGHT,
            0U,
            (uint16_t)(HUB_FACE_FRAME * faceHeight),
            faces->width,
            faceHeight,
            (int16_t)destinationX,
            (int16_t)destinationY,
            1U,
            stats) != ESP_NATIVE_INDEXED_BMP_OK) {
        return 0;
    }

    menuOverlay.paintedZoneFNV = menuZoneFNV(framebuffer);
    return menuOverlay.paintedZoneFNV != 0U &&
           menuOverlay.paintedZoneFNV != menuOverlay.baselineZoneFNV;
}

static const char* pageName(uint8_t page) {
    switch (page) {
    case ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY: return "inventory";
    case ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS: return "status";
    default: return "unknown";
    }
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < (int)HUB_TOP_Y || y >= (int)HUB_BOTTOM_Y) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void clearViewport(uint16_t* framebuffer) {
    size_t pixels;
    if (framebuffer == NULL) return;
    pixels = (size_t)DOOMRPG_LOGICAL_WIDTH * HUB_HEIGHT;
    memset(framebuffer + HUB_TOP_Y * DOOMRPG_LOGICAL_WIDTH,
           0,
           pixels * sizeof(uint16_t));
}

static void drawBorder(uint16_t* framebuffer) {
    int x;
    int y;
    for (x = 0; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
        putPixel(framebuffer, x, (int)HUB_TOP_Y, 0xffffU);
        putPixel(framebuffer, x, (int)HUB_LAST_Y, 0xffffU);
    }
    for (y = (int)HUB_TOP_Y; y <= (int)HUB_LAST_Y; ++y) {
        putPixel(framebuffer, 0, y, 0xffffU);
        putPixel(framebuffer, DOOMRPG_LOGICAL_WIDTH - 1, y, 0xffffU);
    }
}

static int drawText(const EspNativeIndexedBmp* font,
                    uint16_t* framebuffer,
                    const char* text,
                    int x,
                    int y,
                    EspNativeIndexedBmpStats* stats) {
    const unsigned char* p = (const unsigned char*)text;
    if (font == NULL || framebuffer == NULL || text == NULL || stats == NULL ||
        y < (int)HUB_TOP_Y ||
        y + (int)HUB_FONT_HEIGHT > (int)HUB_BOTTOM_Y) {
        return 0;
    }
    while (*p != '\0') {
        uint8_t c = *p++;
        if (c == ' ') {
            x += HUB_FONT_ADVANCE;
            continue;
        }
        if (c < 33U || c > 127U) return 0;
        {
            uint8_t glyph = (uint8_t)(c - 33U);
            uint16_t sourceX =
                (uint16_t)(HUB_FONT_WIDTH * (glyph & 0x0fU));
            uint16_t sourceY =
                (uint16_t)(HUB_FONT_HEIGHT * (glyph >> 4));
            if (EspNativeIndexedBmp_blit(font,
                                         framebuffer,
                                         DOOMRPG_LOGICAL_WIDTH,
                                         DOOMRPG_LOGICAL_HEIGHT,
                                         sourceX,
                                         sourceY,
                                         HUB_FONT_WIDTH,
                                         HUB_FONT_HEIGHT,
                                         (int16_t)x,
                                         (int16_t)y,
                                         HUB_FONT_TRANSPARENT,
                                         stats) != ESP_NATIVE_INDEXED_BMP_OK) {
                return 0;
            }
        }
        x += HUB_FONT_ADVANCE;
        if (x >= DOOMRPG_LOGICAL_WIDTH) break;
    }
    return 1;
}

static int paintInventoryContent(const EspNativeGameplayPlayerState* player,
                                 const EspNativeIndexedBmp* font,
                                 uint16_t* framebuffer,
                                 EspNativeIndexedBmpStats* stats) {
    char line[32];
    uint8_t count;
    int ok = 1;

    if (player == NULL || font == NULL || framebuffer == NULL || stats == NULL) {
        return 0;
    }
    count = EspNativeGameplayHubContent_inventoryEntryCount(player);
    if (count == 0U || hub.selectedRow >= count) return 0;

    /* The visible tabs and three-card list own y=21..72. Keep the lower area as
     * a small read-only position/status footer instead of the old prototype
     * numeric inventory dump. */
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "ENTRY %u/%u",
             (unsigned int)hub.selectedRow + 1U,
             (unsigned int)count);
    ok = drawText(font, framebuffer, line, 4, 73, stats) && ok;
    ok = drawText(font, framebuffer, "READ ONLY", 4, 86, stats) && ok;
    return ok;
}

static int paintStatusContent(const EspNativeGameplayPlayerState* player,
                              const EspNativeIndexedBmp* font,
                              uint16_t* framebuffer,
                              EspNativeIndexedBmpStats* stats) {
    char line[32];
    uint8_t health;
    uint8_t maxHealth;
    uint8_t armor;
    uint8_t maxArmor;
    uint8_t defense;
    uint8_t strength;
    uint8_t agility;
    uint8_t accuracy;
    int ok = 1;

    if (player == NULL || font == NULL || framebuffer == NULL || stats == NULL) {
        return 0;
    }

    health = (uint8_t)(player->param1 & 0xffU);
    maxHealth = (uint8_t)((player->param1 >> 8) & 0xffU);
    armor = (uint8_t)((player->param1 >> 16) & 0xffU);
    maxArmor = (uint8_t)((player->param1 >> 24) & 0xffU);
    defense = (uint8_t)(player->param2 & 0xffU);
    strength = (uint8_t)((player->param2 >> 8) & 0xffU);
    agility = (uint8_t)((player->param2 >> 16) & 0xffU);
    accuracy = (uint8_t)((player->param2 >> 24) & 0xffU);

    memset(line, 0, sizeof(line));
    ok = drawText(font, framebuffer, "HUB < STATUS >", 4, 21, stats) && ok;

    snprintf(line, sizeof(line), "HP %u/%u AR %u/%u",
             (unsigned int)health,
             (unsigned int)maxHealth,
             (unsigned int)armor,
             (unsigned int)maxArmor);
    ok = drawText(font, framebuffer, line, 4, 34, stats) && ok;

    snprintf(line, sizeof(line), "LV %u XP %lu/%lu",
             (unsigned int)player->level,
             (unsigned long)player->currentXP,
             (unsigned long)player->nextLevelXP);
    ok = drawText(font, framebuffer, line, 4, 47, stats) && ok;

    snprintf(line, sizeof(line), "DEF %u STR %u",
             (unsigned int)defense,
             (unsigned int)strength);
    ok = drawText(font, framebuffer, line, 4, 60, stats) && ok;

    snprintf(line, sizeof(line), "AGI %u ACC %u",
             (unsigned int)agility,
             (unsigned int)accuracy);
    ok = drawText(font, framebuffer, line, 4, 73, stats) && ok;

    snprintf(line, sizeof(line), "C %lu K %08lX",
             (unsigned long)player->credits,
             (unsigned long)player->keys);
    ok = drawText(font, framebuffer, line, 4, 86, stats) && ok;
    return ok;
}

static EspNativeGameplayHubStatus paintCurrentPage(void) {
    EspNativeGameplayPlayerState before;
    EspNativeGameplayPlayerState after;
    EspNativeIndexedBmp font;
    EspNativeIndexedBmp faces;
    EspNativeIndexedBmpStats stats;
    uint16_t* framebuffer;
    uint32_t fnvBefore;
    uint32_t fnvAfter;
    uint32_t paintedFNV;
    uint32_t protectedBefore;
    uint32_t protectedAfter;
    uint32_t menuBefore;
    uint32_t menuAfter;
    int ok;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    memset(&font, 0, sizeof(font));
    memset(&faces, 0, sizeof(faces));
    memset(&stats, 0, sizeof(stats));

    if (hub.active == 0U || hub.page >= ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT ||
        !EspNativeGameplayPlayerState_snapshot(&before) || before.active != 1U ||
        !menuOverlay.active) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }
    fnvBefore = EspNativeGameplayPlayerState_fingerprint();
    if (fnvBefore == 0U || fnvBefore != hub.playerFNVAtOpen ||
        EspAssetPack_isOpen()) {
        return EspAssetPack_isOpen()
                   ? ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY
                   : ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (!framebufferReady()) return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;

    protectedBefore = hudProtectedFNV();
    menuBefore = menuZoneFNV(framebuffer);
    if (protectedBefore == 0U || menuBefore == 0U) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }
    if (EspNativeIndexedBmp_open(HUB_FONT_NAME, &font, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        font.width != HUB_FONT_SOURCE_WIDTH ||
        font.height != HUB_FONT_SOURCE_HEIGHT ||
        EspNativeIndexedBmp_open(HUB_FACE_NAME, &faces, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK) {
        EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    /* The HUB owns the resident 160x80 world viewport plus one explicit,
     * bounded 32x20 MENU underlay. Every other HUD pixel remains protected. */
    clearViewport(framebuffer);
    drawBorder(framebuffer);

    if (hub.page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY) {
        ok = paintInventoryContent(&before, &font, framebuffer, &stats);
    }
    else if (hub.page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
        ok = paintStatusContent(&before, &font, framebuffer, &stats);
    }
    else {
        ok = 0;
    }

    if (ok && !EspNativeGameplayHubTouchUi_paint(framebuffer,
                                                  hub.page,
                                                  hub.selectedRow)) {
        ok = 0;
    }
    if (ok && !paintMenuButton(&faces, framebuffer, &stats)) {
        ok = 0;
    }

    EspAssetPack_close();
    if (!ok || EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    fnvAfter = EspNativeGameplayPlayerState_fingerprint();
    protectedAfter = hudProtectedFNV();
    menuAfter = menuZoneFNV(framebuffer);
    if (!EspNativeGameplayPlayerState_snapshot(&after) ||
        fnvAfter != fnvBefore || memcmp(&before, &after, sizeof(before)) != 0 ||
        protectedAfter == 0U || protectedAfter != protectedBefore ||
        menuAfter == 0U || menuAfter != menuOverlay.paintedZoneFNV) {
        printf("[HUB] PAINT-DEFER page=%s player=%08x->%08x hudProtected=%08x->%08x menuZone=%08x->%08x playerExact=%s hudProtectedExact=%s menuButtonExact=%s mutation=no turn=no\n",
               pageName(hub.page),
               (unsigned int)fnvBefore,
               (unsigned int)fnvAfter,
               (unsigned int)protectedBefore,
               (unsigned int)protectedAfter,
               (unsigned int)menuBefore,
               (unsigned int)menuAfter,
               fnvAfter == fnvBefore ? "yes" : "NO",
               protectedAfter == protectedBefore ? "yes" : "NO",
               menuAfter == menuOverlay.paintedZoneFNV ? "yes" : "NO");
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    paintedFNV = frameFNV();
    if (paintedFNV == 0U || !Esp32PlatformVideo_present()) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    ++hub.paints;
    hub.lastPlayerFNV = fnvAfter;
    hub.lastFrameFNV = paintedFNV;
    printf("[HUB] FRAME paint=%u page=%s row=%u frame=%08x viewport=160x80/y20..99 hudProtected=%08x preserved=yes menuButton=hand asset=p.bmp frame=%u menuZone=%08x underlayBytes=%u reads=%u bytes=%u playerFNV=%08x exact=yes packClosed=yes presented=1 mutation=no turn=no\n",
           (unsigned int)hub.paints,
           pageName(hub.page),
           (unsigned int)hub.selectedRow,
           (unsigned int)paintedFNV,
           (unsigned int)protectedAfter,
           (unsigned int)HUB_FACE_FRAME,
           (unsigned int)menuAfter,
           (unsigned int)sizeof(menuOverlay.underlay),
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead,
           (unsigned int)fnvAfter);
    return ESP_NATIVE_GAMEPLAY_HUB_OK;
}

void EspNativeGameplayHub_reset(void) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (menuOverlay.active && framebufferReady()) {
        (void)menuOverlayRestore(framebuffer);
    }
    memset(&menuOverlay, 0, sizeof(menuOverlay));
    memset(&hub, 0, sizeof(hub));
}

int EspNativeGameplayHub_isActive(void) {
    return hub.active != 0U;
}

const EspNativeGameplayHubView* EspNativeGameplayHub_view(void) {
    return &hub;
}

EspNativeGameplayHubStatus EspNativeGameplayHub_open(void) {
    EspNativeGameplayPlayerState player;
    EspNativeGameplayHubStatus status;
    uint16_t* framebuffer;
    uint32_t playerFNV;

    memset(&player, 0, sizeof(player));
    if (hub.active != 0U) return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
    if (EspAssetPack_isOpen()) return ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY;
    if (!EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }
    playerFNV = EspNativeGameplayPlayerState_fingerprint();
    if (playerFNV == 0U || !framebufferReady()) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (!menuOverlayCapture(framebuffer)) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    ++hub.opens;
    hub.selectedRow = 0U;
    hub.page = ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY;
    hub.playerFNVAtOpen = playerFNV;
    hub.lastPlayerFNV = playerFNV;
    hub.active = 1U;

    status = paintCurrentPage();
    if (status != ESP_NATIVE_GAMEPLAY_HUB_OK) {
        (void)menuOverlayRestore(framebuffer);
        memset(&menuOverlay, 0, sizeof(menuOverlay));
        hub.active = 0U;
        return status;
    }

    printf("[HUB] OPEN n=%u mode=inventory+status-readonly page=%s pages=%u viewport=160x80/y20..99 menuButton=hand asset=p.bmp frame=%u menuUnderlayBytes=%u hudProtected=preserved ownerBytes=%u playerStateBytes=%u playerFNV=%08x weapon=%u weapons=%03x ammo=%02u/%02u/%02u/%02u/%02u/%02u items=%02u/%02u/%02u/%02u/%02u keys=%08lx credits=%lu mutation=no turn=no packClosed=yes\n",
           (unsigned int)hub.opens,
           pageName(hub.page),
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT,
           (unsigned int)HUB_FACE_FRAME,
           (unsigned int)sizeof(menuOverlay.underlay),
           (unsigned int)sizeof(hub),
           (unsigned int)sizeof(player),
           (unsigned int)playerFNV,
           (unsigned int)player.weapon,
           (unsigned int)(player.weapons & 0x0fffU),
           (unsigned int)player.ammo[0],
           (unsigned int)player.ammo[1],
           (unsigned int)player.ammo[2],
           (unsigned int)player.ammo[3],
           (unsigned int)player.ammo[4],
           (unsigned int)player.ammo[5],
           (unsigned int)player.inventory[0],
           (unsigned int)player.inventory[1],
           (unsigned int)player.inventory[2],
           (unsigned int)player.inventory[3],
           (unsigned int)player.inventory[4],
           (unsigned long)player.keys,
           (unsigned long)player.credits);
    return ESP_NATIVE_GAMEPLAY_HUB_OK;
}

EspNativeGameplayHubStatus EspNativeGameplayHub_handleAction(uint8_t action) {
    EspNativeGameplayHubStatus status;
    EspNativeGameplayPlayerState player;
    uint16_t* framebuffer;
    uint32_t playerFNV;
    uint32_t restoredHudBands;
    uint32_t expectedHudBands;
    uint8_t beforeRow;
    uint8_t beforePage;
    uint8_t inventoryEntries;
    int menuRestored;

    if (hub.active == 0U) return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {
        playerFNV = EspNativeGameplayPlayerState_fingerprint();
        framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
        expectedHudBands = menuOverlay.baselineHudBandsFNV;
        menuRestored = framebufferReady() && menuOverlayRestore(framebuffer);
        restoredHudBands = hudBandsFNV();

        hub.active = 0U;
        ++hub.closes;
        hub.lastPlayerFNV = playerFNV;
        printf("[HUB] CLOSE n=%u page=%s playerFNV=%08x->%08x exact=%s mutation=no turn=no worldRedraw=pending viewportOnly=yes menuUnderlayRestore=%s hudBands=%08x expected=%08x exact=%s packClosed=%s\n",
               (unsigned int)hub.closes,
               pageName(hub.page),
               (unsigned int)hub.playerFNVAtOpen,
               (unsigned int)playerFNV,
               playerFNV == hub.playerFNVAtOpen ? "yes" : "NO",
               menuRestored ? "exact" : "FAILED",
               (unsigned int)restoredHudBands,
               (unsigned int)expectedHudBands,
               restoredHudBands != 0U && restoredHudBands == expectedHudBands
                   ? "yes" : "NO",
               EspAssetPack_isOpen() ? "NO" : "yes");
        return (playerFNV != 0U && playerFNV == hub.playerFNVAtOpen &&
                menuRestored && restoredHudBands != 0U &&
                restoredHudBands == expectedHudBands &&
                !EspAssetPack_isOpen())
                   ? ESP_NATIVE_GAMEPLAY_HUB_CLOSED
                   : ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT) {
        printf("[HUB] SELECT-DEFER page=%s row=%u cause=read-only-milestone mutation=no turn=no\n",
               pageName(hub.page),
               (unsigned int)hub.selectedRow);
        return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
    }

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ||
        action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT) {
        beforePage = hub.page;
        if (action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT) {
            hub.page = (uint8_t)((hub.page + ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT - 1U) %
                                 ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT);
        }
        else {
            hub.page = (uint8_t)((hub.page + 1U) %
                                 ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT);
        }
        status = paintCurrentPage();
        if (status != ESP_NATIVE_GAMEPLAY_HUB_OK) {
            hub.page = beforePage;
            return status;
        }
        printf("[HUB] PAGE page=%s->%s direction=%s playerMutation=no turn=no worldDispatch=blocked\n",
               pageName(beforePage),
               pageName(hub.page),
               action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ? "left" : "right");
        return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
    }

    if (hub.page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY &&
        (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
         action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK)) {
        memset(&player, 0, sizeof(player));
        if (!EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
            return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
        }
        inventoryEntries =
            EspNativeGameplayHubContent_inventoryEntryCount(&player);
        if (inventoryEntries == 0U || hub.selectedRow >= inventoryEntries) {
            return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
        }

        beforeRow = hub.selectedRow;
        if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD) {
            hub.selectedRow = (uint8_t)((hub.selectedRow + inventoryEntries - 1U) %
                                        inventoryEntries);
        }
        else {
            hub.selectedRow =
                (uint8_t)((hub.selectedRow + 1U) % inventoryEntries);
        }

        status = paintCurrentPage();
        if (status != ESP_NATIVE_GAMEPLAY_HUB_OK) {
            hub.selectedRow = beforeRow;
            return status;
        }
        printf("[HUB] CURSOR page=inventory entry=%u->%u entries=%u direction=%s mutation=no turn=no\n",
               (unsigned int)beforeRow,
               (unsigned int)hub.selectedRow,
               (unsigned int)inventoryEntries,
               action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ? "up" : "down");
        return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
    }

    printf("[HUB] IGNORE action=%u page=%s row=%u worldDispatch=blocked mutation=no turn=no\n",
           (unsigned int)action,
           pageName(hub.page),
           (unsigned int)hub.selectedRow);
    return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
}

const char* EspNativeGameplayHub_statusName(EspNativeGameplayHubStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_HUB_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_HUB_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY: return "PACK_BUSY";
    case ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED: return "IO_FAILED";
    case ESP_NATIVE_GAMEPLAY_HUB_IGNORED: return "IGNORED";
    case ESP_NATIVE_GAMEPLAY_HUB_REDRAWN: return "REDRAWN";
    case ESP_NATIVE_GAMEPLAY_HUB_CLOSED: return "CLOSED";
    case ESP_NATIVE_GAMEPLAY_HUB_OK: return "OK";
    default: return "UNKNOWN";
    }
}
