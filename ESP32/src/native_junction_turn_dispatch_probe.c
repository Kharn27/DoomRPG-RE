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

typedef struct TurnProbeState_s {
    DoomRPG_t* doomRpg;
    uint32_t taps;
    uint32_t turns;
    uint32_t deferred;
    uint32_t roundTrips;
    uint8_t active;
    uint8_t failed;
    uint8_t reserved[2];
} TurnProbeState;

static TurnProbeState probeState;

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

static void failAndDetach(const char* reason) {
    printf("[TURNPROBE] FAILED %s frame=%08x pending=%u pack=%d\n",
           reason, (unsigned int)frameFNV(),
           (unsigned int)EspNativeGameplayInput_peek()->pending,
           EspAssetPack_isOpen());
    probeState.failed = 1U;
    probeState.active = 0U;
    PlatformInput_setTapCallback(NULL);
}

static void onTurnTap(int16_t screenX,
                      int16_t screenY,
                      uint16_t pressure,
                      uint16_t rawX,
                      uint16_t rawY) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState intent;
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
    EspNativeGameplayInputStatus classify;
    EspNativeGameplayDispatchStatus dispatch;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    int logicalX;
    int logicalY;
    int roundTrip;

    if (!probeState.active || probeState.failed || probeState.doomRpg == NULL) return;
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
        EspNativeGameplayInput_peek()->pending || !runtimeBoundary(probeState.doomRpg)) {
        failAndDetach("tap boundary/classify");
        return;
    }
    if (EspNativeGameplayInput_route(&hit, logicalX, logicalY) !=
            ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        EspNativeGameplayInput_consume(&intent) != ESP_NATIVE_GAMEPLAY_INPUT_OK ||
        EspNativeGameplayInput_peek()->pending) {
        failAndDetach("input route/consume");
        return;
    }

    dispatch = EspNativeGameplayDispatch_prepareTurn(
        &intent, &beforeView, &afterView, &beforeTurn, &afterTurn, &result);
    if (dispatch == ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED) {
        ++probeState.deferred;
        printf("[TURNPROBE] DEFERRED n=%u seq=%u action=%s id=%u supportedFamily=TURN_LEFT/TURN_RIGHT view=%u frame=%08x gameplay=no\n",
               (unsigned int)probeState.deferred,
               (unsigned int)intent.sequence,
               EspNativeGameplayInput_actionName(intent.action),
               (unsigned int)intent.action,
               (unsigned int)EspPlayerView_view()->viewAngle,
               (unsigned int)frameFNV());
        return;
    }
    if (dispatch != ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED) {
        failAndDetach("dispatch prepare");
        return;
    }

    heapBefore = heap8();
    largestBefore = largest8();
    frameBefore = frameFNV();
    if (!legacySnapshot(probeState.doomRpg, &legacyBefore) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        failAndDetach("pre-turn snapshots");
        return;
    }

    if (EspNativeGameplayDispatch_commitTurn(
            &beforeView, &afterView, &beforeTurn, &afterTurn, &result) !=
            ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        failAndDetach("dispatch commit");
        return;
    }

    memset(&frameStats, 0, sizeof(frameStats));
    if (!EspNativeGameplayFrame_renderTurn(
            probeState.doomRpg->render, result.angleAfter, &frameStats)) {
        (void)EspNativeGameplayDispatch_rollbackTurn(
            &afterView, &beforeView, &afterTurn, &beforeTurn, &result);
        failAndDetach("native frame render");
        return;
    }

    frameAfter = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (!legacySnapshot(probeState.doomRpg, &legacyAfter) ||
        !legacyEqual(&legacyBefore, &legacyAfter) ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter == frameBefore || frameStats.frameAfterFNV != frameAfter ||
        frameStats.temporaryHudBytes != 12800U ||
        frameStats.finalPresented != 1U || EspAssetPack_isOpen()) {
        failAndDetach("post-turn integrity");
        return;
    }

    roundTrip = result.angleAfter == 64U;
    if (roundTrip && frameAfter != EXPECTED_BASE_FRAME_FNV) {
        failAndDetach("round-trip frame mismatch");
        return;
    }
    if (!roundTrip && frameAfter == EXPECTED_BASE_FRAME_FNV) {
        failAndDetach("turned frame unchanged");
        return;
    }

    ++probeState.turns;
    if (roundTrip) ++probeState.roundTrips;
    printf("[TURN] OK n=%u seq=%u action=%s delta=%d angle=%u->%u viewFNV=%08x->%08x orientFNV=%08x->%08x frame=%08x->%08x roundTrip=%s\n",
           (unsigned int)probeState.turns,
           (unsigned int)result.sequence,
           EspNativeGameplayInput_actionName(result.action),
           (int)result.angleDelta,
           (unsigned int)result.angleBefore,
           (unsigned int)result.angleAfter,
           (unsigned int)fnv1a(&beforeView, sizeof(beforeView)),
           (unsigned int)fnv1a(EspPlayerView_view(), sizeof(EspPlayerViewState)),
           (unsigned int)fnv1a(&beforeTurn, sizeof(beforeTurn)),
           (unsigned int)fnv1a(EspNativeGameplayDispatch_view(), sizeof(EspNativeGameplayTurnState)),
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           roundTrip ? "exact" : "no");
    printf("[TURN] RENDER world=%08x walls=%u/%u planes=%u sprites=%u/%u glows=%u/%u spriteReads=%u hudReads=%u hudPixels=%u tempHud=%uB presented=%u+%u heap=%u->%u largest=%u->%u legacyStable=yes residentStable=yes turnAdvance=no tileDispatch=no facingRefresh=deferred\n",
           (unsigned int)frameStats.worldFrameFNV,
           (unsigned int)frameStats.wallDraws,
           (unsigned int)frameStats.wallPixels,
           (unsigned int)frameStats.planePixels,
           (unsigned int)frameStats.spriteDraws,
           (unsigned int)frameStats.spritePixels,
           (unsigned int)frameStats.glowDraws,
           (unsigned int)frameStats.glowPixels,
           (unsigned int)frameStats.spritePackReads,
           (unsigned int)frameStats.hudPackReads,
           (unsigned int)frameStats.hudPixels,
           (unsigned int)frameStats.temporaryHudBytes,
           (unsigned int)frameStats.worldPresented,
           (unsigned int)frameStats.finalPresented,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
}

void Esp32JunctionTurnDispatchProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeGameplayDispatch_reset();
}

int Esp32JunctionTurnDispatchProbe_isActive(void) {
    return probeState.active != 0U && probeState.failed == 0U;
}

void Esp32JunctionTurnDispatchProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    const EspNativeGameplayTurnState* turn;
    EspNativeGameplayInputState fake;
    EspPlayerViewState beforeView;
    EspPlayerViewState afterView;
    EspNativeGameplayTurnState beforeTurn;
    EspNativeGameplayTurnState afterTurn;
    EspNativeGameplayDispatchResult result;
    EspPlayerViewState viewCopy;
    EspNativeGameplayTurnState turnCopy;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (probeState.failed || probeState.active) return;
    if (!Esp32JunctionGameplayInputProbe_isActive()) return;

    printf("\n=== Doom RPG ESP32-native first gameplay turn dispatcher ===\n");
    printf("[TURNPROBE] CONTRACT consume the existing native touch intent owner but execute ONLY TURN_LEFT(+64) / TURN_RIGHT(-64); publish one settled cardinal EspPlayerViewState plus one 24B runtime fixed-point turn owner, recompose walls+textured planes+sprites+glows+HUD compass and present; every other known action remains DEFERRED. No Game_advanceTurn, Game_executeTile, legacy finishRotation, entities/monsters/world mutation or facing refresh.\n");

    heapBefore = heap8();
    largestBefore = largest8();
    view = EspPlayerView_view();
    if (doomRpg == NULL || view == NULL ||
        fnv1a(view, sizeof(*view)) != EXPECTED_INITIAL_VIEW_FNV ||
        frameFNV() != EXPECTED_BASE_FRAME_FNV ||
        EspNativeGameplayDispatch_adoptView() != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        failAndDetach("activation predecessor/adopt");
        return;
    }
    turn = EspNativeGameplayDispatch_view();
    if (!turnStateMatchesView(turn, view) || turn->sequence != 0U ||
        turn->lastAction != ESP_NATIVE_GAMEPLAY_ACTION_NONE ||
        !runtimeBoundary(doomRpg)) {
        failAndDetach("activation turn owner");
        return;
    }
    viewCopy = *view;
    turnCopy = *turn;

    memset(&fake, 0, sizeof(fake));
    fake.action = ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
    fake.zone = ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD;
    fake.logicalX = 79U;
    fake.logicalY = 32U;
    fake.sequence = 0x1234U;
    fake.pending = 1U;
    fake.active = 1U;
    if (EspNativeGameplayDispatch_prepareTurn(
            &fake, &beforeView, &afterView, &beforeTurn, &afterTurn, &result) !=
            ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED ||
        memcmp(EspPlayerView_view(), &viewCopy, sizeof(viewCopy)) != 0 ||
        memcmp(EspNativeGameplayDispatch_view(), &turnCopy, sizeof(turnCopy)) != 0) {
        failAndDetach("fail-closed deferred action");
        return;
    }

    probeState.doomRpg = doomRpg;
    probeState.active = 1U;
    PlatformInput_setTapCallback(onTurnTap);

    heapAfter = heap8();
    largestAfter = largest8();
    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        !runtimeBoundary(doomRpg)) {
        failAndDetach("activation heap/boundary");
        return;
    }

    turn = EspNativeGameplayDispatch_view();
    printf("[TURNPROBE] READY turnStateBytes=%u resultBytes=%u viewBytes=%u angle=%u fixed=sin:%d cos:%d step:%d,%d baseline=%08x heap=%u->%u largest=%u->%u\n",
           (unsigned int)sizeof(EspNativeGameplayTurnState),
           (unsigned int)sizeof(EspNativeGameplayDispatchResult),
           (unsigned int)sizeof(EspPlayerViewState),
           (unsigned int)turn->destAngle,
           turn->viewSin, turn->viewCos, turn->viewStepX, turn->viewStepY,
           (unsigned int)EXPECTED_BASE_FRAME_FNV,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[TURNPROBE] PARK tap TURN_LEFT then TURN_RIGHT (or inverse): first tap must visibly rotate the complete native scene/HUD compass; inverse must restore frame %08x exactly. Other zones must log DEFERRED only.\n",
           (unsigned int)EXPECTED_BASE_FRAME_FNV);
}
