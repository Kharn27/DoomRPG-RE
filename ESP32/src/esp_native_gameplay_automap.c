#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_line_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_automap.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define AUTOMAP_MAP_SIZE 32
#define AUTOMAP_SCALE 3
#define AUTOMAP_PIXEL_SIZE (AUTOMAP_MAP_SIZE * AUTOMAP_SCALE)
#define AUTOMAP_X ((DOOMRPG_LOGICAL_WIDTH - AUTOMAP_PIXEL_SIZE) / 2)
#define AUTOMAP_Y ((DOOMRPG_LOGICAL_HEIGHT - AUTOMAP_PIXEL_SIZE) / 2)

#define AUTOMAP_BLACK 0x0000U
#define AUTOMAP_VISITED 0x6000U
#define AUTOMAP_ENTRANCE 0xfd40U
#define AUTOMAP_ITEM 0x35c0U
#define AUTOMAP_LINE 0xc800U
#define AUTOMAP_DOOR 0xccc0U
#define AUTOMAP_SPECIAL 0xc800U
#define AUTOMAP_SPECIAL_DIM 0x8800U
#define AUTOMAP_PLAYER 0xffffU

#define AUTOMAP_LINE_FLAG_REGULAR_DOOR 0x00000004UL
#define AUTOMAP_LINE_FLAG_Y_NUDGE 0x00000200UL
#define AUTOMAP_DOOR_FULL_DISPLACEMENT \
    (ESP_NATIVE_DOOR_ANIMATION_FRAMES * ESP_NATIVE_DOOR_ANIMATION_STEP)

#define AUTOMAP_SPECIAL_REVERSE 0x00040000UL
#define AUTOMAP_SPECIAL_HORIZONTAL_1 0x00080000UL
#define AUTOMAP_SPECIAL_HORIZONTAL_2 0x00100000UL
#define AUTOMAP_SPECIAL_VERTICAL_1 0x00400000UL

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native Automap geometry is defined for 160x120 logical output"
#endif

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static void putPixel(uint16_t* framebuffer,
                     int x,
                     int y,
                     uint16_t color,
                     uint16_t* pixels) {
    if (framebuffer == NULL || x < 0 || y < 0 ||
        x >= DOOMRPG_LOGICAL_WIDTH || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
    if (pixels != NULL && *pixels != UINT16_MAX) ++*pixels;
}

static void fillRect(uint16_t* framebuffer,
                     int x,
                     int y,
                     int w,
                     int h,
                     uint16_t color,
                     uint16_t* pixels) {
    int xx;
    int yy;
    for (yy = 0; yy < h; ++yy) {
        for (xx = 0; xx < w; ++xx) {
            putPixel(framebuffer, x + xx, y + yy, color, pixels);
        }
    }
}

static void drawLine(uint16_t* framebuffer,
                     int x0,
                     int y0,
                     int x1,
                     int y1,
                     uint16_t color,
                     uint16_t* pixels) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dyAbs = y1 > y0 ? y1 - y0 : y0 - y1;
    int dy = -dyAbs;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        putPixel(framebuffer, x0, y0, color, pixels);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err << 1;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static int mapCoord(int32_t world) {
    if (world < 0) return -1;
    return (int)((world * AUTOMAP_SCALE + 32) >> 6);
}

static void drawPlayer(uint16_t* framebuffer,
                       int x,
                       int y,
                       uint8_t angle,
                       uint16_t* pixels) {
    int dx = 0;
    int dy = 0;
    int lx;
    int ly;
    int tipX;
    int tipY;
    int leftX;
    int leftY;
    int rightX;
    int rightY;

    switch (angle & 0xc0U) {
    case 0U:   dx = 1; break;
    case 64U:  dy = -1; break;
    case 128U: dx = -1; break;
    default:   dy = 1; break;
    }
    lx = -dy;
    ly = dx;

    /*
     * Compact 3x3 chevron. The first candidate used a roughly 6-pixel cursor,
     * matching the legacy bitmap but visually oversized against a 3x3 tile on
     * the 96x96 CYD automap. Keep direction readable without covering adjacent
     * cells.
     */
    tipX = x + dx;
    tipY = y + dy;
    leftX = x - dx + lx;
    leftY = y - dy + ly;
    rightX = x - dx - lx;
    rightY = y - dy - ly;
    drawLine(framebuffer, leftX, leftY, tipX, tipY,
             AUTOMAP_PLAYER, pixels);
    drawLine(framebuffer, rightX, rightY, tipX, tipY,
             AUTOMAP_PLAYER, pixels);
    putPixel(framebuffer, x, y, AUTOMAP_PLAYER, pixels);
}

static int adjustRegularDoorLine(uint32_t lineIndex, EspMapLine* line) {
    int16_t displacement = 0;
    uint8_t open = 0U;
    int32_t delta;

    if (line == NULL ||
        (line->flags & AUTOMAP_LINE_FLAG_REGULAR_DOOR) == 0U) {
        return 1;
    }

    if (EspNativeDoorAnimator_getLineDisplacement(
            lineIndex, &displacement)) {
        delta = displacement;
    }
    else {
        if (!EspMapLineState_getOpen(lineIndex, &open)) return 0;
        delta = open != 0U ? (int32_t)AUTOMAP_DOOR_FULL_DISPLACEMENT : 0;
    }

    if (delta == 0) return 1;
    if ((line->flags & AUTOMAP_LINE_FLAG_Y_NUDGE) != 0U) {
        if ((uint32_t)line->y1 + (uint32_t)delta > UINT16_MAX) return 0;
        line->y1 = (uint16_t)((uint32_t)line->y1 + (uint32_t)delta);
    }
    else {
        if ((uint32_t)line->x1 + (uint32_t)delta > UINT16_MAX) return 0;
        line->x1 = (uint16_t)((uint32_t)line->x1 + (uint32_t)delta);
    }
    return 1;
}

int EspNativeGameplayAutomap_uncoverAt(int32_t worldX,
                                       int32_t worldY,
                                       uint16_t* outMutated) {
    int dx;
    int dy;
    int x;
    int y;
    uint16_t mutated = 0U;
    uint8_t centerFlags;

    if (outMutated != NULL) *outMutated = 0U;
    if (!EspMapState_isReady() || worldX < 0 || worldY < 0) return 0;
    dx = worldX >> 6;
    dy = worldY >> 6;
    if (dx < 0 || dx >= 32 || dy < 0 || dy >= 32) return 0;

    if (!EspMapState_getTileFlags((uint32_t)(dy * 32 + dx), &centerFlags)) {
        return 0;
    }
    if ((centerFlags & ESP_MAP_TILE_VISITED) != 0U) return 1;

    for (y = dy - 1; y <= dy + 1; ++y) {
        if (y < 0 || y >= 31) continue;
        for (x = dx - 1; x <= dx + 1; ++x) {
            uint32_t tile;
            uint8_t flags;
            if (x < 0 || x >= 31) continue;
            tile = (uint32_t)(y * 32 + x);
            if (!EspMapState_getTileFlags(tile, &flags)) return 0;
            if ((flags & ESP_MAP_TILE_SECRET) != 0U &&
                (x != dx || y != dy)) {
                continue;
            }
            if ((flags & ESP_MAP_TILE_VISITED) == 0U) {
                if (!EspMapState_setVisited(tile, 1U)) return 0;
                if (mutated != UINT16_MAX) ++mutated;
            }
        }
    }

    if (outMutated != NULL) *outMutated = mutated;
    return 1;
}

int EspNativeGameplayAutomap_render(int32_t worldX,
                                    int32_t worldY,
                                    uint8_t angle,
                                    EspNativeGameplayAutomapStats* outStats) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();
    uint16_t* framebuffer;
    size_t framebufferBytes;
    EspNativeGameplayAutomapStats stats;
    uint32_t i;

    memset(&stats, 0, sizeof(stats));
    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (outStats == NULL || runtime == NULL || topology == NULL ||
        automap == NULL || !EspMapState_isReady() ||
        runtime->mapSpriteCount != topology->spriteCount ||
        runtime->mapSpriteCount != automap->spriteCount ||
        runtime->lineCount != automap->lineCount ||
        worldX < 0 || worldY < 0) {
        return 0;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes !=
            (size_t)DOOMRPG_LOGICAL_WIDTH *
            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0;
    }

    fillRect(framebuffer, 0, 0,
             DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
             AUTOMAP_BLACK, &stats.pixelsWritten);

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        uint8_t flags;
        int tx;
        int ty;
        if (!EspMapState_getTileFlags(i, &flags)) return 0;
        if ((flags & ESP_MAP_TILE_VISITED) == 0U ||
            (flags & ESP_MAP_TILE_WALL) != 0U) {
            continue;
        }
        tx = (int)(i & 31U);
        ty = (int)(i >> 5);
        fillRect(framebuffer,
                 AUTOMAP_X + tx * AUTOMAP_SCALE,
                 AUTOMAP_Y + ty * AUTOMAP_SCALE,
                 AUTOMAP_SCALE, AUTOMAP_SCALE,
                 (flags & ESP_MAP_TILE_ENTRANCE) != 0U
                     ? AUTOMAP_ENTRANCE
                     : AUTOMAP_VISITED,
                 &stats.pixelsWritten);
        if (stats.visitedCells != UINT16_MAX) ++stats.visitedCells;
    }

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t tile;
        uint8_t flags;
        (void)subtype;
        (void)linkOrder;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            return 0;
        }
        if (type != 2U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U) {
            continue;
        }
        tile = linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK;
        if (!EspMapState_getTileFlags(tile, &flags)) return 0;
        if ((flags & ESP_MAP_TILE_VISITED) == 0U ||
            (flags & ESP_MAP_TILE_WALL) != 0U) {
            continue;
        }
        fillRect(framebuffer,
                 AUTOMAP_X + (int)(tile & 31U) * AUTOMAP_SCALE +
                     (AUTOMAP_SCALE / 2),
                 AUTOMAP_Y + (int)(tile >> 5) * AUTOMAP_SCALE +
                     (AUTOMAP_SCALE / 2),
                 1, 1, AUTOMAP_ITEM, &stats.pixelsWritten);
        if (stats.visibleItems != UINT16_MAX) ++stats.visibleItems;
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        EspMapSprite sprite;
        uint8_t revealed;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int x0;
        int y0;
        int x1;
        int y1;
        uint16_t color;

        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed)) return 0;
        if (revealed == 0U) continue;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            return 0;
        }
        (void)subtype;
        (void)linkState;
        (void)linkOrder;
        if (type != 14U && type != 15U) continue;
        if (!EspMapRuntime_getMapSprite(i, &sprite)) return 0;

        color = (sprite.info & AUTOMAP_SPECIAL_REVERSE) != 0U
                    ? AUTOMAP_SPECIAL
                    : AUTOMAP_SPECIAL_DIM;
        if ((sprite.info & (AUTOMAP_SPECIAL_HORIZONTAL_1 |
                            AUTOMAP_SPECIAL_HORIZONTAL_2)) != 0U) {
            x0 = AUTOMAP_X + mapCoord((int32_t)sprite.x - 32);
            x1 = AUTOMAP_X + mapCoord((int32_t)sprite.x + 32);
            y0 = y1 = AUTOMAP_Y + mapCoord(sprite.y);
        }
        else {
            x0 = x1 = AUTOMAP_X + mapCoord(sprite.x);
            y0 = AUTOMAP_Y + mapCoord((int32_t)sprite.y - 32);
            y1 = AUTOMAP_Y + mapCoord((int32_t)sprite.y + 32);
        }
        drawLine(framebuffer, x0, y0, x1, y1, color,
                 &stats.pixelsWritten);
        if (stats.revealedSpecials != UINT16_MAX) ++stats.revealedSpecials;
    }

    for (i = 0U; i < runtime->lineCount; ++i) {
        EspMapLine line;
        uint8_t revealed;
        int x0;
        int y0;
        int x1;
        int y1;
        if (!EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;
        if (revealed == 0U || !EspMapRuntime_getLine(i, &line)) continue;
        if (!adjustRegularDoorLine(i, &line)) return 0;

        x0 = AUTOMAP_X + mapCoord(line.x1);
        y0 = AUTOMAP_Y + mapCoord(line.y1);
        x1 = AUTOMAP_X + mapCoord(line.x2);
        y1 = AUTOMAP_Y + mapCoord(line.y2);
        drawLine(framebuffer, x0, y0, x1, y1,
                 (line.flags & 0x00000004UL) != 0U
                     ? AUTOMAP_DOOR : AUTOMAP_LINE,
                 &stats.pixelsWritten);
        if (stats.revealedLines != UINT16_MAX) ++stats.revealedLines;
    }

    stats.playerX = (uint8_t)(AUTOMAP_X +
        mapCoord(worldX - 32) + (AUTOMAP_SCALE / 2));
    stats.playerY = (uint8_t)(AUTOMAP_Y +
        mapCoord(worldY - 32) + (AUTOMAP_SCALE / 2));
    stats.angle = angle;
    drawPlayer(framebuffer, stats.playerX, stats.playerY, angle,
               &stats.pixelsWritten);

    stats.scale = AUTOMAP_SCALE;
    stats.mapX = AUTOMAP_X;
    stats.mapY = AUTOMAP_Y;
    stats.frameFNV1a =
        fnv1a32((const uint8_t*)framebuffer, (uint32_t)framebufferBytes);
    if (!Esp32PlatformVideo_present()) return 0;
    stats.presented = 1U;
    *outStats = stats;

    printf("[AUTOMAP] FRAME map=%ux%u@%u,%u scale=%u visited=%u lines=%u specials=%u items=%u player=%u,%u angle=%u pixels=%u frame=%08x presented=1\n",
           (unsigned int)AUTOMAP_PIXEL_SIZE,
           (unsigned int)AUTOMAP_PIXEL_SIZE,
           (unsigned int)stats.mapX,
           (unsigned int)stats.mapY,
           (unsigned int)stats.scale,
           (unsigned int)stats.visitedCells,
           (unsigned int)stats.revealedLines,
           (unsigned int)stats.revealedSpecials,
           (unsigned int)stats.visibleItems,
           (unsigned int)stats.playerX,
           (unsigned int)stats.playerY,
           (unsigned int)stats.angle,
           (unsigned int)stats.pixelsWritten,
           (unsigned int)stats.frameFNV1a);
    return 1;
}
