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
#define TOUCH_FEEDBACK_MS 250U
#define TOUCH_FEEDBACK_MAX_BORDER_PIXELS 160U
#define TOUCH_FEEDBACK_XOR 0xffffU

typedef struct LegacySnapshot_s {
    uint32_t hud;
    uint32_t player;
    uint32_t game;
    uint32_t canvas;
    uint32_t render;
} LegacySnapshot;

typedef struct TouchFeedback_s {
    uint16_t saved[TOUCH_FEEDBACK_MAX_BORDER_PIXELS];
    uint32_t expiresMs;
    uint16_t count;
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
    uint8_t active;
    uint8_t reserved;
} TouchFeedback;

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
        {144, 10, ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP, ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP, 128, 0, 159, 19},
        {26, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_LEFT, 0, 20, 52, 45},
        {79, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD, 53, 20, 105, 45},
        {132, 32, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_RIGHT, 106, 20, 159, 45},
        {26, 59, ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT, ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT, 0, 46, 52, 72},
        {79, 59, ESP_NATIVE_GAMEPLAY_ACTION_SELECT, ESP_NATIVE_GAMEPLAY_ZONE_SELECT, 53, 46, 105, 72},
        {132, 59, ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT, ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT, 106, 46, 159, 72},
        {26, 86, ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON, 0, 73, 52, 99},
        {79, 86, ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK, ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK, 53, 73, 105, 99},
        {132, 86, ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN, ESP_NATIVE_GAMEPLAY_ZONE_PASS_TURN, 106, 73, 159, 99}
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

    if (EspNativeGameplayInput_classify(80, 10, &hit) !=
            ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT ||
        EspNativeGameplayInput_classify(80, 110, &hit) !=
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

static int writeFeedbackPixel(uint16_t* framebuffer, int x, int y) {
    TouchFeedback* feedback = &probeState.feedback;
    uint16_t* pixel;
    if (feedback->count >= TOUCH_FEEDBACK_MAX_BORDER_PIXELS) return 0;
    pixel = framebuffer + y * DOOMRPG_LOGICAL_WIDTH + x;
    feedback->saved[feedback->count++] = *pixel;
    *pixel ^= TOUCH_FEEDBACK_XOR;
    return 1;
}

static int drawFeedback(const EspNativeGameplayTouchHit* hit) {
    TouchFeedback* feedback = &probeState.feedback;
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    int x;
    int y;

    if (hit == NULL || framebuffer == NULL || feedback->active) return 0;
    memset(feedback, 0, sizeof(*feedback));
    feedback->left = hit->left;
    feedback->top = hit->top;
    feedback->right = hit->right;
    feedback->bottom = hit->bottom;

    for (x = hit->left; x <= hit->right; ++x) {
        if (!writeFeedbackPixel(framebuffer, x, hit->top)) return 0;
    }
    if (hit->bottom != hit->top) {
        for (x = hit->left; x <= hit->right; ++x) {
            if (!writeFeedbackPixel(framebuffer, x, hit->bottom)) return 0;
        }
    }
    for (y = hit->top + 1; y < hit->bottom; ++y) {
        if (!writeFeedbackPixel(framebuffer, hit->left, y)) return 0;
        if (hit->right != hit->left &&
            !writeFeedbackPixel(framebuffer, hit->right, y)) return 0;
    }

    feedback->expiresMs = nowMs() + TOUCH_FEEDBACK_MS;
    feedback->active = 1U;
    return 1;
}

static int restoreFeedback(int present) {
    TouchFeedback* feedback = &probeState.feedback;
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint16_t index = 0U;
    int x;
    int y;
    int ok = 1;

    if (!feedback->active) return 1;
    if (framebuffer == NULL) return 0;

    heapBefore = heap8();
    largestBefore = largest8();

    for (x = feedback->left; x <= feedback->right; ++x) {
        framebuffer[feedback->top * DOOMRPG_LOGICAL_WIDTH + x] =
            feedback->saved[index++];
    }
    if (feedback->bottom != feedback->top) {
        for (x = feedback->left; x <= feedback->right; ++x) {
            framebuffer[feedback->bottom * DOOMRPG_LOGICAL_WIDTH + x] =
                feedback->saved[index++];
        }
    }
    for (y = feedback->top + 1; y < feedback->bottom; ++y) {
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + feedback->left] =
            feedback->saved[index++];
        if (feedback->right != feedback->left) {
            framebuffer[y * DOOMRPG_LOGICAL_WIDTH + feedback->right] =
                feedback->saved[index++];
        }
    }

    if (index != feedback->count || frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV) {
        ok = 0;
    }
    if (ok && present && !Esp32PlatformVideo_present()) ok = 0;

    heapAfter = heap8();
    largestAfter = largest8();
    if (heapAfter != heapBefore || largestAfter != largestBefore) ok = 0;

    if (ok) {
        ++probeState.restores;
        printf("[NATIVEINPUT] FEEDBACK RESTORE n=%u frame=%08x exact=yes heap=%u->%u largest=%u->%u\n",
               (unsigned int)probeState.restores,
               (unsigned int)EXPECTED_GAMEPLAY_FRAME_FNV,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter);
    }
    else {
        printf("[NATIVEINPUT] FAILED feedback restore pixels=%u/%u frame=%08x present=%d heap=%u->%u largest=%u->%u\n",
               (unsigned int)index, (unsigned int)feedback->count,
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
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
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

    if (!runtimeBoundary(probeState.doomRpg) ||
        frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV ||
        EspNativeGameplayInput_peek()->pending) {
        printf("[NATIVEINPUT] FAILED tap boundary n=%u frame=%08x pending=%u shapeData=%p mediaTexels=%p\n",
               (unsigned int)probeState.taps,
               (unsigned int)frameFNV(),
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

    printf("[NATIVEINPUT] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d status=%d\n",
           (unsigned int)probeState.taps,
           rawX, rawY, pressure, screenX, screenY,
           logicalX, logicalY, (int)classifyStatus);

    if (classifyStatus == ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT) {
        ++probeState.misses;
        if (hadFeedback) presentRestored = Esp32PlatformVideo_present();
        printf("[NATIVEINPUT] MISS n=%u logical=%d,%d bottomHUD/upper-center=unbound frame=%08x restoredPresent=%d gameplay=no\n",
               (unsigned int)probeState.misses,
               logicalX, logicalY,
               (unsigned int)frameFNV(), presentRestored);
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
        frameFNV() != EXPECTED_GAMEPLAY_FRAME_FNV ||
        !legacySnapshot(probeState.doomRpg, &legacyAfter) ||
        !legacySnapshotEqual(&legacyBefore, &legacyAfter) ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg)) {
        printf("[NATIVEINPUT] FAILED semantic route action=%u zone=%u sequence=%u frame=%08x pending=%u legacyStable=%d\n",
               (unsigned int)hit.action, (unsigned int)hit.zone,
               (unsigned int)consumed.sequence,
               (unsigned int)frameFNV(),
               (unsigned int)EspNativeGameplayInput_peek()->pending,
               legacySnapshotEqual(&legacyBefore, &legacyAfter));
        probeState.failed = 1U;
        probeState.active = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    if (!drawFeedback(&hit) || !Esp32PlatformVideo_present()) {
        printf("[NATIVEINPUT] FAILED feedback draw action=%s zone=%u pixels=%u frame=%08x\n",
               EspNativeGameplayInput_actionName(hit.action),
               (unsigned int)hit.zone,
               (unsigned int)probeState.feedback.count,
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
    if (overlayFNV == EXPECTED_GAMEPLAY_FRAME_FNV ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        !runtimeBoundary(probeState.doomRpg) || EspAssetPack_isOpen()) {
        printf("[NATIVEINPUT] FAILED feedback invariant frame=%08x heap=%u->%u largest=%u->%u pack=%d\n",
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

    ++probeState.intents;
    printf("[NATIVEINPUT] INTENT n=%u seq=%u action=%s id=%u zone=%u logical=%u,%u consumedBy=probe dispatch=deferred gameplay=no\n",
           (unsigned int)probeState.intents,
           (unsigned int)consumed.sequence,
           EspNativeGameplayInput_actionName(consumed.action),
           (unsigned int)consumed.action,
           (unsigned int)consumed.zone,
           (unsigned int)consumed.logicalX,
           (unsigned int)consumed.logicalY);
    printf("[NATIVEINPUT] FEEDBACK zone=%u logical=x%u..%u y%u..%u physical=x%u..%u y%u..%u borderPixels=%u hold=%ums frame=%08x->%08x heapDelta=0 largestDelta=0\n",
           (unsigned int)hit.zone,
           (unsigned int)hit.left, (unsigned int)hit.right,
           (unsigned int)hit.top, (unsigned int)hit.bottom,
           (unsigned int)hit.left * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.right + 1U) * DOOMRPG_INTEGER_SCALE) - 1U,
           (unsigned int)hit.top * DOOMRPG_INTEGER_SCALE,
           (((unsigned int)hit.bottom + 1U) * DOOMRPG_INTEGER_SCALE) - 1U,
           (unsigned int)probeState.feedback.count,
           (unsigned int)TOUCH_FEEDBACK_MS,
           (unsigned int)EXPECTED_GAMEPLAY_FRAME_FNV,
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
    printf("[NATIVEINPUTPROBE] CONTRACT calibrated CYD press -> logical 160x120 -> invisible Doom RPG keypad intent; 3x3 only on world viewport, MENU/AUTOMAP on top bar corners, bottom HUD and top center unbound; one compact pointer-free pending owner, unsupported/busy fail closed; this milestone consumes every intent in the probe and performs NO movement, turn, select, weapon, menu, automap, entity or world mutation; feedback is a 250ms allocation-free exact-restoring border overlay with on-demand presentation only\n");

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

    printf("[NATIVEINPUTPROBE] READY stateBytes=%u hitBytes=%u feedbackBytes=%u baseline=%08x touch=physical-calibrated/x2 logical releaseDebounce=50ms feedback=%ums heap=%u->%u largest=%u->%u dispatch=deferred gameplay=no\n",
           (unsigned int)sizeof(EspNativeGameplayInputState),
           (unsigned int)sizeof(EspNativeGameplayTouchHit),
           (unsigned int)sizeof(TouchFeedback),
           (unsigned int)EXPECTED_GAMEPLAY_FRAME_FNV,
           (unsigned int)TOUCH_FEEDBACK_MS,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[NATIVEINPUTPROBE] PARK tap all 11 zones; each valid tap must log INTENT + transient FEEDBACK then exact RESTORE; no action is executed yet\n");
}
