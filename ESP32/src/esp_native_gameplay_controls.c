#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_input.h"
#include "platform_video_config.h"

#define CONTROL_ZONE_COUNT 12U

/* Reuse the hardware-proven transient touch-feedback visual language, but as a
 * persistent final compositor layer. Values are additive RGB565 energy, not
 * opaque button fills. */
#define NEON_BLUE_HALO   0x0008U
#define NEON_BLUE_CORE   0x001fU
#define NEON_GREEN_HALO  0x0200U
#define NEON_GREEN_CORE  0x07e0U
#define NEON_YELLOW_HALO 0x4200U
#define NEON_YELLOW_CORE 0xffe0U
#define NEON_RED_HALO    0x4000U
#define NEON_RED_CORE    0xf800U

typedef struct ControlPalette_s {
    uint16_t halo;
    uint16_t core;
} ControlPalette;

static uint16_t glowAdd565(uint16_t destination, uint16_t additive) {
    unsigned int r = ((destination >> 11) & 31U) + ((additive >> 11) & 31U);
    unsigned int g = ((destination >> 5) & 63U) + ((additive >> 5) & 63U);
    unsigned int b = (destination & 31U) + (additive & 31U);
    if (r > 31U) r = 31U;
    if (g > 63U) g = 63U;
    if (b > 31U) b = 31U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static ControlPalette paletteFor(const EspNativeGameplayTouchHit* hit) {
    ControlPalette palette;
    if (hit == NULL || hit->top < 20U) {
        palette.halo = NEON_BLUE_HALO;
        palette.core = NEON_BLUE_CORE;
    }
    else if (hit->top < 46U) {
        palette.halo = NEON_GREEN_HALO;
        palette.core = NEON_GREEN_CORE;
    }
    else if (hit->top < 73U) {
        palette.halo = NEON_YELLOW_HALO;
        palette.core = NEON_YELLOW_CORE;
    }
    else {
        palette.halo = NEON_RED_HALO;
        palette.core = NEON_RED_CORE;
    }
    return palette;
}

static int actionActive(uint8_t action) {
    return action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT;
}

static int addPixel(uint16_t* framebuffer,
                    uint32_t framebufferPixels,
                    int x,
                    int y,
                    uint16_t additive,
                    uint32_t* categoryPixels,
                    EspNativeGameplayControlsStats* stats) {
    uint32_t offset;
    if (framebuffer == NULL || stats == NULL ||
        x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }
    offset = (uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x;
    if (offset >= framebufferPixels) return 0;
    framebuffer[offset] = glowAdd565(framebuffer[offset], additive);
    ++stats->pixelsTouched;
    if (categoryPixels != NULL) ++(*categoryPixels);
    return 1;
}

static int drawLine(uint16_t* framebuffer,
                    uint32_t framebufferPixels,
                    int x0,
                    int y0,
                    int x1,
                    int y1,
                    uint16_t additive,
                    uint32_t* categoryPixels,
                    EspNativeGameplayControlsStats* stats) {
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dyAbs = y1 >= y0 ? y1 - y0 : y0 - y1;
    int dy = -dyAbs;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (!addPixel(framebuffer, framebufferPixels, x0, y0, additive,
                      categoryPixels, stats)) return 0;
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
    return 1;
}

static int drawRect(uint16_t* framebuffer,
                    uint32_t framebufferPixels,
                    int left,
                    int top,
                    int right,
                    int bottom,
                    uint16_t additive,
                    EspNativeGameplayControlsStats* stats) {
    return left <= right && top <= bottom &&
           drawLine(framebuffer, framebufferPixels, left, top, right, top,
                    additive, &stats->borderPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, right, top, right, bottom,
                    additive, &stats->borderPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, right, bottom, left, bottom,
                    additive, &stats->borderPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, left, bottom, left, top,
                    additive, &stats->borderPixels, stats);
}

static int drawCardinalArrow(uint16_t* framebuffer,
                             uint32_t framebufferPixels,
                             int cx,
                             int cy,
                             int dx,
                             int dy,
                             uint16_t color,
                             EspNativeGameplayControlsStats* stats) {
    if (dx < 0) {
        return drawLine(framebuffer, framebufferPixels, cx + 5, cy, cx - 4, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy, cx - 1, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy, cx - 1, cy + 4,
                        color, &stats->glyphPixels, stats);
    }
    if (dx > 0) {
        return drawLine(framebuffer, framebufferPixels, cx - 5, cy, cx + 4, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 5, cy, cx + 1, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 5, cy, cx + 1, cy + 4,
                        color, &stats->glyphPixels, stats);
    }
    if (dy < 0) {
        return drawLine(framebuffer, framebufferPixels, cx, cy + 5, cx, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx, cy - 5, cx - 4, cy - 1,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx, cy - 5, cx + 4, cy - 1,
                        color, &stats->glyphPixels, stats);
    }
    return drawLine(framebuffer, framebufferPixels, cx, cy - 5, cx, cy + 4,
                    color, &stats->glyphPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, cx, cy + 5, cx - 4, cy + 1,
                    color, &stats->glyphPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, cx, cy + 5, cx + 4, cy + 1,
                    color, &stats->glyphPixels, stats);
}

static int drawWeaponChevron(uint16_t* framebuffer,
                             uint32_t framebufferPixels,
                             int cx,
                             int cy,
                             int direction,
                             uint16_t color,
                             EspNativeGameplayControlsStats* stats) {
    if (direction < 0) {
        return drawLine(framebuffer, framebufferPixels, cx + 5, cy - 4, cx + 1, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 1, cy, cx + 5, cy + 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx, cy - 4, cx - 4, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 4, cy, cx, cy + 4,
                        color, &stats->glyphPixels, stats);
    }
    return drawLine(framebuffer, framebufferPixels, cx - 5, cy - 4, cx - 1, cy,
                    color, &stats->glyphPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, cx - 1, cy, cx - 5, cy + 4,
                    color, &stats->glyphPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, cx, cy - 4, cx + 4, cy,
                    color, &stats->glyphPixels, stats) &&
           drawLine(framebuffer, framebufferPixels, cx + 4, cy, cx, cy + 4,
                    color, &stats->glyphPixels, stats);
}

static int drawGlyph(uint16_t* framebuffer,
                     uint32_t framebufferPixels,
                     const EspNativeGameplayTouchHit* hit,
                     uint16_t color,
                     EspNativeGameplayControlsStats* stats) {
    int cx;
    int cy;
    if (hit == NULL || stats == NULL) return 0;
    cx = ((int)hit->left + (int)hit->right) >> 1;
    cy = ((int)hit->top + (int)hit->bottom) >> 1;

    switch (hit->action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
        return drawCardinalArrow(framebuffer, framebufferPixels, cx, cy, -1, 0,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        return drawCardinalArrow(framebuffer, framebufferPixels, cx, cy, 0, -1,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
        return drawCardinalArrow(framebuffer, framebufferPixels, cx, cy, 1, 0,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        return drawCardinalArrow(framebuffer, framebufferPixels, cx, cy, 0, 1,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
        return drawLine(framebuffer, framebufferPixels, cx + 5, cy + 4, cx + 5, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 5, cy - 2, cx - 4, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy - 2, cx - 1, cy - 5,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy - 2, cx - 1, cy + 1,
                        color, &stats->glyphPixels, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
        return drawLine(framebuffer, framebufferPixels, cx - 5, cy + 4, cx - 5, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy - 2, cx + 4, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 5, cy - 2, cx + 1, cy - 5,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 5, cy - 2, cx + 1, cy + 1,
                        color, &stats->glyphPixels, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
        return drawLine(framebuffer, framebufferPixels, cx - 5, cy, cx - 2, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 2, cy, cx + 5, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx, cy - 5, cx, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx, cy + 2, cx, cy + 5,
                        color, &stats->glyphPixels, stats) &&
               addPixel(framebuffer, framebufferPixels, cx, cy, color,
                        &stats->glyphPixels, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON:
        return drawWeaponChevron(framebuffer, framebufferPixels, cx, cy, -1,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON:
        return drawWeaponChevron(framebuffer, framebufferPixels, cx, cy, 1,
                                 color, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN:
        return drawLine(framebuffer, framebufferPixels, cx - 5, cy - 4, cx + 5, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy, cx + 5, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 5, cy + 4, cx + 5, cy + 4,
                        color, &stats->glyphPixels, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
        return drawLine(framebuffer, framebufferPixels, cx - 6, cy + 4, cx - 6, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 6, cy - 4, cx - 2, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 2, cy - 2, cx + 2, cy - 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 2, cy - 4, cx + 6, cy - 2,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 6, cy - 2, cx + 6, cy + 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx - 2, cy - 2, cx - 2, cy + 4,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 2, cy - 4, cx + 2, cy + 2,
                        color, &stats->glyphPixels, stats);
    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:
        return drawLine(framebuffer, framebufferPixels, cx - 6, cy, cx + 2, cy,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 3, cy, cx, cy - 3,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 3, cy, cx, cy + 3,
                        color, &stats->glyphPixels, stats) &&
               drawLine(framebuffer, framebufferPixels, cx + 6, cy - 5, cx + 6, cy + 5,
                        color, &stats->glyphPixels, stats);
    default:
        return 0;
    }
}

int EspNativeGameplayControls_draw(
    uint16_t* framebuffer,
    uint32_t framebufferPixels,
    EspNativeGameplayControlsStats* outStats) {
    EspNativeGameplayControlsStats stats;
    uint8_t count;
    uint8_t i;

    memset(&stats, 0, sizeof(stats));
    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (framebuffer == NULL || outStats == NULL ||
        framebufferPixels != (uint32_t)DOOMRPG_LOGICAL_WIDTH *
                                 (uint32_t)DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }

    count = EspNativeGameplayInput_zoneCount();
    if (count != CONTROL_ZONE_COUNT) return 0;

    for (i = 0U; i < count; ++i) {
        EspNativeGameplayTouchHit hit;
        ControlPalette palette;
        int innerLeft;
        int innerTop;
        int innerRight;
        int innerBottom;

        if (EspNativeGameplayInput_zoneAt(i, &hit) != ESP_NATIVE_GAMEPLAY_INPUT_OK ||
            hit.left > hit.right || hit.top > hit.bottom) {
            return 0;
        }
        palette = paletteFor(&hit);
        innerLeft = (int)hit.left + 1;
        innerTop = (int)hit.top + 1;
        innerRight = (int)hit.right - 1;
        innerBottom = (int)hit.bottom - 1;

        if (!drawRect(framebuffer, framebufferPixels,
                      hit.left, hit.top, hit.right, hit.bottom,
                      palette.halo, &stats) ||
            !drawRect(framebuffer, framebufferPixels,
                      innerLeft, innerTop, innerRight, innerBottom,
                      palette.core, &stats) ||
            !drawGlyph(framebuffer, framebufferPixels, &hit,
                       palette.core, &stats)) {
            return 0;
        }
        ++stats.zonesDrawn;
        if (actionActive(hit.action)) ++stats.activeActions;
        else ++stats.deferredActions;
    }

    *outStats = stats;
    return stats.zonesDrawn == CONTROL_ZONE_COUNT &&
           stats.activeActions == 6U && stats.deferredActions == 6U;
}
