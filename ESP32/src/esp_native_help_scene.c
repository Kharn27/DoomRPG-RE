#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native HELP illustration requires classic CYD 160x120"
#endif

#define PREVIEW_X 8
#define PREVIEW_Y 29
#define PREVIEW_W 144
#define PREVIEW_H 68

#define HELP_TEXTURE_BYTES 2048U
#define HELP_TEXTURE_COLORS 16U
#define HELP_MATERIAL_COUNT 3U
#define HELP_MAP_CELLS 1024U
#define HELP_MAP_BYTES 256U
#define HELP_MAPPING_HEADER 16U
#define HELP_MAPPINGS_PAIR 8U

/* One PAK-first, world-free, bounded illustration owner. Geometric structure
 * comes from the real 32x32 BSP blockmap and authentic spawn point. Three
 * wall materials come from that map's required resource IDs, excluding known
 * door indicator families. Unlike the retired wallpaper, this raster casts
 * spatial rays through the genuine map. It never loads an EspMapRuntime,
 * allocates a world, mutates the map, advances a turn or touches the zip. */
typedef struct HelpWallMaterial_s {
    uint8_t packed[HELP_TEXTURE_BYTES];
    uint16_t palette[HELP_TEXTURE_COLORS];
    uint16_t logicalId;
} HelpWallMaterial;

static struct {
    HelpWallMaterial walls[HELP_MATERIAL_COUNT];
    uint8_t blockMap[HELP_MAP_BYTES];
    uint16_t spawnTile;
    uint16_t ceilingRgb565;
    uint16_t floorRgb565;
    uint8_t spawnDirection;
    uint8_t materialCount;
    uint8_t mapId;
    uint8_t active;
    uint8_t ready;
} scene;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t sourceBgrTo565(uint16_t c) {
    return (uint16_t)(((c & 31U) << 11) |
                      (c & 0x07e0U) | ((c >> 11) & 31U));
}
static uint16_t rgb565(const uint8_t rgb[3]) {
    return (uint16_t)(((uint16_t)(rgb[0] & 0xf8U) << 8) |
                      ((uint16_t)(rgb[1] & 0xfcU) << 3) |
                      (uint16_t)(rgb[2] >> 3));
}
static uint16_t dim565(uint16_t c, unsigned int scale) {
    const unsigned int r = ((c >> 11) & 31U) * scale / 256U;
    const unsigned int g = ((c >> 5) & 63U) * scale / 256U;
    const unsigned int b = (c & 31U) * scale / 256U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
static uint16_t add565(uint16_t base, uint16_t gain) {
    unsigned int r = ((base >> 11) & 31U) + ((gain >> 11) & 31U);
    unsigned int g = ((base >> 5) & 63U) + ((gain >> 5) & 63U);
    unsigned int b = (base & 31U) + (gain & 31U);
    if (r > 31U) r = 31U;
    if (g > 63U) g = 63U;
    if (b > 31U) b = 31U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
static uint16_t sampleMaterial(const HelpWallMaterial* wall,
                                int u, int v) {
    const uint32_t at = (uint32_t)((u & 63) * 64 + (v & 63));
    const uint8_t packed = wall->packed[at >> 1];
    return wall->palette[(at & 1U) ? (packed >> 4) : (packed & 15U)];
}
static int helpTileWall(int x, int y) {
    uint32_t at;
    if (x < 0 || y < 0 || x >= 32 || y >= 32) return 1;
    at = (uint32_t)y * 32U + (uint32_t)x;
    return (scene.blockMap[at >> 2] >> ((at & 3U) * 2U)) & 1U;
}
static int isDoorIndicator(uint16_t id) {
    /* Map entrance, animated door and indicator texture families are NOT
     * suitable for the continuous brick/concrete wall material. */
    return id == 7U || id == 8U || id == 9U || id == 10U ||
           id == 15U || id == 16U;
}
static int loadMaterial(const EspAssetPackEntry* mappings,
                        const EspAssetPackEntry* palettes,
                        const EspAssetPackEntry* textures,
                        uint32_t pairCount, uint32_t paletteSize,
                        uint32_t idBase, uint16_t logicalId,
                        HelpWallMaterial* wall) {
    uint8_t id[2], pair[8], colors[32];
    uint8_t used[16] = {0U};
    uint32_t actual, src, palette, i, distinct = 0U;
    if (wall == NULL ||
        !EspAssetPack_readRange(mappings, idBase + (uint32_t)logicalId * 2U,
                                id, sizeof(id))) return 0;
    actual = readLe16(id);
    if (actual >= pairCount ||
        !EspAssetPack_readRange(mappings,
               HELP_MAPPING_HEADER + actual * HELP_MAPPINGS_PAIR,
               pair, sizeof(pair))) return 0;
    src = readLe32(pair);
    palette = readLe32(pair + 4U);
    if ((int32_t)src < 0 || (src & 1U) != 0U ||
        (int32_t)palette < 0 || palette > paletteSize / 2U ||
        HELP_TEXTURE_COLORS > paletteSize / 2U - palette ||
        src / 2U > textures->size - 4U ||
        HELP_TEXTURE_BYTES > textures->size - 4U - src / 2U ||
        !EspAssetPack_readRange(
            palettes, 4U + palette * 2U, colors, sizeof(colors)) ||
        !EspAssetPack_readRange(
            textures, 4U + src / 2U, wall->packed, HELP_TEXTURE_BYTES))
        return 0;
    for (i = 0U; i < HELP_TEXTURE_BYTES; i += 31U) {
        used[wall->packed[i] & 15U] = 1U;
        used[wall->packed[i] >> 4] = 1U;
    }
    for (i = 0U; i < 16U; ++i) distinct += used[i] != 0U;
    if (distinct < 4U) return 0;
    for (i = 0U; i < HELP_TEXTURE_COLORS; ++i)
        wall->palette[i] = sourceBgrTo565(readLe16(&colors[i * 2U]));
    wall->logicalId = logicalId;
    return 1;
}
void EspNativeHelpScene_reset(void) {
    /* A HELP close releases only its own tiny immutable lookup; the real
     * gameplay map owner remains completely untouched. */
    memset(&scene, 0, sizeof(scene));
}
int EspNativeHelpScene_begin(uint8_t cachedMapId) {
    const uint8_t mapId = EspMapCatalog_isValidId(cachedMapId)
                              ? cachedMapId : 1U; /* Entrance at cold boot */
    const char* const name = EspMapCatalog_nameForId(mapId);
    EspBspInventory inventory;
    EspAssetPackEntry entry, mappings, palettes, textures;
    uint8_t mh[16], ph[4], wh[4];
    uint32_t pairCount, shapeCount, logicalCount, spriteCount, paletteSize;
    uint32_t idBase, id, pass;
    int ok = 0;

    EspNativeHelpScene_reset();
    scene.active = 1U;
    if (name == NULL || EspAssetPack_isOpen() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    if (!EspBspReader_inventoryPackEntry(name, &inventory) ||
        inventory.spawnIndex >= HELP_MAP_CELLS ||
        inventory.sections.blockMapOffset >= inventory.sourceBytes ||
        !EspAssetPack_findEntry(name, &entry) ||
        !EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("wtexels.bin", &textures) ||
        !EspAssetPack_readRange(
            &entry, inventory.sections.blockMapOffset,
            scene.blockMap, sizeof(scene.blockMap)) ||
        !EspAssetPack_readRange(&mappings, 0U, mh, sizeof(mh)) ||
        !EspAssetPack_readRange(&palettes, 0U, ph, sizeof(ph)) ||
        !EspAssetPack_readRange(&textures, 0U, wh, sizeof(wh)))
        goto done;

    pairCount = readLe32(mh);
    shapeCount = readLe32(mh + 4U);
    logicalCount = readLe32(mh + 8U);
    spriteCount = readLe32(mh + 12U);
    paletteSize = readLe32(ph);
    if (pairCount == 0U || pairCount > 4096U ||
        shapeCount == 0U || shapeCount > 4096U ||
        logicalCount == 0U || logicalCount > 4096U ||
        spriteCount > 4096U ||
        (uint64_t)HELP_MAPPING_HEADER +
            (uint64_t)(pairCount + shapeCount) * 8U +
            (uint64_t)(logicalCount + spriteCount) * 2U != mappings.size ||
        paletteSize < 32U || paletteSize != palettes.size - 4U ||
        readLe32(wh) != textures.size - 4U)
        goto done;
    idBase = HELP_MAPPING_HEADER + (pairCount + shapeCount) * 8U;

    /* Prefer only actual map-required wall materials; if there are fewer
     * than three, the shared renderer can reuse those materials in different
     * spatial views. Never pick a random unrelated door sprite. */
    for (pass = 0U; pass < 2U &&
          scene.materialCount < HELP_MATERIAL_COUNT; ++pass) {
        for (id = 1U; id < logicalCount && id < 256U &&
             scene.materialCount < HELP_MATERIAL_COUNT; ++id) {
            HelpWallMaterial* target;
            uint8_t j, duplicate = 0U;
            if (isDoorIndicator((uint16_t)id)) continue;
            if (pass == 0U &&
                (inventory.lineTextureIdBits[id >> 5] &
                 (1UL << (id & 31U))) == 0U) continue;
            for (j = 0U; j < scene.materialCount; ++j)
                if (scene.walls[j].logicalId == id) duplicate = 1U;
            if (duplicate) continue;
            target = &scene.walls[scene.materialCount];
            if (loadMaterial(&mappings, &palettes, &textures,
                             pairCount, paletteSize, idBase,
                             (uint16_t)id, target))
                ++scene.materialCount;
        }
    }
    if (scene.materialCount == 0U) goto done;

    scene.spawnTile = inventory.spawnIndex;
    scene.spawnDirection = inventory.spawnDirection;
    scene.floorRgb565 = rgb565(inventory.floorRgb);
    scene.ceilingRgb565 = rgb565(inventory.ceilingRgb);
    scene.mapId = mapId;
    scene.ready = 1U;
    ok = 1;
done:
    EspAssetPack_close();
    if (!ok) scene.ready = 0U;
    return ok;
}
int EspNativeHelpScene_isReady(void) {
    return scene.active != 0U && scene.ready != 0U;
}
uint8_t EspNativeHelpScene_previewMapId(void) {
    return EspNativeHelpScene_isReady() ? scene.mapId : 0U;
}
uint8_t EspNativeHelpScene_materialCount(void) {
    return EspNativeHelpScene_isReady() ? scene.materialCount : 0U;
}

/* One mathematically smooth 2.56s breath clock, shared by both HUD guides,
 * main gameplay grid and its three row-by-row previews. */
uint8_t EspNativeHelpScene_breathStep(uint32_t elapsedMs) {
    return (uint8_t)((elapsedMs / ESP_NATIVE_HELP_BREATH_STEP_MS) %
                     ESP_NATIVE_HELP_BREATH_STEPS);
}
uint8_t EspNativeHelpScene_breathLevel(uint8_t step) {
    if (step >= ESP_NATIVE_HELP_BREATH_STEPS) return 0U;
    return step <= 16U ? step : (uint8_t)(32U - step);
}
uint8_t EspNativeHelpScene_breathPower(uint8_t level) {
    if (level > 16U) return 0U;
    return (uint8_t)(((uint32_t)level * level *
        (48U - 2U * level) * 255U + 2048U) / 4096U);
}

/* Bounded ray samples through the genuine 2-bit BSP blockmap; distance and
 * wall texture are map-derived. This is explicitly an instructional mini
 * raycast, not a second game world, live BSP compositor or synthetic corridor.
 * Distinct cameras face different directions from walkable nearby cells. */
static void cameraFor(uint8_t variant, int* px, int* py, int* direction) {
    static const int8_t offsetX[9] = {0, 1, 0, -1, 0, 2, -2, 1, -1};
    static const int8_t offsetY[9] = {0, 0, 1, 0, -1, 0, 0, -1, 1};
    const int sx = scene.spawnTile & 31U;
    const int sy = scene.spawnTile >> 5;
    int i;
    *px = sx * 1024 + 512;
    *py = sy * 1024 + 512;
    *direction = ((int)(scene.spawnDirection + variant * 64U + 32U) >> 6) & 3;
    if (variant == 0U) return;
    for (i = 0; i < 9; ++i) {
        const int k = (i + variant * 2) % 9;
        const int x = sx + offsetX[k], y = sy + offsetY[k];
        if (!helpTileWall(x, y)) {
            *px = x * 1024 + 512;
            *py = y * 1024 + 512;
            return;
        }
    }
}
/* One DDA-style bounded 1/16-tile ray per column, never per pixel.
 * Wall heights and materials are sampled only from the original BSP/PAK.
 * No gameplay render state, no large transient workspace, no randomization. */
static void paintWorld(uint16_t* fb, int top, int bottom,
                       uint8_t variant) {
    static const int8_t forwardX[4] = {1, 0, -1, 0};
    static const int8_t forwardY[4] = {0, -1, 0, 1};
    static const int8_t rightX[4] = {0, 1, 0, -1};
    static const int8_t rightY[4] = {1, 0, -1, 0};
    int cx, cy, dir;
    int x, y;
    const int height = bottom - top + 1;
    cameraFor(variant, &cx, &cy, &dir);
    for (x = 0; x < PREVIEW_W; ++x) {
        int px = cx, py = cy, prevX = cx >> 10, prevY = cy >> 10;
        const int side = (x * 2 - (PREVIEW_W - 1)) * 620 /
                          (PREVIEW_W - 1);
        const int dx = forwardX[dir] * 64 + rightX[dir] * side / 16;
        const int dy = forwardY[dir] * 64 + rightY[dir] * side / 16;
        int tileX = prevX, tileY = prevY, u;
        int step, wallHeight, wallTop, wallBottom, shade;
        const HelpWallMaterial* wall;
        for (step = 1; step <= 160; ++step) {
            px += dx;
            py += dy;
            tileX = px >> 10;
            tileY = py >> 10;
            if (helpTileWall(tileX, tileY)) break;
            prevX = tileX;
            prevY = tileY;
        }
        if (step > 160) step = 160;
        wall = &scene.walls[
            ((unsigned int)((tileX < 0 ? 0 : tileX) * 3 +
                             (tileY < 0 ? 0 : tileY) * 5 + variant)) %
            scene.materialCount];
        u = (tileX != prevX ? py : px) & 1023;
        u >>= 4;
        wallHeight = height * 26 / (step + 20);
        if (wallHeight < 1) wallHeight = 1;
        if (wallHeight > height) wallHeight = height;
        wallTop = (height - wallHeight) / 2;
        wallBottom = wallTop + wallHeight;
        shade = step * 2 > 120 ? 136 : 256 - step * 2;
        for (y = 0; y < height; ++y) {
            uint16_t ink;
            if (y < wallTop)
                ink = scene.ceilingRgb565;
            else if (y >= wallBottom)
                ink = scene.floorRgb565;
            else
                ink = dim565(sampleMaterial(
                     wall, u, (y - wallTop) * 64 / wallHeight),
                     (unsigned int)shade);
            fb[(top + y) * DOOMRPG_LOGICAL_WIDTH + PREVIEW_X + x] =
                ink;
        }
    }
}

static void putGlow(uint16_t* framebuffer, int x, int y,
                    uint16_t color) {
    if (x < PREVIEW_X || x >= PREVIEW_X + PREVIEW_W ||
        y < PREVIEW_Y || y >= PREVIEW_Y + PREVIEW_H) return;
    const uint32_t index = (uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x;
    framebuffer[index] = add565(framebuffer[index], color);
}

static void segment(uint16_t* framebuffer, int x0, int y0, int x1, int y1,
                    uint16_t color) {
    int dx = x0 <= x1 ? x1 - x0 : x0 - x1;
    int sx = x0 <= x1 ? 1 : -1;
    int dy = y0 <= y1 ? y0 - y1 : y1 - y0;
    int sy = y0 <= y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        putGlow(framebuffer, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        {
            const int twice = err * 2;
            if (twice >= dy) { err += dy; x0 += sx; }
            if (twice <= dx) { err += dx; y0 += sy; }
        }
    }
}

/* A rounded return-arrow arc instead of the previous three right-angle
 * segments. These connected 1px strokes are mirrored for TURN L / TURN R,
 * stay within a 48x23 miniature touch cell and retain the live green pulse. */
static void helpPaintTurnArc(uint16_t* fb, int cx, int cy,
                             int col, uint16_t core) {
    static const int8_t turnCurve[][2] = {
        { 5,  4}, { 5,  1}, { 4, -2}, { 2, -4},
        { 0, -5}, {-2, -4}, {-4, -2}, {-5,  0}
    };
    const int mirror = col == 0 ? 1 : -1;
    unsigned int i;
    for (i = 1U; i < sizeof(turnCurve) / sizeof(turnCurve[0]); ++i) {
        segment(fb,
                cx + mirror * turnCurve[i - 1U][0],
                cy + turnCurve[i - 1U][1],
                cx + mirror * turnCurve[i][0],
                cy + turnCurve[i][1], core);
    }
    /* Pointing left on the left-hand button, right on the right-hand one. */
    segment(fb, cx - mirror * 3, cy - 3,
            cx - mirror * 6, cy, core);
    segment(fb, cx - mirror * 6, cy,
            cx - mirror * 3, cy + 3, core);
}

/* Only the eight glyphs used by WPN/PREV/NEXT. 5x7 ROM masks cost 56B,
 * no texture, new font owner, allocation, or offscreen render surface. */
typedef struct HelpWeaponGlyph_s {
    char letter;
    uint8_t rows[7];
} HelpWeaponGlyph;
static const HelpWeaponGlyph helpWeaponGlyphs[] = {
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'E', {0x1f, 0x10, 0x1e, 0x10, 0x10, 0x10, 0x1f}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}},
    {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}}
};

static void helpPaintWeaponWord(uint16_t* fb, int centerX, int y,
                                const char* word, uint16_t ink) {
    const int length = (int)strlen(word);
    const int firstX = centerX - (length * 6 - 1) / 2;
    int i, glyph, r, x;
    for (i = 0; i < length; ++i) {
        for (glyph = 0;
             glyph < (int)(sizeof(helpWeaponGlyphs) /
                           sizeof(helpWeaponGlyphs[0])); ++glyph) {
            if (helpWeaponGlyphs[glyph].letter != word[i]) continue;
            for (r = 0; r < 7; ++r) {
                for (x = 0; x < 5; ++x) {
                    if ((helpWeaponGlyphs[glyph].rows[r] &
                         (uint8_t)(0x10U >> x)) != 0U)
                        putGlow(fb, firstX + i * 6 + x, y + r, ink);
                }
            }
            break;
        }
    }
}

static void helpPaintWeaponCaption(uint16_t* fb, int centerX, int top,
                                   const char* secondLine, uint16_t ink) {
    int x, y;
    /* A small translucent-black reading pad makes the yellow text legible
     * against bright PAK wall textures while keeping the cell frame visible. */
    for (y = top + 1; y <= top + 16; ++y) {
        for (x = centerX - 14; x <= centerX + 13; ++x) {
            uint16_t* pixel = &fb[y * DOOMRPG_LOGICAL_WIDTH + x];
            *pixel = dim565(*pixel, 160U);
        }
    }
    helpPaintWeaponWord(fb, centerX, top + 1, "WPN", ink);
    helpPaintWeaponWord(fb, centerX, top + 9, secondLine, ink);
}

static void drawActionMark(uint16_t* framebuffer, int row, int col,
                           int cx, int cy, uint16_t core) {
    /* Feedback icon vocabulary from esp_native_gameplay_controls.c.
     * MOVE/TURN green, SELECT red, PREV/NEXT WEAPON yellow. */
    if ((row == 0 && col == 1) || (row == 2 && col == 1)) {
        const int d = row == 0 ? -1 : 1;
        segment(framebuffer, cx, cy - 4*d, cx, cy + 3*d, core);
        segment(framebuffer, cx, cy + 4*d, cx - 3, cy, core);
        segment(framebuffer, cx, cy + 4*d, cx + 3, cy, core);
    }
    else if (row == 1 && col == 1) {
        segment(framebuffer, cx - 5, cy, cx - 2, cy, core);
        segment(framebuffer, cx + 2, cy, cx + 5, cy, core);
        segment(framebuffer, cx, cy - 5, cx, cy - 2, core);
        segment(framebuffer, cx, cy + 2, cx, cy + 5, core);
        putGlow(framebuffer, cx, cy, core);
    }
    else if (row == 1) {
        helpPaintTurnArc(framebuffer, cx, cy, col, core);
    }
    else {
        const int d = col == 0 ? -1 : 1;
        segment(framebuffer, cx - d*4, cy,
                cx + d*4, cy, core);
        segment(framebuffer, cx + d*5, cy,
                cx + d, cy - 4, core);
        segment(framebuffer, cx + d*5, cy,
                cx + d, cy + 4, core);
    }
}

/* One easing curve for the 3x3 and all row overlays; labels are always
 * drawn, while neon contours ease gently to zero instead of blinking. */
/* The original game's directional glyphs remain steady. THE breath is
 * ONLY an accent on the imaginary touch-zone dividers, not the icon glyphs,
 * not the floor/walls and never a translucent blue fill. */
static void helpPaintTouchCell(uint16_t* fb, int row, int col,
                               int left, int right, int top, int bottom,
                               uint8_t level) {
    const uint16_t neon = row == 1 && col == 1 ? 0xf800U :
                          row == 2 && col != 1 ? 0xffe0U :
                          0x07e0U;
    const uint16_t labelInk = dim565(neon, 180U);
    (void)level;
    if (row == 2 && col != 1) {
        helpPaintWeaponCaption(fb, (left + right) / 2, top,
                               col == 0 ? "PREV" : "NEXT", labelInk);
    }
    else {
        drawActionMark(fb, row, col, (left + right) / 2,
                       (top + bottom) / 2, labelInk);
    }
}

/* One BLUE pulse, confined to the actual imaginary separators x56/x104,
 * and additionally y51/y74 ONLY on the complete 3x3 demonstration.
 * Geometric extent stays 1 pixel; there is no rectangle fill or flicker. */
static void helpPaintTouchDividers(uint16_t* fb,
                                   int top, int bottom,
                                   int left, int right,
                                   uint8_t level, int fullGrid) {
    const uint8_t power = EspNativeHelpScene_breathPower(level);
    const uint16_t glow = (uint16_t)(((uint32_t)power * 31U) / 255U);
    int x, y;
    if (glow == 0U) return;
    for (y = top; y <= bottom; ++y) {
        putGlow(fb, left + 48, y, glow);
        putGlow(fb, left + 96, y, glow);
    }
    if (fullGrid) {
        for (x = left; x <= right; ++x) {
            putGlow(fb, x, top + 22, glow);
            putGlow(fb, x, top + 45, glow);
        }
    }
}

int EspNativeHelpScene_paint(uint16_t* fb, uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    static const int yZone[4] = {0, 22, 45, PREVIEW_H};
    int row, col;
    if (fb == NULL || !EspNativeHelpScene_isReady() || level > 16U)
        return 0;
    paintWorld(fb, PREVIEW_Y, PREVIEW_Y + PREVIEW_H - 1, 0U);
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            helpPaintTouchCell(fb, row, col,
                               PREVIEW_X + xZone[col],
                               PREVIEW_X + xZone[col + 1] - 1,
                               PREVIEW_Y + yZone[row],
                               PREVIEW_Y + yZone[row + 1] - 1,
                               level);
        }
    }
    helpPaintTouchDividers(fb, PREVIEW_Y, PREVIEW_Y + PREVIEW_H - 1,
                          PREVIEW_X, PREVIEW_X + PREVIEW_W - 1, level, 1);
    return 1;
}

/* Each control row uses a DISTINCT real BSP camera (three directions /
 * nearby walkable spawn cells), not one cropped copy of the main image. */
int EspNativeHelpScene_paintRow(uint16_t* fb, uint8_t sourceRow,
                                uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    const int top = 39, bottom = 61;
    int col;
    if (fb == NULL || !EspNativeHelpScene_isReady() ||
        sourceRow > 2U || level > 16U) return 0;
    paintWorld(fb, top, bottom, (uint8_t)(sourceRow + 1U));
    for (col = 0; col < 3; ++col)
        helpPaintTouchCell(fb, sourceRow, col,
                           PREVIEW_X + xZone[col],
                           PREVIEW_X + xZone[col + 1] - 1,
                           top, bottom, level);
    helpPaintTouchDividers(fb, top, bottom, PREVIEW_X,
                          PREVIEW_X + PREVIEW_W - 1, level, 0);
    return 1;
}
