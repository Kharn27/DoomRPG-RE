#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define HUB_FONT_NAME "a.bmp"
#define HUB_FONT_WIDTH 9U
#define HUB_FONT_HEIGHT 12U
#define HUB_FONT_ADVANCE 7
#define HUB_FONT_SOURCE_WIDTH 144U
#define HUB_FONT_SOURCE_HEIGHT 72U
#define HUB_FONT_TRANSPARENT 1U
#define HUB_ROWS 3U

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native gameplay hub is defined for the 160x120 logical framebuffer"
#endif

static EspNativeGameplayHubView hub;

static uint32_t fnv1a32(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void clearFrame(uint16_t* framebuffer) {
    size_t pixels;
    if (framebuffer == NULL) return;
    pixels = (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT;
    memset(framebuffer, 0, pixels * sizeof(uint16_t));
}

static void drawBorder(uint16_t* framebuffer) {
    int x;
    int y;
    for (x = 0; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
        putPixel(framebuffer, x, 0, 0xffffU);
        putPixel(framebuffer, x, DOOMRPG_LOGICAL_HEIGHT - 1, 0xffffU);
    }
    for (y = 0; y < DOOMRPG_LOGICAL_HEIGHT; ++y) {
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
    if (font == NULL || framebuffer == NULL || text == NULL || stats == NULL) {
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

static EspNativeGameplayHubStatus paintInventory(void) {
    EspNativeGameplayPlayerState before;
    EspNativeGameplayPlayerState after;
    EspNativeIndexedBmp font;
    EspNativeIndexedBmpStats stats;
    uint16_t* framebuffer;
    uint32_t fnvBefore;
    uint32_t fnvAfter;
    uint32_t paintedFNV;
    char line[32];
    int ok = 1;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    memset(&font, 0, sizeof(font));
    memset(&stats, 0, sizeof(stats));
    memset(line, 0, sizeof(line));

    if (hub.active == 0U || hub.page != 0U ||
        !EspNativeGameplayPlayerState_snapshot(&before) || before.active != 1U) {
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
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }
    if (EspNativeIndexedBmp_open(HUB_FONT_NAME, &font, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        font.width != HUB_FONT_SOURCE_WIDTH ||
        font.height != HUB_FONT_SOURCE_HEIGHT) {
        EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    clearFrame(framebuffer);
    drawBorder(framebuffer);
    ok = drawText(&font, framebuffer, "DOOM RPG // HUB", 4, 3, &stats) && ok;
    ok = drawText(&font, framebuffer, "INVENTORY VIEW", 4, 16, &stats) && ok;

    snprintf(line, sizeof(line), "%cWPN %02u OWN %03X",
             hub.selectedRow == 0U ? '>' : ' ',
             (unsigned int)before.weapon,
             (unsigned int)(before.weapons & 0x0fffU));
    ok = drawText(&font, framebuffer, line, 4, 29, &stats) && ok;

    snprintf(line, sizeof(line), "%cA %02u %02u %02u %02u %02u %02u",
             hub.selectedRow == 1U ? '>' : ' ',
             (unsigned int)before.ammo[0],
             (unsigned int)before.ammo[1],
             (unsigned int)before.ammo[2],
             (unsigned int)before.ammo[3],
             (unsigned int)before.ammo[4],
             (unsigned int)before.ammo[5]);
    ok = drawText(&font, framebuffer, line, 4, 42, &stats) && ok;

    snprintf(line, sizeof(line), "%cI %02u %02u %02u %02u %02u",
             hub.selectedRow == 2U ? '>' : ' ',
             (unsigned int)before.inventory[0],
             (unsigned int)before.inventory[1],
             (unsigned int)before.inventory[2],
             (unsigned int)before.inventory[3],
             (unsigned int)before.inventory[4]);
    ok = drawText(&font, framebuffer, line, 4, 55, &stats) && ok;

    snprintf(line, sizeof(line), "KEYS %08lX",
             (unsigned long)before.keys);
    ok = drawText(&font, framebuffer, line, 4, 68, &stats) && ok;

    snprintf(line, sizeof(line), "CRED %lu",
             (unsigned long)before.credits);
    ok = drawText(&font, framebuffer, line, 4, 81, &stats) && ok;

    snprintf(line, sizeof(line), "LV %u XP %lu",
             (unsigned int)before.level,
             (unsigned long)before.currentXP);
    ok = drawText(&font, framebuffer, line, 4, 94, &stats) && ok;
    ok = drawText(&font, framebuffer, "MENU BACK  UP/DN CUR", 4, 107, &stats) && ok;

    EspAssetPack_close();
    if (!ok || EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    fnvAfter = EspNativeGameplayPlayerState_fingerprint();
    if (!EspNativeGameplayPlayerState_snapshot(&after) ||
        fnvAfter != fnvBefore || memcmp(&before, &after, sizeof(before)) != 0) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    paintedFNV = frameFNV();
    if (paintedFNV == 0U || !Esp32PlatformVideo_present()) {
        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    }

    ++hub.paints;
    hub.lastPlayerFNV = fnvAfter;
    hub.lastFrameFNV = paintedFNV;
    printf("[HUB] FRAME paint=%u page=inventory row=%u frame=%08x reads=%u bytes=%u playerFNV=%08x exact=yes packClosed=yes presented=1 mutation=no turn=no\n",
           (unsigned int)hub.paints,
           (unsigned int)hub.selectedRow,
           (unsigned int)paintedFNV,
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead,
           (unsigned int)fnvAfter);
    return ESP_NATIVE_GAMEPLAY_HUB_OK;
}

void EspNativeGameplayHub_reset(void) {
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
    uint32_t playerFNV;

    memset(&player, 0, sizeof(player));
    if (hub.active != 0U) return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
    if (EspAssetPack_isOpen()) return ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY;
    if (!EspNativeGameplayPlayerState_snapshot(&player) || player.active != 1U) {
        return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }
    playerFNV = EspNativeGameplayPlayerState_fingerprint();
    if (playerFNV == 0U) return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;

    ++hub.opens;
    hub.selectedRow = 0U;
    hub.page = 0U;
    hub.playerFNVAtOpen = playerFNV;
    hub.lastPlayerFNV = playerFNV;
    hub.active = 1U;

    status = paintInventory();
    if (status != ESP_NATIVE_GAMEPLAY_HUB_OK) {
        hub.active = 0U;
        return status;
    }

    printf("[HUB] OPEN n=%u mode=inventory-readonly ownerBytes=%u playerStateBytes=%u playerFNV=%08x weapon=%u weapons=%03x ammo=%02u/%02u/%02u/%02u/%02u/%02u items=%02u/%02u/%02u/%02u/%02u keys=%08lx credits=%lu mutation=no turn=no packClosed=yes\n",
           (unsigned int)hub.opens,
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
    uint32_t playerFNV;
    uint8_t beforeRow;

    if (hub.active == 0U) return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {
        playerFNV = EspNativeGameplayPlayerState_fingerprint();
        hub.active = 0U;
        ++hub.closes;
        hub.lastPlayerFNV = playerFNV;
        printf("[HUB] CLOSE n=%u playerFNV=%08x->%08x exact=%s mutation=no turn=no worldRedraw=pending packClosed=%s\n",
               (unsigned int)hub.closes,
               (unsigned int)hub.playerFNVAtOpen,
               (unsigned int)playerFNV,
               playerFNV == hub.playerFNVAtOpen ? "yes" : "NO",
               EspAssetPack_isOpen() ? "NO" : "yes");
        return (playerFNV != 0U && playerFNV == hub.playerFNVAtOpen &&
                !EspAssetPack_isOpen())
                   ? ESP_NATIVE_GAMEPLAY_HUB_CLOSED
                   : ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
    }

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT) {
        printf("[HUB] SELECT-DEFER row=%u cause=read-only-milestone mutation=no turn=no\n",
               (unsigned int)hub.selectedRow);
        return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
    }

    beforeRow = hub.selectedRow;
    if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD) {
        hub.selectedRow = (uint8_t)((hub.selectedRow + HUB_ROWS - 1U) % HUB_ROWS);
    }
    else if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK) {
        hub.selectedRow = (uint8_t)((hub.selectedRow + 1U) % HUB_ROWS);
    }
    else {
        printf("[HUB] IGNORE action=%u row=%u worldDispatch=blocked mutation=no turn=no\n",
               (unsigned int)action,
               (unsigned int)hub.selectedRow);
        return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
    }

    status = paintInventory();
    if (status != ESP_NATIVE_GAMEPLAY_HUB_OK) return status;
    printf("[HUB] CURSOR row=%u->%u direction=%s mutation=no turn=no\n",
           (unsigned int)beforeRow,
           (unsigned int)hub.selectedRow,
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ? "up" : "down");
    return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
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
