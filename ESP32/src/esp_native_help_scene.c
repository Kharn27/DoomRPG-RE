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
/* Small persistent thumbnail of a REAL presented gameplay frame.
 * 128x64xRGB565=16384 B, not a second 160x120 framebuffer and not a map.
 * Capture once per level; original source viewport is 160x80 at y20..99.
 * No screenshot of an invented wall or randomly selected PAK door texture. */
#define HELP_SNAPSHOT_W 128
#define HELP_SNAPSHOT_H 64

static struct {
    uint16_t pixel[HELP_SNAPSHOT_W * HELP_SNAPSHOT_H];
    uint8_t mapId;
    uint8_t valid;
} lastWorld;
static uint8_t helpActive;
static uint8_t requestedMapId;

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
    /* Opening/closing HELP does not erase the snapshot owned by gameplay.
     * A new map will replace it on its next successful native present. */
    helpActive = 0U;
    requestedMapId = 0U;
}
void EspNativeHelpScene_begin(uint8_t cachedMapId) {
    helpActive = 1U;
    requestedMapId = cachedMapId;
}
int EspNativeHelpScene_isReady(void) { return helpActive != 0U; }
int EspNativeHelpScene_hasGameplaySnapshot(void) {
    return helpActive != 0U && lastWorld.valid != 0U &&
           requestedMapId != 0U && lastWorld.mapId == requestedMapId;
}
uint8_t EspNativeHelpScene_snapshotMapId(void) {
    return EspNativeHelpScene_hasGameplaySnapshot() ? lastWorld.mapId : 0U;
}
int EspNativeHelpScene_captureGameplayViewport(const uint16_t* fb,
                                               uint8_t mapId) {
    int x, y;
    if (fb == NULL || mapId == 0U) return 0;
    /* Exactly one 16 KB copy per new map. No cost on ordinary turn redraws.
     * Capture is called only after a canonical published, non-automap frame. */
    if (lastWorld.valid != 0U && lastWorld.mapId == mapId) return 1;
    lastWorld.valid = 0U;
    for (y = 0; y < HELP_SNAPSHOT_H; ++y) {
        const int sourceY = 20 + y * 80 / HELP_SNAPSHOT_H;
        for (x = 0; x < HELP_SNAPSHOT_W; ++x) {
            const int sourceX = x * 160 / HELP_SNAPSHOT_W;
            lastWorld.pixel[y * HELP_SNAPSHOT_W + x] =
                fb[sourceY * DOOMRPG_LOGICAL_WIDTH + sourceX];
        }
    }
    lastWorld.mapId = mapId;
    lastWorld.valid = 1U;
    return 1;
}
/* One unique 2.56s smoothstep breath clock (32 x 80ms), shared by
 * top/bottom HUD miniatures, full 3x3, and every instructional row. */
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
    /* smoothstep 3t²-2t³, t=level/16. Adjacent steps are near-identical
     * around darkness/peak: no jump from all-off to bright button glyphs. */
    return (uint8_t)(((uint32_t)level * level *
        (48U - 2U * level) * 255U + 2048U) / 4096U);
}
static uint16_t snapshotPixel(int lx, int ly) {
    const int x = lx * HELP_SNAPSHOT_W / PREVIEW_W;
    const int y = ly * HELP_SNAPSHOT_H / PREVIEW_H;
    return lastWorld.pixel[y * HELP_SNAPSHOT_W + x];
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
static void helpPaintTouchCell(uint16_t* fb, int row, int col,
                               int left, int right, int top, int bottom,
                               uint8_t level) {
    const uint16_t neon = row == 1 && col == 1 ? 0xf800U :
                          row == 2 && col != 1 ? 0xffe0U :
                          0x07e0U;
    const uint8_t power = EspNativeHelpScene_breathPower(level);
    const uint16_t core = dim565(neon, power);
    const uint16_t halo = dim565(neon, (uint32_t)power / 4U);
    int x, y;

    if (power > 0U) {
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
    }
    if (row == 2 && col != 1) {
        /* Original WPN labels stay readable at rest; ONLY neon breathes.
         * No abrupt appearance/disappearance of the black reading pad. */
        helpPaintWeaponCaption(fb, (left + right) / 2, top,
                               col == 0 ? "PREV" : "NEXT", 0xffe0U);
    }
    else if (power > 0U) {
        drawActionMark(fb, row, col, (left + right) / 2,
                       (top + bottom) / 2, core);
    }
}

int EspNativeHelpScene_paint(uint16_t* fb, uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    static const int yZone[4] = {0, 22, 45, PREVIEW_H};
    int lx, ly, row, col;
    if (fb == NULL || !helpActive || level > 16U) return 0;

    /* REAL sampled first-person gameplay view if this map has been played;
     * first-ever boot falls back to the original starfield reading window,
     * never an artificial corridor or random door sprite. */
    if (EspNativeHelpScene_hasGameplaySnapshot()) {
        for (ly = 0; ly < PREVIEW_H; ++ly)
            for (lx = 0; lx < PREVIEW_W; ++lx)
                fb[(PREVIEW_Y + ly) * DOOMRPG_LOGICAL_WIDTH +
                   PREVIEW_X + lx] = snapshotPixel(lx, ly);
    }

    /* Exact 3x3 native touch order; no active world/map renderer. */
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            const int left = PREVIEW_X + xZone[col];
            const int right = PREVIEW_X + xZone[col + 1] - 1;
            const int top = PREVIEW_Y + yZone[row];
            const int bottom = PREVIEW_Y + yZone[row + 1] - 1;
            helpPaintTouchCell(fb, row, col, left, right, top, bottom, level);
        }
    }
    return 1;
}

/* x8..151/y39..61: downsample the SAME real saved native viewpoint.
 * No screenshot? Preserve the starfield. Icons and breath stay identical. */
int EspNativeHelpScene_paintRow(uint16_t* fb, uint8_t sourceRow,
                                uint8_t level) {
    static const int xZone[4] = {0, 48, 96, PREVIEW_W};
    const int top = 39, bottom = 61;
    int lx, ly, col;
    if (fb == NULL || !helpActive || sourceRow > 2U || level > 16U)
        return 0;
    if (EspNativeHelpScene_hasGameplaySnapshot()) {
        for (ly = top; ly <= bottom; ++ly) {
            for (lx = 0; lx < PREVIEW_W; ++lx) {
                fb[ly * DOOMRPG_LOGICAL_WIDTH + PREVIEW_X + lx] =
                    snapshotPixel(lx, (ly - top) *
                                   (PREVIEW_H - 1) / (bottom - top));
            }
        }
    }
    for (col = 0; col < 3; ++col) {
        helpPaintTouchCell(fb, sourceRow, col,
                           PREVIEW_X + xZone[col],
                           PREVIEW_X + xZone[col + 1] - 1,
                           top, bottom, level);
    }
    return 1;
}
