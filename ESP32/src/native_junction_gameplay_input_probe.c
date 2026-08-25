#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_input.h"
#include "native_junction_gameplay_hud_probe.h"
#include "native_junction_gameplay_input_probe.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

#define EXPECTED_GAMEPLAY_FRAME_FNV 0xba3e5182U
#define EXPECTED_GAMEPLAY_HUD_FNV 0x4756db9cU
#define TOUCH_FEEDBACK_MS 120U
#define TOUCH_FEEDBACK_MAX_EDITS 512U

#define NEON_BLUE_HALO 0x0008U
#define NEON_BLUE_CORE 0x001fU
#define NEON_GREEN_HALO 0x0200U
#define NEON_GREEN_CORE 0x07e0U
#define NEON_YELLOW_HALO 0x4200U
#define NEON_YELLOW_CORE 0xffe0U
#define NEON_RED_HALO 0x4000U
#define NEON_RED_CORE 0xf800U

typedef struct LegacySnapshot_s {
    uint32_t hud;
    uint32_t player;
    uint32_t game;
    uint32_t canvas;
    uint32_t render;
} LegacySnapshot;

typedef struct TouchFeedbackEdit_s {
    uint16_t offset;
    uint16_t saved;
} TouchFeedbackEdit;

typedef struct TouchFeedback_s {
    TouchFeedbackEdit edits[TOUCH_FEEDBACK_MAX_EDITS];
    uint32_t expiresMs;
    uint32_t baselineFNV;
    uint16_t count;
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
    uint8_t action;
    uint8_t zone;
    uint8_t active;
    uint8_t reserved;
} TouchFeedback;

typedef struct FeedbackPalette_s {
    uint16_t halo;
    uint16_t core;
    const char* name;
} FeedbackPalette;

typedef struct GameplayInputProbeState_s {
    DoomRPG_t* doomRpg;
    TouchFeedback feedback;
    uint32_t taps;
    uint32_t intents;
    uint32_t misses;
    uint32_t restores;
    uint8_t active;
    uint8_t failed;
    uint8_t reserved[2];
} GameplayInputProbeState;

static GameplayInputProbeState probeState;

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

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t nowMs(void) {
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static int legacySnapshot(const DoomRPG_t* doomRpg, LegacySnapshot* out) {
    if (doomRpg == NULL || out == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->render == NULL) {
        return 0;
    }
    out->hud = fnv1a(doomRpg->hud, sizeof(*doomRpg->hud));
    out->player = fnv1a(doomRpg->player, sizeof(*doomRpg->player));
    out->game = fnv1a(doomRpg->game, sizeof(*doomRpg->game));
    out->canvas = fnv1a(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    out->render = fnv1a(doomRpg->render, sizeof(*doomRpg->render));
    return 1;
}

static int legacySnapshotEqual(const LegacySnapshot* a,
                               const LegacySnapshot* b) {
    return a != NULL && b != NULL && memcmp(a, b, sizeof(*a)) == 0;
}

static int runtimeBoundary(const DoomRPG_t* doomRpg) {
    const Render_t* render;
    const EspNativeGameplayHudState* hudState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL) {
        return 0;
    }
    render = doomRpg->render;
    hudState = EspNativeGameplayHud_view();
    return doomRpg->doomCanvas->state == ST_INTRO &&
           doomRpg->doomCanvas->storyPage == 3 &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0 &&
           render->framebuffer == Esp32PlatformVideo_framebuffer() &&
           render->screenX == 0 && render->screenY == 20 &&
           render->screenWidth == 160 && render->screenHeight == 80 &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           EspNativeGameplayHud_isReady() && hudState != NULL &&
           fnv1a(hudState, sizeof(*hudState)) == EXPECTED_GAMEPLAY_HUD_FNV &&
           !EspAssetPack_isOpen();
}

static int hitMatches(const EspNativeGameplayTouchHit* hit,
                      uint8_t action,
                      uint8_t zone,
                      uint8_t left,
                      uint8_t top,
                      uint8_t right,
                      uint8_t bottom) {
    return hit != NULL && hit->action == action && hit->zone == zone &&
           hit->left == left && hit->top == top &&
           hit->right == right && hit->bottom == bottom;
}

static int pureHitTestContract(void) {
    EspNativeGameplayTouchHit hit;
    static const struct {
        uint8_t x;
        uint8_t y;
        uint8_t action;
        uint8_t zone;
        uint8_t left;
        uint8_t top;
        uint8_t right;
        uint8_t bottom;
    } tests[] = {
        {16, 10, ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN, ESP_NATIVE_GAMEPLAY_ZONE_MENU, 0, 0, 31, 19},
        {80, 10, ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN, ESP_NATIVE_GAMEPLAY_ZONE_PASS_TURN, 32, 0, 127, 19},
        {144, 10, ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP, ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP, 128, 0, 159, 19},
        {26, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_LEFT, 0, 20, 52, 45},
        {79, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD, 53, 20, 105, 45},
        {132, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_RIGHT, 106, 20, 159, 45},
        {26, 59, ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT, ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT, 0, 46, 52, 72},
        {79, 59, ESP_NATIVE_GAMEPLAY_ACTION_SELECT, ESP_NATIVE_GAMEPLAY_ZONE_SELECT, 53, 46, 105, 72},
        {132, 59, ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT, ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT, 106, 46, 159, 72},
        {26, 86, ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_PREV_WEAPON, 0, 73, 52, 99},
        {79, 86, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK, 53, 73, 105, 99},
        {132, 86, ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON, 106, 73, 159, 99}
    };
    unsigned int i;

    if (sizeof(EspNativeGameplayInputState) != 12U ||
        sizeof(EspNativeGameplayTouchHit) != 6U) {
        return 0;
    }

    for (i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (EspNativeGameplayInput_classify(tests[i].x, tests[i].y, &hit) !=
                ESP_NATIVE_GAMEPLAY_INPUT_OK ||
            !hitMatches(&hit, tests[i].action, tests[i].zone,
                        tests[i].left, tests[i].top,
                        tests[i].right, tests[i].bottom)) {
            return 0;
        }
    }

    if (EspNativeGameplayInput_classify(80, 110, &hit) !=
            ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT ||
        EspNativeGameplayInput_classify(-1, 20, &hit) !=
            ESP_NATIVE_GAMEPLAY_INPUT_INVALID ||
        EspNativeGameplayInput_classify(160, 20, &hit) !=
            ESP_NATIVE_GAMEPLAY_INPUT_INVALID) {
        return 0;
    }
    return 1;
}

static void printZone(int logicalX, int logicalY) {
    EspNativeGameplayTouchHit hit;
    if (EspNativeGameplayInput_classify(logicalX, logicalY, &hit) !=
            ESP_NATIVE_GAMEPLAY_INPUT_OK) {
        return;
    }
    printf("[NATIVEINPUT] ZONE zone=%u action=%s logical=x%u..%u y%u..%u physical=x%u..%u y%u..%u\n",
           (unsigned int)hit.zone,
           EspNativeGameplayInput_actionName(hit.action),
           (unsigned int)hit.left, (unsigned int)hit.right,
           (unsigned int)hit.top, (unsigned int)hit.bottom,
           (unsigned int)hit.left * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.right + 1U) * DOOMRPG_INTEGER_SCALE) - 1U,
           (unsigned int)hit.top * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.bottom + 1U) * DOOMRPG_INTEGER_SCALE) - 1U);
}

static FeedbackPalette feedbackPalette(const EspNativeGameplayTouchHit* hit) {
    FeedbackPalette palette;
    if (hit == NULL || hit->top < 20U) {
        palette.halo = NEON_BLUE_HALO;
        palette.core = NEON_BLUE_CORE;
        palette.name = "BLUE";
    }
    else if (hit->top < 46U) {
        palette.halo = NEON_GREEN_HALO;
        palette.core = NEON_GREEN_CORE;
        palette.name = "GREEN";
    }
    else if (hit->top < 73U) {
        palette.halo = NEON_YELLOW_HALO;
        palette.core = NEON_YELLOW_CORE;
        palette.name = "YELLOW";
    }
    else {
        palette.halo = NEON_RED_HALO;
        palette.core = NEON_RED_CORE;
        palette.name = "RED";
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
    TouchFeedback* feedback = &probeState.feedback;
    TouchFeedbackEdit* edit;
    uint16_t* pixel;
    unsigned int offset;

    if (x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }
    if (feedback->count >= TOUCH_FEEDBACK_MAX_EDITS) return 0;

    offset = (unsigned int)y * DOOMRPG_LOGICAL_WIDTH + (unsigned int)x;
    edit = &feedback->edits[feedback->count++];
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

static int drawFeedback(const EspNativeGameplayTouchHit* hit) {
    TouchFeedback* feedback = &probeState.feedback;
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const FeedbackPalette palette = feedbackPalette(hit);
    const int innerLeft = (int)hit->left + 1;
    const int innerTop = (int)hit->top + 1;
    const int innerRight = (int)hit->right - 1;
    const int innerBottom = (int)hit->bottom - 1;
    const uint32_t baseline = frameFNV();

    if (hit == NULL || framebuffer == NULL || feedback->active || baseline == 0U)
        return 0;
    memset(feedback, 0, sizeof(*feedback));
    feedback->baselineFNV = baseline;
    feedback->left = hit->left;
    feedback->top = hit->top;
    feedback->right = hit->right;
    feedback->bottom = hit->bottom;
    feedback->action = hit->action;
    feedback->zone = hit->zone;
    feedback->active = 1U;

    if (!drawFeedbackRect(framebuffer,
                          hit->left, hit->top, hit->right, hit->bottom,
                          palette.halo) ||
        !drawFeedbackRect(framebuffer,
                          innerLeft, innerTop, innerRight, innerBottom,
                          palette.core) ||
        !drawActionGlyph(framebuffer, hit, palette.core)) {
        return 0;
    }

    feedback->expiresMs = nowMs() + TOUCH_FEEDBACK_MS;
    return 1;
}

static int restoreFeedback(int present) {
    TouchFeedback* feedback = &probeState.feedback;
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const uint32_t expectedFNV = feedback->baselineFNV;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint16_t index;
    int ok = 1;

    if (!feedback->active) return 1;
    if (framebuffer == NULL || expectedFNV == 0U) return 0;

    heapBefore = heap8();
    largestBefore = largest8();

    index = feedback->count;
    while (index > 0U) {
        const TouchFeedbackEdit* edit = &feedback->edits[--index];
        framebuffer[edit->offset] = edit->saved;
    }

    if (frameFNV() != expectedFNV) ok = 0;
    if (ok && present && !Esp32PlatformVideo_present()) ok = 0;

    heapAfter = heap8();
    largestAfter = largest8();
    if (heapAfter != heapBefore || largestAfter != largestBefore) ok = 0;

    if (ok) {
        ++probeState.restores;
        printf("[NATIVEINPUT] FEEDBACK RESTORE n=%u frame=%08x exact=yes dynamicBaseline=yes heap=%u->%u largest=%u->%u\n",
               (unsigned int)probeState.restores,
               (unsigned int)expectedFNV,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter);
    }
    else {
        printf("[NATIVEINPUT] FAILED feedback restore edits=%u expected=%08x frame=%08x present=%d heap=%u->%u largest=%u->%u\n",
               (unsigned int)feedback->count,
               (unsigned int)expectedFNV,
               (unsigned int)frameFNV(), present,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter);
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
    }

    memset(feedback, 0, sizeof(*feedback));
    return ok;
}

static void onGameplayTap(int16_t screenX,
                          int16_t screenY,
                          uint16_t pressure,
                          uint16_t rawX,
                          uint16_t rawY) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState consumed;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    LegacySnapshot legacyBefore;
    LegacySnapshot legacyAfter;
    EspNativeGameplayInputStatus classifyStatus;
    FeedbackPalette palette;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t baselineFNV;
    uint32_t overlayFNV;
    int logicalX;
    int logicalY;
    int hadFeedback;
    int presentRestored = 0;

    if (!probeState.active || probeState.failed || probeState.doomRpg == NULL) return;

    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    ++probeState.taps;
    classifyStatus = EspNativeGameplayInput_classify(logicalX, logicalY, &hit);

    hadFeedback = probeState.feedback.active != 0U;
    if (hadFeedback && !restoreFeedback(0)) return;

    baselineFNV = frameFNV();
    if (!runtimeBoundary(probeState.doomRpg) || baselineFNV == 0U ||
        EspNativeGameplayInput_peek()->pending) {
        printf("[NATIVEINPUT] FAILED tap boundary n=%u frame=%08x pending=%u shapeData=%p mediaTexels=%p\n",
               (unsigned int)probeState.taps,
               (unsigned int)baselineFNV,
               (unsigned int)EspNativeGameplayInput_peek()->pending,
               probeState.doomRpg->render != NULL ?
                   (void*)probeState.doomRpg->render->shapeData : NULL,
               probeState.doomRpg->render != NULL ?
                   (void*)probeState.doomRpg->render->mediaTexels : NULL);
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    printf("[NATIVEINPUT] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d status=%d frame=%08x\n",
           (unsigned int)probeState.taps,
           rawX, rawY, pressure, screenX, screenY,
           logicalX, logicalY, (int)classifyStatus,
           (unsigned int)baselineFNV);

    if (classifyStatus == ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT) {
        ++probeState.misses;
        if (hadFeedback) presentRestored = Esp32PlatformVideo_present();
        printf("[NATIVEINPUT] MISS n=%u logical=%d,%d unbound=bottom-HUD frame=%08x restoredPresent=%d gameplay=no\n",
               (unsigned int)probeState.misses,
               logicalX, logicalY,
               (unsigned int)baselineFNV, presentRestored);
        return;
    }
    if (classifyStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK) {
        printf("[NATIVEINPUT] FAILED classify n=%u physical=%d,%d logical=%d,%d status=%d\n",
               (unsigned int)probeState.taps,
               screenX, screenY, logicalX, logicalY, (int)classifyStatus);
        return;
    }

    heapBefore = heap8();
    largestBefore = largest8();
    if (!legacySnapshot(probeState.doomRpg, &legacyBefore) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        EspNativeGameplayInput_route(&hit, logicalX, logicalY) !=
            ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        EspNativeGameplayInput_consume(&consumed) !=
            ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        consumed.action != hit.action || consumed.zone != hit.zone ||
        consumed.logicalX != logicalX || consumed.logicalY != logicalY ||
        !consumed.pending || !consumed.active || consumed.sequence == 0U ||
        EspNativeGameplayInput_peek()->pending ||
        frameFNV() != baselineFNV ||
        !legacySnapshot(probeState.doomRpg, &legacyAfter) ||
        !legacySnapshotEqual(&legacyBefore, &legacyAfter) ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg)) {
        printf("[NATIVEINPUT] FAILED semantic route action=%u zone=%u sequence=%u frame=%08x expected=%08x pending=%u legacyStable=%d\n",
               (unsigned int)hit.action, (unsigned int)hit.zone,
               (unsigned int)consumed.sequence,
               (unsigned int)frameFNV(), (unsigned int)baselineFNV,
               (unsigned int)EspNativeGameplayInput_peek()->pending,
               legacySnapshotEqual(&legacyBefore, &legacyAfter));
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    if (!drawFeedback(&hit) ||
        probeState.feedback.baselineFNV != baselineFNV ||
        !Esp32PlatformVideo_present()) {
        printf("[NATIVEINPUT] FAILED feedback draw action=%s zone=%u edits=%u/%u baseline=%08x frame=%08x\n",
               EspNativeGameplayInput_actionName(hit.action),
               (unsigned int)hit.zone,
               (unsigned int)probeState.feedback.count,
               (unsigned int)TOUCH_FEEDBACK_MAX_EDITS,
               (unsigned int)probeState.feedback.baselineFNV,
               (unsigned int)frameFNV());
        (void)restoreFeedback(1);
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    overlayFNV = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (overlayFNV == baselineFNV ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        !runtimeBoundary(probeState.doomRpg) || EspAssetPack_isOpen()) {
        printf("[NATIVEINPUT] FAILED feedback invariant baseline=%08x frame=%08x heap=%u->%u largest=%u->%u pack=%d\n",
               (unsigned int)baselineFNV,
               (unsigned int)overlayFNV,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               EspAssetPack_isOpen());
        (void)restoreFeedback(1);
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    palette = feedbackPalette(&hit);
    ++probeState.intents;
    printf("[NATIVEINPUT] INTENT n=%u seq=%u action=%s id=%u zone=%u logical=%u,%u consumedBy=probe dispatch=observer gameplay=no\n",
           (unsigned int)probeState.intents,
           (unsigned int)consumed.sequence,
           EspNativeGameplayInput_actionName(consumed.action),
           (unsigned int)consumed.action,
           (unsigned int)consumed.zone,
           (unsigned int)consumed.logicalX,
           (unsigned int)consumed.logicalY);
    printf("[NATIVEINPUT] FEEDBACK zone=%u action=%s palette=%s logical=x%u..%u y%u..%u physical=x%u..%u y%u..%u edits=%u hold=%ums style=neon-double-ring+vector-glyph frame=%08x->%08x dynamicBaseline=yes heapDelta=0 largestDelta=0\n",
           (unsigned int)hit.zone,
           EspNativeGameplayInput_actionName(hit.action),
           palette.name,
           (unsigned int)hit.left, (unsigned int)hit.right,
           (unsigned int)hit.top, (unsigned int)hit.bottom,
           (unsigned int)hit.left * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.right + 1U) * DOOMRPG_INTEGER_SCALE) - 1U,
           (unsigned int)hit.top * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.bottom + 1U) * DOOMRPG_INTEGER_SCALE) - 1U,
           (unsigned int)probeState.feedback.count,
           (unsigned int)TOUCH_FEEDBACK_MS,
           (unsigned int)baselineFNV,
           (unsigned int)overlayFNV);
}

void Esp32JunctionGameplayInputProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeGameplayInput_reset();
}

int Esp32JunctionGameplayInputProbe_isActive(void) {
    return probeState.active != 0U && probeState.failed == 0U;
}

void Esp32JunctionGameplayInputProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeGameplayInputState emptyIntent;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    LegacySnapshot legacyBefore;
    LegacySnapshot legacyAfter;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (probeState.failed) return;

    if (probeState.active) {
        if (probeState.feedback.active &&
            (int32_t)(nowMs() - probeState.feedback.expiresMs) >= 0) {
            (void)restoreFeedback(1);
        }
        return;
    }

    if (!Esp32JunctionGameplayHudProbe_isDone()) return;

    printf("\n=== Doom RPG ESP32-native invisible gameplay touch pad ===\n");
    printf("[NATIVEINPUTPROBE] CONTRACT calibrated CYD press -> logical 160x120 -> twelve invisible semantic zones: full 3x3 world keypad with PREV_WEAPON/BACK/NEXT_WEAPON on its bottom row plus MENU/PASS_TURN/AUTOMAP across the top HUD; one compact pointer-free pending owner, unsupported/busy fail closed; feedback is 120ms allocation-free row-coded neon double-ring + action-specific vector glyph and records/restores the exact current gameplay-frame FNV so the same input layer remains valid after native view changes. This layer itself performs no movement, turn, select, weapon, menu, automap, pass-turn, entity or world mutation.\n");

    heapBefore = heap8();
    largestBefore = largest8();
    if (!runtimeBoundary(doomRpg) ||
        frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV ||
        !pureHitTestContract() ||
        !legacySnapshot(doomRpg, &legacyBefore) ||
        !EspMapResidentLifecycle_capture(&residentBefore)) {
        printf("[NATIVEINPUTPROBE] FAILED predecessor frame=%08x hud=%08x boundary=%d hitContract=%d entities=%d monsters=%d pack=%d\n",
               (unsigned int)frameFNV(),
               (unsigned int)(EspNativeGameplayHud_view() != NULL ?
                   fnv1a(EspNativeGameplayHud_view(), sizeof(EspNativeGameplayHudState)) : 0U),
               runtimeBoundary(doomRpg), pureHitTestContract(),
               doomRpg && doomRpg->game ? doomRpg->game->numEntities : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numMonsters : -1,
               EspAssetPack_isOpen());
        probeState.failed = 1U;
        return;
    }

    EspNativeGameplayInput_reset();
    memset(&emptyIntent, 0xa5, sizeof(emptyIntent));
    if (EspNativeGameplayInput_consume(&emptyIntent) !=
            ESP_NATIVE_GAMEPLAY_INPUT_EMPTY ||
        fnv1a(&emptyIntent, sizeof(emptyIntent)) !=
            fnv1a(&(EspNativeGameplayInputState){0}, sizeof(emptyIntent)) ||
        EspNativeGameplayInput_peek()->pending ||
        !legacySnapshot(doomRpg, &legacyAfter) ||
        !legacySnapshotEqual(&legacyBefore, &legacyAfter) ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV) {
        printf("[NATIVEINPUTPROBE] FAILED owner empty/atomic gate frame=%08x pending=%u legacyStable=%d residentStable=%d\n",
               (unsigned int)frameFNV(),
               (unsigned int)EspNativeGameplayInput_peek()->pending,
               legacySnapshotEqual(&legacyBefore, &legacyAfter),
               memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) == 0);
        probeState.failed = 1U;
        return;
    }

    probeState.doomRpg = doomRpg;
    probeState.active = 1U;
    PlatformInput_setTapCallback(onGameplayTap);

    heapAfter = heap8();
    largestAfter = largest8();
    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV ||
        !runtimeBoundary(doomRpg)) {
        printf("[NATIVEINPUTPROBE] FAILED activation heap=%u->%u largest=%u->%u frame=%08x boundary=%d\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameFNV(), runtimeBoundary(doomRpg));
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    printZone(16, 10);
    printZone(80, 10);
    printZone(144, 10);
    printZone(26, 32);
    printZone(79, 32);
    printZone(132, 32);
    printZone(26, 59);
    printZone(79, 59);
    printZone(132, 59);
    printZone(26, 86);
    printZone(79, 86);
    printZone(132, 86);

    printf("[NATIVEINPUTPROBE] READY zones=12 stateBytes=%u hitBytes=%u feedbackBytes=%u maxEdits=%u initialBaseline=%08x dynamicRestore=yes touch=physical-calibrated/x2 logical releaseDebounce=50ms feedback=%ums palette=BLUE/GREEN/YELLOW/RED style=neon-double-ring+vector-glyph heap=%u->%u largest=%u->%u dispatch=observer gameplay=no\n",
           (unsigned int)sizeof(EspNativeGameplayInputState),
           (unsigned int)sizeof(EspNativeGameplayTouchHit),
           (unsigned int)sizeof(TouchFeedback),
           (unsigned int)TOUCH_FEEDBACK_MAX_EDITS,
           (unsigned int)EXPECTED_GAMEPLAY_FRAME_FNV,
           (unsigned int)TOUCH_FEEDBACK_MS,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[NATIVEINPUTPROBE] PARK tap all 12 zones plus bottom-HUD MISS; every valid tap must restore the exact frame it started from, including after native view changes.\n");
}
