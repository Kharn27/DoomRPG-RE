#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_native_gameplay_controls.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define NEON_BLUE_HALO   0x0008U
#define NEON_BLUE_CORE   0x001fU
#define NEON_GREEN_HALO  0x0200U
#define NEON_GREEN_CORE  0x07e0U
#define NEON_YELLOW_HALO 0x4200U
#define NEON_YELLOW_CORE 0xffe0U
#define NEON_RED_HALO    0x4000U
#define NEON_RED_CORE    0xf800U

typedef struct TouchFeedbackEdit_s {
    uint16_t offset;
    uint16_t saved;
} TouchFeedbackEdit;

typedef struct TouchFeedback_s {
    TouchFeedbackEdit edits[ESP_NATIVE_GAMEPLAY_FEEDBACK_MAX_EDITS];
    uint32_t expiresMs;
    uint32_t baselineFNV;
    uint32_t overlayFNV;
    uint16_t count;
    uint8_t action;
    uint8_t zone;
    uint8_t active;
    uint8_t reserved;
} TouchFeedback;

typedef struct FeedbackPalette_s {
    uint16_t halo;
    uint16_t core;
} FeedbackPalette;

static TouchFeedback feedback;

static uint32_t fnv1a(const void* data, uint32_t bytes) {
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
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

static uint32_t nowMs(void) {
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static FeedbackPalette feedbackPalette(const EspNativeGameplayTouchHit* hit) {
    FeedbackPalette palette;
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

static uint16_t glowAdd565(uint16_t destination, uint16_t additive) {
    unsigned int r = ((destination >> 11) & 31U) + ((additive >> 11) & 31U);
    unsigned int g = ((destination >> 5) & 63U) + ((additive >> 5) & 63U);
    unsigned int b = (destination & 31U) + (additive & 31U);
    if (r > 31U) r = 31U;
    if (g > 63U) g = 63U;
    if (b > 31U) b = 31U;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static int editFeedbackPixel(uint16_t* framebuffer,
                             int x,
                             int y,
                             uint16_t additive) {
    TouchFeedbackEdit* edit;
    uint16_t* pixel;
    unsigned int offset;

    if (framebuffer == NULL ||
        x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT ||
        feedback.count >= ESP_NATIVE_GAMEPLAY_FEEDBACK_MAX_EDITS) {
        return 0;
    }

    offset = (unsigned int)y * DOOMRPG_LOGICAL_WIDTH + (unsigned int)x;
    edit = &feedback.edits[feedback.count++];
    pixel = framebuffer + offset;
    edit->offset = (uint16_t)offset;
    edit->saved = *pixel;
    *pixel = glowAdd565(*pixel, additive);
    return 1;
}

static int drawFeedbackLine(uint16_t* framebuffer,
                            int x0,
                            int y0,
                            int x1,
                            int y1,
                            uint16_t additive) {
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dyAbs = y1 >= y0 ? y1 - y0 : y0 - y1;
    int dy = -dyAbs;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (!editFeedbackPixel(framebuffer, x0, y0, additive)) return 0;
        if (x0 == x1 && y0 == y1) break;
        {
            const int e2 = err << 1;
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

static int drawFeedbackRect(uint16_t* framebuffer,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            uint16_t additive) {
    int x;
    int y;
    if (left > right || top > bottom) return 0;
    for (x = left; x <= right; ++x) {
        if (!editFeedbackPixel(framebuffer, x, top, additive)) return 0;
    }
    if (bottom != top) {
        for (x = left; x <= right; ++x) {
            if (!editFeedbackPixel(framebuffer, x, bottom, additive)) return 0;
        }
    }
    for (y = top + 1; y < bottom; ++y) {
        if (!editFeedbackPixel(framebuffer, left, y, additive)) return 0;
        if (right != left &&
            !editFeedbackPixel(framebuffer, right, y, additive)) return 0;
    }
    return 1;
}

static int drawCardinalArrow(uint16_t* framebuffer,
                             int cx,
                             int cy,
                             int dx,
                             int dy,
                             uint16_t color) {
    if (dx < 0) {
        return drawFeedbackLine(framebuffer, cx + 5, cy, cx - 4, cy, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy, cx - 1, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy, cx - 1, cy + 4, color);
    }
    if (dx > 0) {
        return drawFeedbackLine(framebuffer, cx - 5, cy, cx + 4, cy, color) &&
               drawFeedbackLine(framebuffer, cx + 5, cy, cx + 1, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx + 5, cy, cx + 1, cy + 4, color);
    }
    if (dy < 0) {
        return drawFeedbackLine(framebuffer, cx, cy + 5, cx, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx, cy - 5, cx - 4, cy - 1, color) &&
               drawFeedbackLine(framebuffer, cx, cy - 5, cx + 4, cy - 1, color);
    }
    return drawFeedbackLine(framebuffer, cx, cy - 5, cx, cy + 4, color) &&
           drawFeedbackLine(framebuffer, cx, cy + 5, cx - 4, cy + 1, color) &&
           drawFeedbackLine(framebuffer, cx, cy + 5, cx + 4, cy + 1, color);
}

static int drawWeaponChevron(uint16_t* framebuffer,
                             int cx,
                             int cy,
                             int direction,
                             uint16_t color) {
    if (direction < 0) {
        return drawFeedbackLine(framebuffer, cx + 5, cy - 4, cx + 1, cy, color) &&
               drawFeedbackLine(framebuffer, cx + 1, cy, cx + 5, cy + 4, color) &&
               drawFeedbackLine(framebuffer, cx, cy - 4, cx - 4, cy, color) &&
               drawFeedbackLine(framebuffer, cx - 4, cy, cx, cy + 4, color);
    }
    return drawFeedbackLine(framebuffer, cx - 5, cy - 4, cx - 1, cy, color) &&
           drawFeedbackLine(framebuffer, cx - 1, cy, cx - 5, cy + 4, color) &&
           drawFeedbackLine(framebuffer, cx, cy - 4, cx + 4, cy, color) &&
           drawFeedbackLine(framebuffer, cx + 4, cy, cx, cy + 4, color);
}

static int drawActionGlyph(uint16_t* framebuffer,
                           const EspNativeGameplayTouchHit* hit,
                           uint16_t color) {
    const int cx = ((int)hit->left + (int)hit->right) >> 1;
    const int cy = ((int)hit->top + (int)hit->bottom) >> 1;

    switch (hit->action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
        return drawCardinalArrow(framebuffer, cx, cy, -1, 0, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        return drawCardinalArrow(framebuffer, cx, cy, 0, -1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
        return drawCardinalArrow(framebuffer, cx, cy, 1, 0, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        return drawCardinalArrow(framebuffer, cx, cy, 0, 1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
        return drawFeedbackLine(framebuffer, cx + 5, cy + 4, cx + 5, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx + 5, cy - 2, cx - 4, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy - 2, cx - 1, cy - 5, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy - 2, cx - 1, cy + 1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
        return drawFeedbackLine(framebuffer, cx - 5, cy + 4, cx - 5, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy - 2, cx + 4, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx + 5, cy - 2, cx + 1, cy - 5, color) &&
               drawFeedbackLine(framebuffer, cx + 5, cy - 2, cx + 1, cy + 1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
        return drawFeedbackLine(framebuffer, cx - 5, cy, cx - 2, cy, color) &&
               drawFeedbackLine(framebuffer, cx + 2, cy, cx + 5, cy, color) &&
               drawFeedbackLine(framebuffer, cx, cy - 5, cx, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx, cy + 2, cx, cy + 5, color) &&
               editFeedbackPixel(framebuffer, cx, cy, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON:
        return drawWeaponChevron(framebuffer, cx, cy, -1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON:
        return drawWeaponChevron(framebuffer, cx, cy, 1, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN:
        return drawFeedbackLine(framebuffer, cx - 5, cy - 4, cx + 5, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy, cx + 5, cy, color) &&
               drawFeedbackLine(framebuffer, cx - 5, cy + 4, cx + 5, cy + 4, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
        return drawFeedbackLine(framebuffer, cx - 6, cy + 4, cx - 6, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx - 6, cy - 4, cx - 2, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx - 2, cy - 2, cx + 2, cy - 4, color) &&
               drawFeedbackLine(framebuffer, cx + 2, cy - 4, cx + 6, cy - 2, color) &&
               drawFeedbackLine(framebuffer, cx + 6, cy - 2, cx + 6, cy + 4, color) &&
               drawFeedbackLine(framebuffer, cx - 2, cy - 2, cx - 2, cy + 4, color) &&
               drawFeedbackLine(framebuffer, cx + 2, cy - 4, cx + 2, cy + 2, color);
    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:
        return drawFeedbackLine(framebuffer, cx - 6, cy, cx + 2, cy, color) &&
               drawFeedbackLine(framebuffer, cx + 3, cy, cx, cy - 3, color) &&
               drawFeedbackLine(framebuffer, cx + 3, cy, cx, cy + 3, color) &&
               drawFeedbackLine(framebuffer, cx + 6, cy - 5, cx + 6, cy + 5, color);
    default:
        return 0;
    }
}

static void fillStats(EspNativeGameplayControlsStats* outStats) {
    if (outStats == NULL) return;
    memset(outStats, 0, sizeof(*outStats));
    outStats->baselineFNV = feedback.baselineFNV;
    outStats->overlayFNV = feedback.overlayFNV;
    outStats->edits = feedback.count;
    outStats->action = feedback.action;
    outStats->zone = feedback.zone;
}

void EspNativeGameplayControls_reset(void) {
    memset(&feedback, 0, sizeof(feedback));
}

int EspNativeGameplayControls_begin(
    const EspNativeGameplayTouchHit* hit,
    EspNativeGameplayControlsStats* outStats) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    FeedbackPalette palette;
    uint32_t baseline;
    int innerLeft;
    int innerTop;
    int innerRight;
    int innerBottom;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (hit == NULL || framebuffer == NULL || feedback.active ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0;
    }

    baseline = frameFNV();
    if (baseline == 0U) return 0;

    memset(&feedback, 0, sizeof(feedback));
    feedback.baselineFNV = baseline;
    feedback.action = hit->action;
    feedback.zone = hit->zone;
    feedback.active = 1U;

    palette = feedbackPalette(hit);
    innerLeft = (int)hit->left + 1;
    innerTop = (int)hit->top + 1;
    innerRight = (int)hit->right - 1;
    innerBottom = (int)hit->bottom - 1;

    if (!drawFeedbackRect(framebuffer,
                          hit->left, hit->top, hit->right, hit->bottom,
                          palette.halo) ||
        !drawFeedbackRect(framebuffer,
                          innerLeft, innerTop, innerRight, innerBottom,
                          palette.core) ||
        !drawActionGlyph(framebuffer, hit, palette.core)) {
        (void)EspNativeGameplayControls_restore(0, NULL);
        return 0;
    }

    feedback.overlayFNV = frameFNV();
    if (feedback.overlayFNV == 0U || feedback.overlayFNV == baseline) {
        (void)EspNativeGameplayControls_restore(0, NULL);
        return 0;
    }
    feedback.expiresMs = nowMs() + ESP_NATIVE_GAMEPLAY_FEEDBACK_MS;
    fillStats(outStats);
    return 1;
}

int EspNativeGameplayControls_isActive(void) {
    return feedback.active != 0U;
}

int EspNativeGameplayControls_isExpired(void) {
    return feedback.active != 0U &&
           (int32_t)(nowMs() - feedback.expiresMs) >= 0;
}

int EspNativeGameplayControls_restore(
    int present,
    EspNativeGameplayControlsStats* outStats) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    uint16_t index;
    uint32_t expected;
    int ok = 1;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (!feedback.active) return 1;
    if (framebuffer == NULL) return 0;

    fillStats(outStats);
    expected = feedback.baselineFNV;
    index = feedback.count;
    while (index > 0U) {
        const TouchFeedbackEdit* edit = &feedback.edits[--index];
        framebuffer[edit->offset] = edit->saved;
    }

    if (frameFNV() != expected) ok = 0;
    if (ok && present && !Esp32PlatformVideo_present()) ok = 0;
    memset(&feedback, 0, sizeof(feedback));
    return ok;
}
