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
#include "native_junction_move_collision_probe.h"
#include "native_junction_turn_dispatch_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXPECTED_BASE_FRAME_FNV 0xba3e5182U
#define EXPECTED_BASE_VIEWPORT_FNV 0x9206eb24U
#define EXPECTED_BASE_HUD_BANDS_FNV 0x6c2aa46fU
#define EXPECTED_INITIAL_HUD_FNV 0x4756db9cU
#define EXPECTED_INITIAL_VIEW_FNV 0xafcdcf74U
#define EXPECTED_RESIDENT_FNV 0xbb714d80U
#define CANONICAL_SPAWN_X 992
#define CANONICAL_SPAWN_Y 1888
#define MAP_MIN_CENTER 32
#define MAP_MAX_CENTER 2016

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
    EspNativeGameplayInputState restoredTurn;
    EspNativeGameplayInputState pendingIntent;
    uint32_t turns;
    uint32_t roundTrips;
    uint32_t observedTurns;
    uint32_t feedbackBaselineFNV;
    uint8_t initialized;
    uint8_t feedbackTurnPending;
    uint8_t pendingIntentValid;
    uint8_t failed;
} TurnProbeState;

static TurnProbeState probeState;
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

static int centeredCoordinate(int32_t value) {
    return value >= MAP_MIN_CENTER && value <= MAP_MAX_CENTER &&
           (value & 63) == 32;
}

static int atCanonicalSpawn(const EspPlayerViewState* view) {
    return view != NULL && view->viewX == CANONICAL_SPAWN_X &&
           view->viewY == CANONICAL_SPAWN_Y;
}

static int movementAction(uint8_t action) {
    return action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT;
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
           centeredCoordinate(view->viewX) && centeredCoordinate(view->viewY) &&
           view->viewZ == 36 && view->viewX == view->destX &&
           view->viewY == view->destY && view->viewAngle == view->destAngle &&
           view->viewAngle >= 0 && view->viewAngle <= 255 &&
           (view->viewAngle & 63) == 0 &&
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
           !EspAssetPack_isOpen();
}

static void failProbe(const char* reason) {
    printf("[TURNPROBE] FAILED %s frame=%08x pending=%u queued=%u feedbackBaseline=%08x pack=%d\n",
           reason, (unsigned int)frameFNV(),
           (unsigned int)EspNativeGameplayInput_peek()->pending,
           (unsigned int)probeState.pendingIntentValid,
           (unsigned int)probeState.feedbackBaselineFNV,
           EspAssetPack_isOpen());
    probeState.failed = 1U;
    probeState.feedbackTurnPending = 0U;
    probeState.pendingIntentValid = 0U;
    probeState.feedbackBaselineFNV = 0U;
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

    printf("[TURNSTACK] beforeRender highWater=%u execScratch=%uB pos=%d,%d angle=%u\n",
           stackHighWater(), (unsigned int)sizeof(executionWorkspace),
           (int)w->afterView.viewX, (int)w->afterView.viewY,
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
    printf("[TURN] PHASE angle=%u pos=%d,%d viewport=%08x->world:%08x->sprites:%08x hud=%08x->preserved:%08x->dir:%08x frame=%08x->%08x worldRouteNoPresent=%u finalPresent=%u\n",
           (unsigned int)w->result.angleAfter,
           (int)w->afterView.viewX, (int)w->afterView.viewY,
           (unsigned int)w->frameStats.viewportBeforeFNV,
           (unsigned int)w->frameStats.viewportAfterWorldFNV,
           (unsigned int)w->frameStats.viewportAfterSpritesFNV,
           (unsigned int)w->frameStats.hudBandsBeforeFNV,
           (unsigned int)w->frameStats.hudBandsRestoredFNV,
           (unsigned int)w->frameStats.hudBandsAfterFNV,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)w->frameStats.worldRouteNoPresent,
           (unsigned int)w->frameStats.finalPresented);

    if (!legacySnapshot(probeState.doomRpg, &w->legacyAfter) ||
        !legacyEqual(&w->legacyBefore, &w->legacyAfter) ||
        !EspMapResidentLifecycle_capture(&w->residentAfter) ||
        memcmp(&w->residentBefore, &w->residentAfter,
               sizeof(w->residentBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter == frameBefore || w->frameStats.frameAfterFNV != frameAfter ||
        w->frameStats.temporaryHudBytes != 0U ||
        w->frameStats.worldRouteNoPresent != 1U ||
        w->frameStats.finalPresented != 1U || EspAssetPack_isOpen()) {
        failProbe("post-turn integrity");
        return 0;
    }

    roundTrip = w->result.angleAfter == 64U && atCanonicalSpawn(&w->afterView);
    if (roundTrip &&
        w->frameStats.viewportAfterSpritesFNV != EXPECTED_BASE_VIEWPORT_FNV) {
        failProbe("round-trip viewport mismatch");
        return 0;
    }
    if (roundTrip &&
        w->frameStats.hudBandsAfterFNV != EXPECTED_BASE_HUD_BANDS_FNV) {
        failProbe("round-trip HUD mismatch");
        return 0;
    }
    if (roundTrip && frameAfter != EXPECTED_BASE_FRAME_FNV) {
        failProbe("round-trip frame mismatch");
        return 0;
    }
    if (atCanonicalSpawn(&w->afterView) &&
        w->result.angleAfter != 64U && frameAfter == EXPECTED_BASE_FRAME_FNV) {
        failProbe("turned frame unchanged");
        return 0;
    }

    ++probeState.turns;
    if (roundTrip) ++probeState.roundTrips;
    printf("[TURN] OK n=%u seq=%u action=%s delta=%d angle=%u->%u pos=%d,%d viewFNV=%08x->%08x orientFNV=%08x->%08x frame=%08x->%08x canonicalRoundTrip=%s\n",
           (unsigned int)probeState.turns,
           (unsigned int)w->result.sequence,
           EspNativeGameplayInput_actionName(w->result.action),
           (int)w->result.angleDelta,
           (unsigned int)w->result.angleBefore,
           (unsigned int)w->result.angleAfter,
           (int)w->afterView.viewX, (int)w->afterView.viewY,
           (unsigned int)fnv1a(&w->beforeView, sizeof(w->beforeView)),
           (unsigned int)fnv1a(EspPlayerView_view(), sizeof(EspPlayerViewState)),
           (unsigned int)fnv1a(&w->beforeTurn, sizeof(w->beforeTurn)),
           (unsigned int)fnv1a(EspNativeGameplayDispatch_view(),
                               sizeof(EspNativeGameplayTurnState)),
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           roundTrip ? "exact" : "no");
    printf("[TURN] RENDER world=%08x walls=%u/%u planes=%u sprites=%u/%u glows=%u/%u spriteReads=%u hudReads=%u hudPixels=%u tempHud=%uB routeNoPresent=%u final=%u timeUs=world:%u sprite:%u hud:%u present:%u total:%u heap=%u->%u largest=%u->%u stackHighWater=%u legacyStable=yes residentStable=yes turnAdvance=no tileDispatch=no facingRefresh=deferred\n",
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
           (unsigned int)w->frameStats.worldRouteNoPresent,
           (unsigned int)w->frameStats.finalPresented,
           (unsigned int)w->frameStats.worldMicros,
           (unsigned int)w->frameStats.spriteMicros,
           (unsigned int)w->frameStats.hudMicros,
           (unsigned int)w->frameStats.presentMicros,
           (unsigned int)w->frameStats.totalMicros,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           stackHighWater());
    return 1;
}

static int queueRestoredTurn(void) {
    const uint32_t baseline = probeState.feedbackBaselineFNV;
    if (!probeState.feedbackTurnPending || probeState.pendingIntentValid ||
        probeState.failed || baseline == 0U || frameFNV() != baseline) return 0;
    probeState.pendingIntent = probeState.restoredTurn;
    probeState.pendingIntentValid = 1U;
    memset(&probeState.restoredTurn, 0, sizeof(probeState.restoredTurn));
    probeState.feedbackTurnPending = 0U;
    probeState.feedbackBaselineFNV = 0U;
    printf("[TURNPROBE] QUEUED seq=%u action=%s baseline=%08x origin=restored-input-probe render=next-service\n",
           (unsigned int)probeState.pendingIntent.sequence,
           EspNativeGameplayInput_actionName(probeState.pendingIntent.action),
           (unsigned int)baseline);
    return 1;
}

void Esp32NativeGameplayInputProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) {
    uint32_t baseline;

    if (!probeState.initialized || probeState.failed || intent == NULL) return;
    if (probeState.feedbackTurnPending || probeState.pendingIntentValid) return;

    if (movementAction(intent->action)) {
        Esp32JunctionMoveCollisionProbe_observeConsumed(intent);
        return;
    }
    if (intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT &&
        intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT) return;

    baseline = frameFNV();
    if (baseline == 0U) return;
    probeState.restoredTurn = *intent;
    probeState.feedbackBaselineFNV = baseline;
    probeState.feedbackTurnPending = 1U;
    ++probeState.observedTurns;
    printf("[TURNPROBE] HANDOFF n=%u seq=%u action=%s baseline=%08x callbackOwner=input-probe waitingForFeedbackRestore=yes\n",
           (unsigned int)probeState.observedTurns,
           (unsigned int)intent->sequence,
           EspNativeGameplayInput_actionName(intent->action),
           (unsigned int)baseline);
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

        printf("\n=== Doom RPG ESP32-native gameplay turn dispatcher v8 viewport-hotpath ===\n");
        printf("[TURNPROBE] CONTRACT input remains sole touch owner; TURN_LEFT(+64)/TURN_RIGHT(-64) still waits for exact dynamic neon restore and renders only on a later service. Runtime world recomposition is now viewport-only: no full-frame clear, no temporary 12.8 KiB HUD save, and no historical intermediate world present. Canonical spawn hashes and all legacy/resident side-effect guards remain unchanged.\n");

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

        printf("[TURNPROBE] READY turnStateBytes=%u resultBytes=%u viewBytes=%u frameStatsBytes=%u execScratchBytes=%u angle=%u fixed=sin:%d cos:%d step:%d,%d initialFrame=%08x initialViewport=%08x initialHudBands=%08x dynamicRestore=yes position=tile-center-runtime heap=%u->%u largest=%u->%u stackHighWater=%u callbackOwner=input-probe renderFromCallback=no gameplayWorldPresent=none tempHud=0\n",
               (unsigned int)sizeof(EspNativeGameplayTurnState),
               (unsigned int)sizeof(EspNativeGameplayDispatchResult),
               (unsigned int)sizeof(EspPlayerViewState),
               (unsigned int)sizeof(EspNativeGameplayFrameStats),
               (unsigned int)sizeof(executionWorkspace),
               (unsigned int)turn->destAngle,
               turn->viewSin, turn->viewCos, turn->viewStepX, turn->viewStepY,
               (unsigned int)EXPECTED_BASE_FRAME_FNV,
               (unsigned int)EXPECTED_BASE_VIEWPORT_FNV,
               (unsigned int)EXPECTED_BASE_HUD_BANDS_FNV,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               stackHighWater());
        printf("[TURNPROBE] PARK TURN remains live at moved tile centers; action logs now expose world/sprite/HUD/present microseconds and require tempHud=0 plus viewport-only HUD preservation.\n");
        return;
    }

    if (probeState.feedbackTurnPending && !probeState.pendingIntentValid) {
        const uint32_t baseline = probeState.feedbackBaselineFNV;
        if (baseline == 0U) {
            failProbe("missing feedback baseline");
            return;
        }
        if (frameFNV() != baseline || EspNativeGameplayInput_peek()->pending) {
            return;
        }
        if (!queueRestoredTurn()) {
            failProbe("restored turn queue");
            return;
        }
        printf("[TURNPROBE] RESTORED frame=%08x exact=yes queued=yes lifecycleReturnBeforeRender=yes\n",
               (unsigned int)baseline);
        return;
    }

    if (probeState.pendingIntentValid) {
        EspNativeGameplayInputState intent = probeState.pendingIntent;
        memset(&probeState.pendingIntent, 0, sizeof(probeState.pendingIntent));
        probeState.pendingIntentValid = 0U;
        (void)executeConsumedIntent(&intent);
        return;
    }
}
