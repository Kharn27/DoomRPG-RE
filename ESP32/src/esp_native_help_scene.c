#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "HELP viewport thumbnail requires a 160x120 logical CYD framebuffer"
#endif

#define PREVIEW_X 8
#define PREVIEW_Y 29
#define PREVIEW_W 144
#define PREVIEW_H 68
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

static int loadOriginalMaterial(uint8_t cachedMapId, uint32_t seed) {
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

/* The ORIGINAL 2026-11-08 HELP scene, selected directly from whichever
 * native PAK is present. The map flash ID and current HELP epoch keep
 * the visual tied to current contents but do not load an active game.
 * No BSP mini-raycaster/plane arenas or extra framebuffer. */
static uint8_t previewMapId;
int EspNativeHelpScene_begin(uint8_t cachedMapId, uint32_t seed) {
    const int ok = loadOriginalMaterial(cachedMapId, seed);
    previewMapId = ok ? (cachedMapId != 0U ? cachedMapId : 1U) : 0U;
    return ok;
}
uint8_t EspNativeHelpScene_previewMapId(void) {
    return EspNativeHelpScene_isReady() ? previewMapId : 0U;
}
uint8_t EspNativeHelpScene_materialCount(void) {
    return EspNativeHelpScene_isReady() ? 1U : 0U;
}

static void putGlow(uint16_t* framebuffer, int x, int y,
                    uint16_t color) {
    if (x < PREVIEW_X || x >= PREVIEW_X + PREVIEW_W ||
        y < PREVIEW_Y || y >= PREVIEW_Y + PREVIEW_H) return;
    const uint32_t index = (uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x;
    /* Darken ONLY this one-pixel stroke so saturated backgrounds cannot
     * wash green/yellow/red indicators into white; no rectangular tint. */
    framebuffer[index] =
        add565(dim565(framebuffer[index], 116U), color);
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

/* Smooth, directional J-shaped 180-degree turn glyph, mirrored for
 * TURN LEFT/RIGHT. A gently arched ten-segment head is more legible at the
 * 48x23 touch scale than the old square or cramped arc. 2-tone strokes
 * provide a soft green highlight while retaining strict per-cell bounds. */
static void helpPaintTurnArc(uint16_t* fb, int cx, int cy,
                             int col, uint16_t core) {
    static const int8_t turnCurve[][2] = {
        { 6,  5}, { 6,  3}, { 6,  0}, { 5, -3},
        { 3, -5}, { 1, -6}, {-1, -6},
        {-3, -6}, {-5, -4}, {-6, -2}, {-6, 0}
    };
    const int mirror = col == 0 ? 1 : -1;
    const uint16_t glow = dim565(core, 100U);
    unsigned int i;
    for (i = 1U; i < sizeof(turnCurve) / sizeof(turnCurve[0]); ++i) {
        /* One extra soft row below, then a crisp, colored main curve. */
        segment(fb,
                cx + mirror * turnCurve[i - 1U][0],
                cy + turnCurve[i - 1U][1] + 1,
                cx + mirror * turnCurve[i][0],
                cy + turnCurve[i][1] + 1, glow);
        segment(fb,
                cx + mirror * turnCurve[i - 1U][0],
                cy + turnCurve[i - 1U][1],
                cx + mirror * turnCurve[i][0],
                cy + turnCurve[i][1], core);
    }
    /* Arrowhead faces the turn, never a mirrored forward/backward chevron. */
    segment(fb, cx - mirror * 3, cy - 4,
            cx - mirror * 7, cy, core);
    segment(fb, cx - mirror * 7, cy,
            cx - mirror * 3, cy + 4, core);
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

static void helpPaintWeaponCaption(uint16_t* fb, int centerX,
                                   int top, int bottom,
                                   const char* secondLine, uint16_t ink) {
    /* Two 7px glyph rows with one blank scanline: 15px total, centered
     * vertically in the full 23px weapon button (including its frame). */
    const int textTop = top + ((bottom - top + 1) - 15) / 2;
    int x, y;
    /* The stable 16px reading pad follows the centered glyphs. No new
     * buffer, extra draw or change to the neon breathing/text palette. */
    for (y = textTop; y <= textTop + 15; ++y) {
        for (x = centerX - 14; x <= centerX + 13; ++x) {
            uint16_t* pixel = &fb[y * DOOMRPG_LOGICAL_WIDTH + x];
            *pixel = dim565(*pixel, 160U);
        }
    }
    helpPaintWeaponWord(fb, centerX, textTop, "WPN", ink);
    helpPaintWeaponWord(fb, centerX, textTop + 8, secondLine, ink);
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
        /* TURN LEFT/RIGHT symbols sit 2px farther toward their own edges.
         * Shift the connected arc and chevron together, not the hitboxes. */
        helpPaintTurnArc(framebuffer, cx + (col == 0 ? -2 : 2), cy, col, core);
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

/* LIVE vocabulary, identical RGB565 cores to the gameplay feedback:
 * movement/turn green, attack/use red, weapon prev/next yellow.
 * Breath affects ONLY drawn neon strokes and glyphs. Never flood a zone.
 * 32-step smoothstep shares the same phase with all other HELP pages. */
static void helpPaintTouchCell(uint16_t* fb, int row, int col,
                               int left, int right, int top, int bottom,
                               uint8_t level) {
    const uint16_t neon = row == 1 && col == 1 ? 0xf800U :
                          row == 2 && col != 1 ? 0xffe0U :
                          0x07e0U;
    const uint8_t power = EspNativeHelpScene_breathPower(level);
    const uint32_t accent = 72U + ((uint32_t)power * 183U) / 255U;
    const uint16_t core = dim565(neon, accent);
    const uint16_t halo = dim565(neon, accent / 4U);
    int x, y;

    /* One precise pixel at the cell perimeter; 1px inner softer halo.
     * Colors follow the actual finger action, no global blue separators. */
    for (x = left; x <= right; ++x) {
        putGlow(fb, x, top, core);
        putGlow(fb, x, bottom, core);
        if (bottom - top >= 4) {
            putGlow(fb, x, top + 1, halo);
            putGlow(fb, x, bottom - 1, halo);
        }
    }
    for (y = top + 1; y < bottom; ++y) {
        putGlow(fb, left, y, core);
        putGlow(fb, right, y, core);
        putGlow(fb, left + 1, y, halo);
        putGlow(fb, right - 1, y, halo);
    }
    if (row == 2 && col != 1) {
        /* WPN/PREV/NEXT letters breathe yellow; black glyph reading pad
         * is stable so there is no flashing or solid-colour wash. */
        helpPaintWeaponCaption(fb, (left + right) / 2, top, bottom,
                               col == 0 ? "PREV" : "NEXT", core);
    }
    else {
        /* Every turn/move/select symbol now breathes in its ORIGINAL hue. */
        drawActionMark(fb, row, col, (left + right) / 2,
                       (top + bottom) / 2, core);
    }
}

/* Restore the ORIGINAL PAK wall-perspective picture that preceded the
 * BSP/plane raycaster experiments (and their wide flat dead bands).
 * A single 4-bpp wall texture gives the recognisable repeating door-style
 * illustration; wall texels/palette always come from the PRESENT PAK. */
static void helpPaintOriginalBackground(uint16_t* fb) {
    int lx, ly;
    for (lx = 0; lx < PREVIEW_W; ++lx) {
        const int flank = lx < 54 ? lx :
                          lx >= 90 ? (PREVIEW_W - 1 - lx) : 54;
        const int wallTop = 19 * flank / 54;
        const int wallBottom = (PREVIEW_H - 1) - 17 * flank / 54;
        for (ly = 0; ly < PREVIEW_H; ++ly) {
            uint16_t ink;
            if (ly < wallTop)
                ink = (uint16_t)(0x0842U + ((uint16_t)ly << 5));
            else if (ly > wallBottom)
                ink = (uint16_t)(0x1042U +
                                 ((uint16_t)(PREVIEW_H - 1 - ly) << 5));
            else {
                const int u = lx < 54 ? lx * 2 : lx >= 90
                             ? (PREVIEW_W - 1 - lx) * 2 : (lx - 54) * 2;
                const int v = (ly - wallTop) * 63 /
                              (wallBottom - wallTop + 1);
                ink = sampleWall(u, v);
            }
            fb[(PREVIEW_Y + ly) * DOOMRPG_LOGICAL_WIDTH +
               PREVIEW_X + lx] = ink;
        }
    }
}
int EspNativeHelpScene_paint(uint16_t* fb, uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    static const int yZone[4] = {0, 22, 45, PREVIEW_H};
    int row, col;
    if (fb == NULL || !material.ready || level > 16U) return 0;
    helpPaintOriginalBackground(fb);
    for (row = 0; row < 3; ++row)
        for (col = 0; col < 3; ++col)
            helpPaintTouchCell(fb, row, col,
                               PREVIEW_X + xZone[col],
                               PREVIEW_X + xZone[col + 1] - 1,
                               PREVIEW_Y + yZone[row],
                               PREVIEW_Y + yZone[row + 1] - 1, level);
    return 1;
}
/* The three teaching rows retain their original 64x64 PAK wall source but
 * sample distinct, bounded texture windows, not a cached gameplay frame. */
int EspNativeHelpScene_paintRow(uint16_t* fb, uint8_t sourceRow,
                                uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    const int top = 39, bottom = 61;
    int lx, ly, col;
    if (fb == NULL || !material.ready || sourceRow > 2U ||
        level > 16U) return 0;
    for (ly = top; ly <= bottom; ++ly) {
        for (lx = 0; lx < PREVIEW_W; ++lx) {
            const int u = (lx * 63 / (PREVIEW_W - 1) +
                           (int)sourceRow * 19) & 63;
            const int v = ((ly - top) * 63 / (bottom - top) +
                           (int)sourceRow * 23) & 63;
            fb[ly * DOOMRPG_LOGICAL_WIDTH + PREVIEW_X + lx] =
                sampleWall(u, v);
        }
    }
    for (col = 0; col < 3; ++col)
        helpPaintTouchCell(fb, sourceRow, col,
                           PREVIEW_X + xZone[col],
                           PREVIEW_X + xZone[col + 1] - 1,
                           top, bottom, level);
    return 1;
}
