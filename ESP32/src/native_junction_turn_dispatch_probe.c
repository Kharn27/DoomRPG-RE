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
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_input.h"
#include "esp_player_view_state.h"
#include "native_junction_gameplay_input_probe.h"
#include "native_junction_turn_dispatch_probe.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXPECTED_BASE_FRAME_FNV 0xba3e5182U
#define EXPECTED_INITIAL_HUD_FNV 0x4756db9cU
#define EXPECTED_INITIAL_VIEW_FNV 0xafcdcf74U
#define EXPECTED_RESIDENT_FNV 0xbb714d80U

typedef struct LegacySnapshot_s {
    uint32_t hud;
    uint32_t player;
    uint32_t game;
    uint32_t canvas;
    uint32_t render;
} LegacySnapshot;

typedef struct TurnExecutionWorkspace_s {
    EspPlayerViewState beforeView;
    EspPlayerViewState afterView;
    EspNativeGameplayTurnState beforeTurn;
    EspNativeGameplayTurnState afterTurn;
    EspNativeGameplayDispatchResult result;
    EspNativeGameplayFrameStats frameStats;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    LegacySnapshot legacyBefore;
    LegacySnapshot legacyAfter;
} TurnExecutionWorkspace;

typedef struct TurnProbeState_s {
    DoomRPG_t* doomRpg;
    EspNativeGameplayInputState firstTurn;
    EspNativeGameplayInputState pendingIntent;
    uint32_t taps;
    uint32_t turns;
    uint32_t deferred;
    uint32_t roundTrips;
    uint8_t initialized;
    uint8_t firstTurnPending;
    uint8_t pendingIntentValid;
    uint8_t callbackOwned;
    uint8_t failed;
    uint8_t reserved[3];
} TurnProbeState;

static TurnProbeState probeState;
/* Temporary probe-only static workspace. Keeping the snapshot/result graph out
 * of loopTask stack is intentional: the reusable first-frame renderer already
 * has its own bounded stack workspace and gameplay TURN must not deepen it just
 * because input was serviced immediately beforehand. */
static TurnExecutionWorkspace executionWorkspace;

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

static unsigned int stackHighWater(void) {
    return (unsigned int)uxTaskGetStackHighWaterMark(NULL);
}

static int legacySnapshot(const DoomRPG_t* doomRpg, LegacySnapshot* out) {
    if (doomRpg == NULL || out == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->render == NULL) return 0;
    out->hud = fnv1a(doomRpg->hud, sizeof(*doomRpg->hud));
    out->player = fnv1a(doomRpg->player, sizeof(*doomRpg->player));
    out->game = fnv1a(doomRpg->game, sizeof(*doomRpg->game));
    out->canvas = fnv1a(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    out->render = fnv1a(doomRpg->render, sizeof(*doomRpg->render));
    return 1;
}

static int legacyEqual(const LegacySnapshot* a, const LegacySnapshot* b) {
    return a != NULL && b != NULL && memcmp(a, b, sizeof(*a)) == 0;
}

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           fnv1a(s, sizeof(*s)) == EXPECTED_RESIDENT_FNV &&
           s->totalPayloadBytes == 10410U && s->entityCount == 30U &&
           s->enemyCount == 0U && s->destructibleCount == 3U;
}

static int viewRuntimeValid(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == 44U &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewX == view->destX && view->viewY == view->destY &&
           view->viewAngle == view->destAngle && view->viewAngle >= 0 &&
           view->viewAngle <= 255 && (view->viewAngle & 63) == 0 &&
           view->targetMapId == 9U && view->gameplayLoadMapId == 2U &&
           view->loadType == 0U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 0U && view->playerSetupPending == 0U &&
           view->tileEnterPending == 0U && view->active == 1U;
}

static int turnStateMatchesView(const EspNativeGameplayTurnState* turn,
                                const EspPlayerViewState* view) {
    if (turn == NULL || view == NULL || sizeof(*turn) != 24U ||
        turn->active != 1U || turn->destAngle != (uint8_t)view->viewAngle) return 0;
    switch (turn->destAngle) {
    case 0U:
        return turn->viewSin == 0 && turn->viewCos == 65536 &&
               turn->viewStepX == 64 && turn->viewStepY == 0;
    case 64U:
        return turn->viewSin == 65536 && turn->viewCos == 0 &&
               turn->viewStepX == 0 && turn->viewStepY == -64;
    case 128U:
        return turn->viewSin == 0 && turn->viewCos == -65536 &&
               turn->viewStepX == -64 && turn->viewStepY == 0;
    case 192U:
        return turn->viewSin == -65536 && turn->viewCos == 0 &&
               turn->viewStepX == 0 && turn->viewStepY == 64;
    default:
        return 0;
    }
}

static int runtimeBoundary(const DoomRPG_t* doomRpg) {
    const EspPlayerViewState* view;
    const EspNativeGameplayTurnState* turn;
    const EspNativeGameplayHudState* hud;
    EspMapResidentSnapshot resident;
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL)
        return 0;
    view = EspPlayerView_view();
    turn = EspNativeGameplayDispatch_view();
    hud = EspNativeGameplayHud_view();
    return doomRpg->doomCanvas->state == ST_INTRO &&
           doomRpg->doomCanvas->storyPage == 3 &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0 &&
           doomRpg->render->framebuffer == Esp32PlatformVideo_framebuffer() &&
           doomRpg->render->screenX == 0 && doomRpg->render->screenY == 20 &&
           doomRpg->render->screenWidth == 160 && doomRpg->render->screenHeight == 80 &&
           doomRpg->render->shapeData == NULL && doomRpg->render->mediaTexels == NULL &&
           viewRuntimeValid(view) && turnStateMatchesView(turn, view) &&
           hud != NULL && fnv1a(hud, sizeof(*hud)) == EXPECTED_INITIAL_HUD_FNV &&
           EspMapResidentLifecycle_capture(&resident) && residentCanonical(&resident) &&
           !EspAssetPack_isOpen() &&
           (view->viewAngle != 64 || frameFNV() == EXPECTED_BASE_FRAME_FNV);
}

static void failProbe(const char* reason) {
    printf("[TURNPROBE] FAILED %s frame=%08x pending=%u queued=%u pack=%d callbackOwned=%u\n",
           reason, (unsigned int)frameFNV(),
           (unsigned int)EspNativeGameplayInput_peek()->pending,
           (unsigned int)probeState.pendingIntentValid,
           EspAssetPack_isOpen(), (unsigned int)probeState.callbackOwned);
    probeState.failed = 1U;
    probeState.firstTurnPending = 0U;
    probeState.pendingIntentValid = 0U;
    if (probeState.callbackOwned) {
        PlatformInput_setTapCallback(NULL);
        probeState.callbackOwned = 0U;
    }
}

static int executeConsumedIntent(const EspNativeGameplayInputState* intent) {
    TurnExecutionWorkspace* w = &executionWorkspace;
    EspNativeGameplayDispatchStatus dispatch;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    int roundTrip;

    memset(w, 0, sizeof(*w));
    if (intent == NULL || probeState.doomRpg == NULL ||
        !runtimeBoundary(probeState.doomRpg)) {
        failProbe("execute boundary");
        return 0;
    }

    dispatch = EspNativeGameplayDispatch_prepareTurn(
        intent, &w->beforeView, &w->afterView,
        &w->beforeTurn, &w->afterTurn, &w->result);
    if (dispatch == ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED) {
        ++probeState.deferred;
        printf("[TURNPROBE] DEFERRED n=%u seq=%u action=%s id=%u view=%u frame=%08x gameplay=no\n",
               (unsigned int)probeState.deferred,
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)intent->action,
               (unsigned int)EspPlayerView_view()->viewAngle,
               (unsigned int)frameFNV());
        return 1;
    }
    if (dispatch != ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED) {
        failProbe("dispatch prepare");
        return 0;
    }

    heapBefore = heap8();
    largestBefore = largest8();
    frameBefore = frameFNV();
    if (!legacySnapshot(probeState.doomRpg, &w->legacyBefore) ||
        !EspMapResidentLifecycle_capture(&w->residentBefore) ||
        !residentCanonical(&w->residentBefore)) {
        failProbe("pre-turn snapshots");
        return 0;
    }

    if (EspNativeGameplayDispatch_commitTurn(
            &w->beforeView, &w->afterView,
            &w->beforeTurn, &w->afterTurn, &w->result) !=
            ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        failProbe("dispatch commit");
        return 0;
    }

    printf("[TURNSTACK] beforeRender highWater=%u workspace=%uB angle=%u\n",
           stackHighWater(), (unsigned int)sizeof(executionWorkspace),
           (unsigned int)w->result.angleAfter);
    if (!EspNativeGameplayFrame_renderTurn(
            probeState.doomRpg->render, w->result.angleAfter, &w->frameStats)) {
        (void)EspNativeGameplayDispatch_rollbackTurn(
            &w->afterView, &w->beforeView,
            &w->afterTurn, &w->beforeTurn, &w->result);
        failProbe("native frame render");
        return 0;
    }

    frameAfter = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (!legacySnapshot(probeState.doomRpg, &w->legacyAfter) ||
        !legacyEqual(&w->legacyBefore, &w->legacyAfter) ||
        !EspMapResidentLifecycle_capture(&w->residentAfter) ||
        memcmp(&w->residentBefore, &w->residentAfter,
               sizeof(w->residentBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter == frameBefore || w->frameStats.frameAfterFNV != frameAfter ||
        w->frameStats.temporaryHudBytes != 12800U ||
        w->frameStats.finalPresented != 1U || EspAssetPack_isOpen()) {
        failProbe("post-turn integrity");
        return 0;
    }

    roundTrip = w->result.angleAfter == 64U;
    if (roundTrip && frameAfter != EXPECTED_BASE_FRAME_FNV) {
        failProbe("round-trip frame mismatch");
        return 0;
    }
    if (!roundTrip && frameAfter == EXPECTED_BASE_FRAME_FNV) {
        failProbe("turned frame unchanged");
        return 0;
    }

    ++probeState.turns;
    if (roundTrip) ++probeState.roundTrips;
    printf("[TURN] OK n=%u seq=%u action=%s delta=%d angle=%u->%u viewFNV=%08x->%08x orientFNV=%08x->%08x frame=%08x->%08x roundTrip=%s\n",
           (unsigned int)probeState.turns,
           (unsigned int)w->result.sequence,
           EspNativeGameplayInput_actionName(w->result.action),
           (int)w->result.angleDelta,
           (unsigned int)w->result.angleBefore,
           (unsigned int)w->result.angleAfter,
           (unsigned int)fnv1a(&w->beforeView, sizeof(w->beforeView)),
           (unsigned int)fnv1a(EspPlayerView_view(), sizeof(EspPlayerViewState)),
           (unsigned int)fnv1a(&w->beforeTurn, sizeof(w->beforeTurn)),
           (unsigned int)fnv1a(EspNativeGameplayDispatch_view(),
                               sizeof(EspNativeGameplayTurnState)),
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           roundTrip ? "exact" : "no");
    printf("[TURN] RENDER world=%08x walls=%u/%u planes=%u sprites=%u/%u glows=%u/%u spriteReads=%u hudReads=%u hudPixels=%u tempHud=%uB presented=%u+%u heap=%u->%u largest=%u->%u stackHighWater=%u legacyStable=yes residentStable=yes turnAdvance=no tileDispatch=no facingRefresh=deferred\n",
           (unsigned int)w->frameStats.worldFrameFNV,
           (unsigned int)w->frameStats.wallDraws,
           (unsigned int)w->frameStats.wallPixels,
           (unsigned int)w->frameStats.planePixels,
           (unsigned int)w->frameStats.spriteDraws,
           (unsigned int)w->frameStats.spritePixels,
           (unsigned int)w->frameStats.glowDraws,
           (unsigned int)w->frameStats.glowPixels,
           (unsigned int)w->frameStats.spritePackReads,
           (unsigned int)w->frameStats.hudPackReads,
           (unsigned int)w->frameStats.hudPixels,
           (unsigned int)w->frameStats.temporaryHudBytes,
           (unsigned int)w->frameStats.worldPresented,
           (unsigned int)w->frameStats.finalPresented,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           stackHighWater());
    return 1;
}

static int queueIntent(const EspNativeGameplayInputState* intent,
                       const char* origin) {
    if (intent == NULL || probeState.pendingIntentValid || probeState.failed) {
        return 0;
    }
    probeState.pendingIntent = *intent;
    probeState.pendingIntentValid = 1U;
    printf("[TURNPROBE] QUEUED seq=%u action=%s origin=%s render=next-service\n",
           (unsigned int)intent->sequence,
           EspNativeGameplayInput_actionName(intent->action),
           origin != NULL ? origin : "unknown");
    return 1;
}

static void onTurnTap(int16_t screenX,
                      int16_t screenY,
                      uint16_t pressure,
                      uint16_t rawX,
                      uint16_t rawY) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState intent;
    EspNativeGameplayInputStatus classify;
    int logicalX;
    int logicalY;

    if (!probeState.callbackOwned || probeState.failed || probeState.doomRpg == NULL)
        return;

    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    ++probeState.taps;
    classify = EspNativeGameplayInput_classify(logicalX, logicalY, &hit);
    printf("[TURNPROBE] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d status=%d\n",
           (unsigned int)probeState.taps, rawX, rawY, pressure,
           screenX, screenY, logicalX, logicalY, (int)classify);

    if (classify == ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT) {
        printf("[TURNPROBE] MISS logical=%d,%d gameplay=no\n", logicalX, logicalY);
        return;
    }
    if (classify != ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        probeState.pendingIntentValid || EspNativeGameplayInput_peek()->pending ||
        !runtimeBoundary(probeState.doomRpg)) {
        failProbe("tap boundary/classify/busy");
        return;
    }
    if (EspNativeGameplayInput_route(&hit, logicalX, logicalY) !=
            ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        EspNativeGameplayInput_consume(&intent) != ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        EspNativeGameplayInput_peek()->pending ||
        !queueIntent(&intent, "turn-callback")) {
        failProbe("input route/consume/queue");
        return;
    }
}

/* Called only by the temporary weak observation point in
 * EspNativeGameplayInput_consume(). Before the first real turn, the proven
 * input probe remains the actual touch callback owner and still performs its
 * 250 ms exact-restoring feedback. We copy one TURN intent and wait for that
 * feedback to restore the canonical framebuffer before queueing execution. */
void Esp32NativeGameplayInputProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) {
    if (!probeState.initialized || probeState.failed || probeState.callbackOwned ||
        probeState.firstTurnPending || probeState.pendingIntentValid ||
        intent == NULL) return;
    if (intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT &&
        intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT) return;
    probeState.firstTurn = *intent;
    probeState.firstTurnPending = 1U;
    printf("[TURNPROBE] HANDOFF seq=%u action=%s waitingForInputFeedbackRestore=yes\n",
           (unsigned int)intent->sequence,
           EspNativeGameplayInput_actionName(intent->action));
}

void Esp32JunctionTurnDispatchProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    memset(&executionWorkspace, 0, sizeof(executionWorkspace));
    EspNativeGameplayDispatch_reset();
}

int Esp32JunctionTurnDispatchProbe_isActive(void) {
    return probeState.initialized != 0U && probeState.failed == 0U;
}

void Esp32JunctionTurnDispatchProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    const EspNativeGameplayTurnState* turn;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (probeState.failed) return;

    if (!probeState.initialized) {
        if (!Esp32JunctionGameplayInputProbe_isActive()) return;

        printf("\n=== Doom RPG ESP32-native first gameplay turn dispatcher v3 ===\n");
        printf("[TURNPROBE] CONTRACT input callbacks never render. The hardware-proven gameplay input probe owns the first TURN through neon feedback + exact restore; the restored intent is queued, the lifecycle returns once, and only a later service executes TURN_LEFT(+64)/TURN_RIGHT(-64). After takeover, touch callbacks still only enqueue semantic intents. Heavy probe snapshots live in static probe scratch, not loopTask stack. Other actions remain DEFERRED; no Game_advanceTurn, Game_executeTile, legacy finishRotation, entities/monsters/world mutation or facing refresh.\n");

        heapBefore = heap8();
        largestBefore = largest8();
        view = EspPlayerView_view();
        if (doomRpg == NULL || view == NULL ||
            fnv1a(view, sizeof(*view)) != EXPECTED_INITIAL_VIEW_FNV ||
            frameFNV() != EXPECTED_BASE_FRAME_FNV ||
            EspNativeGameplayDispatch_adoptView() != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
            failProbe("activation predecessor/adopt");
            return;
        }
        turn = EspNativeGameplayDispatch_view();
        if (!turnStateMatchesView(turn, view) || turn->sequence != 0U ||
            turn->lastAction != ESP_NATIVE_GAMEPLAY_ACTION_NONE ||
            !runtimeBoundary(doomRpg)) {
            failProbe("activation turn owner");
            return;
        }

        probeState.doomRpg = doomRpg;
        probeState.initialized = 1U;
        heapAfter = heap8();
        largestAfter = largest8();
        if (heapAfter != heapBefore || largestAfter != largestBefore ||
            !runtimeBoundary(doomRpg)) {
            failProbe("activation heap/boundary");
            return;
        }

        printf("[TURNPROBE] READY turnStateBytes=%u resultBytes=%u viewBytes=%u execScratchBytes=%u angle=%u fixed=sin:%d cos:%d step:%d,%d baseline=%08x heap=%u->%u largest=%u->%u stackHighWater=%u callbackOwner=input-probe renderFromCallback=no\n",
               (unsigned int)sizeof(EspNativeGameplayTurnState),
               (unsigned int)sizeof(EspNativeGameplayDispatchResult),
               (unsigned int)sizeof(EspPlayerViewState),
               (unsigned int)sizeof(executionWorkspace),
               (unsigned int)turn->destAngle,
               turn->viewSin, turn->viewCos, turn->viewStepX, turn->viewStepY,
               (unsigned int)EXPECTED_BASE_FRAME_FNV,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               stackHighWater());
        printf("[TURNPROBE] PARK first TURN => proven neon feedback => exact restore => queue => one full lifecycle return => native render on later service.\n");
        return;
    }

    if (!probeState.callbackOwned && probeState.firstTurnPending &&
        !probeState.pendingIntentValid) {
        /* The input probe owns its feedback until canonical restore. Once it is
         * restored, queue the intent and RETURN. This deliberately unwinds the
         * complete lifecycle stack before any renderer is entered. */
        if (frameFNV() != EXPECTED_BASE_FRAME_FNV ||
            EspNativeGameplayInput_peek()->pending) {
            return;
        }
        if (!queueIntent(&probeState.firstTurn, "restored-input-probe")) {
            failProbe("first turn queue");
            return;
        }
        memset(&probeState.firstTurn, 0, sizeof(probeState.firstTurn));
        probeState.firstTurnPending = 0U;
        printf("[TURNPROBE] RESTORED frame=%08x queued=yes lifecycleReturnBeforeRender=yes\n",
               (unsigned int)EXPECTED_BASE_FRAME_FNV);
        return;
    }

    if (probeState.pendingIntentValid) {
        EspNativeGameplayInputState intent = probeState.pendingIntent;
        const int firstTakeover = !probeState.callbackOwned;
        /* Clear the queue before execution so fail paths cannot replay it. */
        memset(&probeState.pendingIntent, 0, sizeof(probeState.pendingIntent));
        probeState.pendingIntentValid = 0U;
        if (!executeConsumedIntent(&intent)) return;
        if (firstTakeover) {
            PlatformInput_setTapCallback(onTurnTap);
            probeState.callbackOwned = 1U;
            printf("[TURNPROBE] TAKEOVER callbackOwner=turn-probe afterFirstTurn=yes frame=%08x angle=%u renderFromCallback=no\n",
                   (unsigned int)frameFNV(),
                   (unsigned int)EspPlayerView_view()->viewAngle);
        }
        return;
    }
}
