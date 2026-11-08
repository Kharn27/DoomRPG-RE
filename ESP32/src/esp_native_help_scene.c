#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "HELP viewport thumbnail requires a 160x120 logical CYD framebuffer"
#endif

#define PREVIEW_X 16
#define PREVIEW_Y 42
#define PREVIEW_W 128
#define PREVIEW_H 53
#define WALL_BYTES 2048U
#define WALL_PALETTE 16U
#define MAPPING_HEADER 16U
#define PALETTE_HEADER 4U
#define WALL_HEADER 4U
#define MAX_CANDIDATES 24U

/* The same 64x64 4bpp indexed wall material as the actual native gameplay
 * renderer, but only ONE resource. The screen and PAK are not retained. */
typedef struct HelpMaterial_s {
    uint8_t packed[WALL_BYTES];
    uint16_t palette[WALL_PALETTE];
    uint16_t logicalId;
    uint8_t ready;
} HelpMaterial;
static HelpMaterial material;

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t sourceBgrToFramebuffer565(uint16_t c) {
    return (uint16_t)(((c & 31U) << 11) | (c & 0x07e0U) |
                      ((c >> 11) & 31U));
}
static uint16_t sampleWall(int u, int v) {
    /* 64 rows per source column: matches esp_native_first_frame.c
     * packedIndex=position>>13, nibbleShift=(position>>10)&4. */
    const unsigned int index = (unsigned int)((u & 63) * 64 + (v & 63));
    const uint8_t raw = material.packed[index >> 1];
    return material.palette[(index & 1U) ? (raw >> 4) : (raw & 15U)];
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

void EspNativeHelpScene_reset(void) {
    memset(&material, 0, sizeof(material));
}
int EspNativeHelpScene_isReady(void) {
    return material.ready == 1U;
}
uint16_t EspNativeHelpScene_textureId(void) {
    return EspNativeHelpScene_isReady() ? material.logicalId : 0U;
}

int EspNativeHelpScene_load(uint8_t cachedMapId, uint32_t seed) {
    EspAssetPackEntry mappings, palettes, walls;
    uint8_t mh[16], ph[4], wh[4], id[2], pair[8], paletteBytesRaw[32];
    uint32_t texelPairs, shapes, logicalCount, spriteCount;
    uint32_t paletteBytes, idBase, cap, attempt;
    uint64_t expectedSize;
    int ok = 0;

    EspNativeHelpScene_reset();
    if (EspAssetPack_isOpen() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;

    if (!EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("wtexels.bin", &walls) ||
        mappings.size < MAPPING_HEADER ||
        palettes.size < PALETTE_HEADER + WALL_PALETTE * 2U ||
        walls.size < WALL_HEADER + WALL_BYTES ||
        !EspAssetPack_readRange(&mappings, 0U, mh, sizeof(mh)) ||
        !EspAssetPack_readRange(&palettes, 0U, ph, sizeof(ph)) ||
        !EspAssetPack_readRange(&walls, 0U, wh, sizeof(wh)))
        goto done;

    texelPairs = le32(mh);
    shapes = le32(mh + 4U);
    logicalCount = le32(mh + 8U);
    spriteCount = le32(mh + 12U);
    paletteBytes = le32(ph);
    expectedSize = MAPPING_HEADER +
        ((uint64_t)texelPairs + (uint64_t)shapes) * 8U +
        ((uint64_t)logicalCount + (uint64_t)spriteCount) * 2U;
    if (texelPairs == 0U || texelPairs > 4096U ||
        shapes == 0U || shapes > 4096U ||
        logicalCount < 2U || logicalCount > 4096U ||
        spriteCount > 4096U ||
        expectedSize != mappings.size ||
        paletteBytes < WALL_PALETTE * 2U || (paletteBytes & 1U) ||
        paletteBytes != palettes.size - PALETTE_HEADER ||
        le32(wh) != walls.size - WALL_HEADER)
        goto done;

    idBase = MAPPING_HEADER + (texelPairs + shapes) * 8U;
    cap = logicalCount > 256U ? 256U : logicalCount;
    for (attempt = 0U; attempt < MAX_CANDIDATES; ++attempt) {
        uint32_t actual, source, paletteOffset, i;
        int distinct = 0;
        uint8_t used[16] = {0U};
        /* The cached flash map id changes the material family. A small
         * seed changes the choice across HELP openings without affecting
         * the shared gameplay RNG. Only IDs from the native PAK are used. */
        const uint32_t start = cachedMapId != 0U
            ? ((uint32_t)cachedMapId * 57U + (seed / 13000U) % 7U)
            : (seed / 137U);
        const uint32_t logical = 1U +
            ((start + attempt * 37U) % (cap - 1U));

        if (!EspAssetPack_readRange(&mappings, idBase + logical * 2U,
                                    id, sizeof(id))) goto done;
        actual = le16(id);
        if (actual >= texelPairs ||
            !EspAssetPack_readRange(&mappings,
                MAPPING_HEADER + actual * 8U, pair, sizeof(pair)))
            continue;

        source = le32(pair); /* original nibble address */
        paletteOffset = le32(pair + 4U);
        if ((int32_t)source < 0 || (source & 1U) ||
            (int32_t)paletteOffset < 0 ||
            paletteOffset > paletteBytes / 2U ||
            WALL_PALETTE > paletteBytes / 2U - paletteOffset ||
            source / 2U > walls.size - WALL_HEADER ||
            WALL_BYTES > walls.size - WALL_HEADER - source / 2U)
            continue;

        if (!EspAssetPack_readRange(&palettes,
                PALETTE_HEADER + paletteOffset * 2U,
                paletteBytesRaw, sizeof(paletteBytesRaw)) ||
            !EspAssetPack_readRange(&walls,
                WALL_HEADER + source / 2U, material.packed, WALL_BYTES))
            goto done;

        /* Avoid invisible or featureless textures in the tutorial. */
        for (i = 0U; i < WALL_BYTES; i += 31U) {
            used[material.packed[i] & 15U] = 1U;
            used[material.packed[i] >> 4] = 1U;
        }
        for (i = 0U; i < 16U; ++i) distinct += used[i] != 0U;
        if (distinct < 3) continue;
        for (i = 0U; i < WALL_PALETTE; ++i) {
            material.palette[i] =
                sourceBgrToFramebuffer565(le16(&paletteBytesRaw[i * 2U]));
        }
        material.logicalId = (uint16_t)logical;
        material.ready = 1U;
        ok = 1;
        break;
    }

done:
    EspAssetPack_close();
    if (!ok) EspNativeHelpScene_reset();
    return ok;
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
 * stay within a 43x18 miniature touch cell and retain the live green pulse. */
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

int EspNativeHelpScene_paint(uint16_t* fb, uint8_t level) {
    static const int xZone[4] = {0, 42, 85, PREVIEW_W};
    static const int yZone[4] = {0, 17, 35, PREVIEW_H};
    int lx, ly, row, col;
    if (fb == NULL || !material.ready || level > 6U) return 0;

    /* A small deterministic perspective illustration with an actual native
     * wall texture; NOT a screenshot and no map/BSP world state. */
    for (lx = 0; lx < PREVIEW_W; ++lx) {
        const int flank = lx < 48 ? lx :
                          lx >= 80 ? (127 - lx) : 48;
        const int wallTop = 15 * flank / 48;
        const int wallBottom = 52 - 13 * flank / 48;
        for (ly = 0; ly < PREVIEW_H; ++ly) {
            uint16_t ink;
            if (ly < wallTop) {
                ink = (uint16_t)(0x0842U + ((uint16_t)ly << 5));
            }
            else if (ly > wallBottom) {
                ink = (uint16_t)(0x1042U + ((uint16_t)(52 - ly) << 5));
            }
            else {
                int u = lx < 48 ? lx * 2 : lx >= 80 ? (127 - lx) * 2 :
                        (lx - 48) * 2;
                int v = (ly - wallTop) * 63 / (wallBottom - wallTop + 1);
                const unsigned int light = (unsigned int)(155 + flank);
                ink = dim565(sampleWall(u, v), light);
            }
            fb[(PREVIEW_Y + ly) * DOOMRPG_LOGICAL_WIDTH +
               PREVIEW_X + lx] = ink;
        }
    }

    /* Exact grid proportions from gameplay input 0..52/53..105/106..159
     * and y20..45/46..72/73..99, reduced into one x128/y53 thumbnail.
     * Every region has the same semantic feedback color as gameplay. */
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            const uint16_t neon = row == 1 && col == 1 ? 0xf800U :
                                  row == 2 && col != 1 ? 0xffe0U :
                                  0x07e0U;
            const uint16_t core = dim565(neon, 85U + (uint32_t)level * 27U);
            const uint16_t halo = dim565(neon, 26U + (uint32_t)level * 9U);
            const int left = PREVIEW_X + xZone[col];
            const int right = PREVIEW_X + xZone[col + 1] - 1;
            const int top = PREVIEW_Y + yZone[row];
            const int bottom = PREVIEW_Y + yZone[row + 1] - 1;
            int x, y;
            for (x = left + 1; x < right; ++x) {
                putGlow(fb, x, top, core);
                putGlow(fb, x, bottom, core);
                putGlow(fb, x, top + 1, halo);
                putGlow(fb, x, bottom - 1, halo);
            }
            for (y = top + 1; y < bottom; ++y) {
                putGlow(fb, left, y, core);
                putGlow(fb, right, y, core);
                putGlow(fb, left + 1, y, halo);
                putGlow(fb, right - 1, y, halo);
            }
            if (row == 2 && col != 1) {
                /* WPN/PREV and WPN/NEXT replace the ambiguous yellow arrows.
                 * Two 7px lines plus a 1px gap fit the 18px-high cells. */
                const uint16_t textInk = dim565(
                    neon, 170U + (uint32_t)level * 13U);
                helpPaintWeaponCaption(fb, (left + right) / 2, top,
                                       col == 0 ? "PREV" : "NEXT",
                                       textInk);
            }
            else {
                drawActionMark(fb, row, col,
                               (left + right) / 2,
                               (top + bottom) / 2, core);
            }
        }
    }
    return 1;
}
