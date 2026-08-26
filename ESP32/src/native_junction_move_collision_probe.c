#include <SDL.h>
#include <stddef.h>
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
#include "esp_map_line_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_collision.h"
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
#define SPAWN_X 992
#define SPAWN_Y 1888
#define MAP_MIN_CENTER 32
#define MAP_MAX_CENTER 2016
#define DYNAMIC_LINE_WITNESS_INDEX 35U
#define DYNAMIC_LINE_WITNESS_SOURCE_TILE 943U
#define DYNAMIC_LINE_WITNESS_DEST_TILE 975U

typedef struct LegacySnapshot_s {
    uint32_t hud;
    uint32_t player;
    uint32_t game;
    uint32_t canvas;
    uint32_t render;
} LegacySnapshot;

typedef struct MoveExecutionWorkspace_s {
    EspPlayerViewState liveBefore;
    EspPlayerViewState beforeView;
    EspPlayerViewState afterView;
    EspNativeGameplayTurnState turnBefore;
    EspNativeGameplayTurnState turnAfter;
    EspNativeGameplayMoveResult result;
    EspNativeGameplayFrameStats frameStats;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    LegacySnapshot legacyBefore;
    LegacySnapshot legacyAfter;
} MoveExecutionWorkspace;

typedef struct MoveProbeState_s {
    DoomRPG_t* doomRpg;
    EspNativeGameplayInputState restoredMove;
    EspNativeGameplayInputState pendingIntent;
    uint32_t moves;
    uint32_t blockedMoves;
    uint32_t observedMoves;
    uint32_t feedbackBaselineFNV;
    uint8_t recommendedAction;
    uint8_t recommendedInverse;
    uint8_t blockedWitnessAction;
    uint8_t initialized;
    uint8_t feedbackMovePending;
    uint8_t pendingIntentValid;
    uint8_t failed;
    uint8_t reserved;
} MoveProbeState;

static MoveProbeState probeState;
static MoveExecutionWorkspace executionWorkspace;

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

static int movementAction(uint8_t action) {
    return action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT;
}

static uint8_t inverseAction(uint8_t action) {
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
        return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
        return ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT;
    default:
        return ESP_NATIVE_GAMEPLAY_ACTION_NONE;
    }
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

static int residentEqualExceptLineStateFNV(
    const EspMapResidentSnapshot* a,
    const EspMapResidentSnapshot* b) {
    const size_t fieldOffset = offsetof(EspMapResidentSnapshot, lineStateFNV1a);
    const size_t tailOffset = fieldOffset + sizeof(a->lineStateFNV1a);

    if (a == NULL || b == NULL || tailOffset > sizeof(*a)) return 0;
    return memcmp(a, b, fieldOffset) == 0 &&
           memcmp((const uint8_t*)a + tailOffset,
                  (const uint8_t*)b + tailOffset,
                  sizeof(*a) - tailOffset) == 0;
}

static int viewRuntimeValid(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == 44U &&
           centeredCoordinate(view->viewX) && centeredCoordinate(view->viewY) &&
           view->viewZ == 36 && view->viewX == view->destX &&
           view->viewY == view->destY && view->viewAngle == view->destAngle &&
           view->viewAngle >= 0 && view->viewAngle <= 255 &&
           (view->viewAngle & 63) == 0 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->hudRefreshPending == 0U && view->facingRefreshPending == 0U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->active == 1U;
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
    const EspMapLineStateView* lines;
    EspMapResidentSnapshot resident;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL)
        return 0;
    view = EspPlayerView_view();
    turn = EspNativeGameplayDispatch_view();
    hud = EspNativeGameplayHud_view();
    lines = EspMapLineState_view();
    return doomRpg->doomCanvas->state == ST_INTRO &&
           doomRpg->doomCanvas->storyPage == 3 &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0 &&
           doomRpg->render->framebuffer == Esp32PlatformVideo_framebuffer() &&
           doomRpg->render->screenX == 0 && doomRpg->render->screenY == 20 &&
           doomRpg->render->screenWidth == 160 && doomRpg->render->screenHeight == 80 &&
           doomRpg->render->shapeData == NULL && doomRpg->render->mediaTexels == NULL &&
           viewRuntimeValid(view) && turnStateMatchesView(turn, view) &&
           hud != NULL && fnv1a(hud, sizeof(*hud)) == EXPECTED_INITIAL_HUD_FNV &&
           EspMapState_isReady() && EspMapSpriteTopology_isReady() &&
           lines != NULL && lines->openCount == 0U &&
           EspMapResidentLifecycle_capture(&resident) && residentCanonical(&resident) &&
           !EspAssetPack_isOpen();
}

static int deltaForAction(const EspNativeGameplayTurnState* turn,
                          uint8_t action,
                          int32_t* outX,
                          int32_t* outY) {
    if (turn == NULL || outX == NULL || outY == NULL || !movementAction(action)) {
        return 0;
    }
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        *outX = turn->viewStepX;
        *outY = turn->viewStepY;
        return 1;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        *outX = -turn->viewStepX;
        *outY = -turn->viewStepY;
        return 1;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
        *outX = turn->viewStepY;
        *outY = -turn->viewStepX;
        return 1;
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
        *outX = -turn->viewStepY;
        *outY = turn->viewStepX;
        return 1;
    default:
        return 0;
    }
}

static void failProbe(const char* reason) {
    printf("[MOVEPROBE] FAILED %s frame=%08x pending=%u queued=%u feedbackBaseline=%08x pack=%d\n",
           reason, (unsigned int)frameFNV(),
           (unsigned int)EspNativeGameplayInput_peek()->pending,
           (unsigned int)probeState.pendingIntentValid,
           (unsigned int)probeState.feedbackBaselineFNV,
           EspAssetPack_isOpen());
    probeState.failed = 1U;
    probeState.feedbackMovePending = 0U;
    probeState.pendingIntentValid = 0U;
    probeState.feedbackBaselineFNV = 0U;
}

static int scanNeighbor(uint8_t action,
                        EspNativeGameplayCollisionResult* outCollision) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayTurnState* turn = EspNativeGameplayDispatch_view();
    int32_t dx;
    int32_t dy;
    EspNativeGameplayCollisionStatus status;

    if (outCollision == NULL || view == NULL || turn == NULL ||
        !deltaForAction(turn, action, &dx, &dy)) return 0;
    status = EspNativeGameplayCollision_traceCardinalStep(
        view->viewX, view->viewY, view->viewX + dx, view->viewY + dy,
        outCollision);
    printf("[MOVEPROBE] NEIGHBOR action=%s delta=%d,%d tile=%u->%u flags=%02x status=%s blocker=%u type=%u openLines=%u\n",
           EspNativeGameplayInput_actionName(action), (int)dx, (int)dy,
           (unsigned int)outCollision->sourceTile,
           (unsigned int)outCollision->destTile,
           (unsigned int)outCollision->destFlags,
           EspNativeGameplayCollision_statusName(status),
           (unsigned int)outCollision->blockerSpriteIndex,
           (unsigned int)outCollision->blockerType,
           (unsigned int)outCollision->openLineCount);
    return status == ESP_NATIVE_GAMEPLAY_COLLISION_CLEAR ? 1 :
           (status == ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_WALL ||
            status == ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY ? 2 : 0);
}

static int verifyDynamicLineCollision(DoomRPG_t* doomRpg) {
    MoveExecutionWorkspace* w = &executionWorkspace;
    const EspMapLineStateView* lines;
    const EspPlayerViewState* view;
    EspNativeGameplayCollisionResult closedResult;
    EspNativeGameplayCollisionResult openResult;
    EspNativeGameplayCollisionResult restoredResult;
    EspNativeGameplayCollisionStatus closedStatus;
    EspNativeGameplayCollisionStatus openStatus;
    EspNativeGameplayCollisionStatus restoredStatus;
    uint32_t stateFNVBefore = 0U;
    uint32_t stateFNVOpen = 0U;
    uint32_t openCountBefore = 0U;
    uint32_t lockedCountBefore = 0U;
    uint32_t frameBefore = 0U;
    uint32_t heapBefore = 0U;
    uint32_t largestBefore = 0U;
    uint8_t openBit;
    int opened = 0;
    const char* failure = "precondition";

    memset(w, 0, sizeof(*w));
    memset(&closedResult, 0, sizeof(closedResult));
    memset(&openResult, 0, sizeof(openResult));
    memset(&restoredResult, 0, sizeof(restoredResult));

    lines = EspMapLineState_view();
    view = EspPlayerView_view();
    if (doomRpg == NULL || lines == NULL || view == NULL ||
        lines->lineCount <= DYNAMIC_LINE_WITNESS_INDEX ||
        view->viewX != SPAWN_X || view->viewY != SPAWN_Y ||
        !EspMapLineState_getOpen(DYNAMIC_LINE_WITNESS_INDEX, &openBit) ||
        openBit != 0U) {
        goto fail;
    }

    stateFNVBefore = lines->stateFNV1a;
    openCountBefore = lines->openCount;
    lockedCountBefore = lines->lockedCount;
    frameBefore = frameFNV();
    heapBefore = heap8();
    largestBefore = largest8();
    w->liveBefore = *view;
    if (frameBefore != EXPECTED_BASE_FRAME_FNV || openCountBefore != 0U ||
        !legacySnapshot(doomRpg, &w->legacyBefore) ||
        !EspMapResidentLifecycle_capture(&w->residentBefore) ||
        !residentCanonical(&w->residentBefore)) {
        goto fail;
    }

    closedStatus = EspNativeGameplayCollision_traceCardinalStep(
        SPAWN_X, SPAWN_Y, SPAWN_X, SPAWN_Y + 64, &closedResult);
    if (closedStatus != ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY ||
        closedResult.sourceTile != DYNAMIC_LINE_WITNESS_SOURCE_TILE ||
        closedResult.destTile != DYNAMIC_LINE_WITNESS_DEST_TILE ||
        closedResult.blockerSpriteIndex != ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE ||
        closedResult.blockerType != 0U ||
        closedResult.openLineCount != (uint8_t)openCountBefore) {
        failure = "closed witness";
        goto fail;
    }
    printf("[DYNLINECOLLISION] CLOSED line=%u tile=%u->%u status=%s blocker=%u type=%u openLines=%u stateFNV=%08x\n",
           (unsigned int)DYNAMIC_LINE_WITNESS_INDEX,
           (unsigned int)closedResult.sourceTile,
           (unsigned int)closedResult.destTile,
           EspNativeGameplayCollision_statusName(closedStatus),
           (unsigned int)closedResult.blockerSpriteIndex,
           (unsigned int)closedResult.blockerType,
           (unsigned int)closedResult.openLineCount,
           (unsigned int)stateFNVBefore);

    if (!EspMapLineState_setOpen(DYNAMIC_LINE_WITNESS_INDEX, 1U)) {
        failure = "open mutation";
        goto fail;
    }
    opened = 1;
    lines = EspMapLineState_view();
    if (lines == NULL ||
        !EspMapLineState_getOpen(DYNAMIC_LINE_WITNESS_INDEX, &openBit) ||
        openBit != 1U || lines->openCount != openCountBefore + 1U ||
        lines->lockedCount != lockedCountBefore ||
        lines->stateFNV1a == stateFNVBefore) {
        failure = "open state";
        goto fail;
    }
    stateFNVOpen = lines->stateFNV1a;

    openStatus = EspNativeGameplayCollision_traceCardinalStep(
        SPAWN_X, SPAWN_Y, SPAWN_X, SPAWN_Y + 64, &openResult);
    if (openStatus != ESP_NATIVE_GAMEPLAY_COLLISION_CLEAR ||
        openResult.sourceTile != DYNAMIC_LINE_WITNESS_SOURCE_TILE ||
        openResult.destTile != DYNAMIC_LINE_WITNESS_DEST_TILE ||
        openResult.blockerSpriteIndex != ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE ||
        openResult.linkedBlockers != 0U ||
        openResult.openLineCount != (uint8_t)(openCountBefore + 1U)) {
        failure = "open collision";
        goto fail;
    }
    if (frameFNV() != frameBefore || heap8() != heapBefore ||
        largest8() != largestBefore || EspPlayerView_view() == NULL ||
        memcmp(EspPlayerView_view(), &w->liveBefore, sizeof(w->liveBefore)) != 0 ||
        !legacySnapshot(doomRpg, &w->legacyAfter) ||
        !legacyEqual(&w->legacyBefore, &w->legacyAfter) ||
        !EspMapResidentLifecycle_capture(&w->residentAfter) ||
        w->residentAfter.lineStateFNV1a != stateFNVOpen ||
        !residentEqualExceptLineStateFNV(&w->residentBefore, &w->residentAfter) ||
        EspAssetPack_isOpen()) {
        failure = "open side effect";
        goto fail;
    }
    printf("[DYNLINECOLLISION] OPEN line=%u tile=%u->%u status=%s blocker=%u openLines=%u stateFNV=%08x frame=%08x heap=%u largest=%u sideEffects=line-open-bit+resident-line-fnv-only\n",
           (unsigned int)DYNAMIC_LINE_WITNESS_INDEX,
           (unsigned int)openResult.sourceTile,
           (unsigned int)openResult.destTile,
           EspNativeGameplayCollision_statusName(openStatus),
           (unsigned int)openResult.blockerSpriteIndex,
           (unsigned int)openResult.openLineCount,
           (unsigned int)stateFNVOpen,
           (unsigned int)frameBefore,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore);

    if (!EspMapLineState_setOpen(DYNAMIC_LINE_WITNESS_INDEX, 0U)) {
        failure = "close rollback mutation";
        goto fail;
    }
    opened = 0;
    lines = EspMapLineState_view();
    if (lines == NULL ||
        !EspMapLineState_getOpen(DYNAMIC_LINE_WITNESS_INDEX, &openBit) ||
        openBit != 0U || lines->openCount != openCountBefore ||
        lines->lockedCount != lockedCountBefore ||
        lines->stateFNV1a != stateFNVBefore) {
        failure = "close rollback state";
        goto fail;
    }

    restoredStatus = EspNativeGameplayCollision_traceCardinalStep(
        SPAWN_X, SPAWN_Y, SPAWN_X, SPAWN_Y + 64, &restoredResult);
    if (restoredStatus != closedStatus ||
        memcmp(&closedResult, &restoredResult, sizeof(closedResult)) != 0 ||
        frameFNV() != frameBefore || heap8() != heapBefore ||
        largest8() != largestBefore || EspPlayerView_view() == NULL ||
        memcmp(EspPlayerView_view(), &w->liveBefore, sizeof(w->liveBefore)) != 0 ||
        !legacySnapshot(doomRpg, &w->legacyAfter) ||
        !legacyEqual(&w->legacyBefore, &w->legacyAfter) ||
        !EspMapResidentLifecycle_capture(&w->residentAfter) ||
        memcmp(&w->residentBefore, &w->residentAfter,
               sizeof(w->residentBefore)) != 0 ||
        !runtimeBoundary(doomRpg) || EspAssetPack_isOpen()) {
        failure = "restored exactness";
        goto fail;
    }

    printf("[DYNLINECOLLISION] RESTORED line=%u tile=%u->%u status=%s openLines=%u stateFNV=%08x openFNV=%08x resultExact=yes frameExact=yes viewExact=yes legacyExact=yes residentExact=yes heapExact=yes\n",
           (unsigned int)DYNAMIC_LINE_WITNESS_INDEX,
           (unsigned int)restoredResult.sourceTile,
           (unsigned int)restoredResult.destTile,
           EspNativeGameplayCollision_statusName(restoredStatus),
           (unsigned int)restoredResult.openLineCount,
           (unsigned int)lines->stateFNV1a,
           (unsigned int)stateFNVOpen);
    return 1;

fail:
    if (opened) {
        if (!EspMapLineState_setOpen(DYNAMIC_LINE_WITNESS_INDEX, 0U)) {
            failure = "rollback after failure";
        }
        opened = 0;
    }
    lines = EspMapLineState_view();
    printf("[DYNLINECOLLISION] FAILED reason=%s line=%u openLines=%u stateFNV=%08x baselineFNV=%08x frame=%08x heap=%u largest=%u\n",
           failure,
           (unsigned int)DYNAMIC_LINE_WITNESS_INDEX,
           (unsigned int)(lines != NULL ? lines->openCount : 0U),
           (unsigned int)(lines != NULL ? lines->stateFNV1a : 0U),
           (unsigned int)stateFNVBefore,
           (unsigned int)frameFNV(),
           (unsigned int)heap8(),
           (unsigned int)largest8());
    return 0;
}

static int executeConsumedIntent(const EspNativeGameplayInputState* intent) {
    MoveExecutionWorkspace* w = &executionWorkspace;
    EspNativeGameplayDispatchStatus dispatch;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    int positionRoundTrip;

    memset(w, 0, sizeof(*w));
    if (intent == NULL || probeState.doomRpg == NULL ||
        !runtimeBoundary(probeState.doomRpg) || EspPlayerView_view() == NULL ||
        EspNativeGameplayDispatch_view() == NULL) {
        failProbe("execute boundary");
        return 0;
    }

    w->liveBefore = *EspPlayerView_view();
    w->turnBefore = *EspNativeGameplayDispatch_view();
    heapBefore = heap8();
    largestBefore = largest8();
    frameBefore = frameFNV();
    if (!legacySnapshot(probeState.doomRpg, &w->legacyBefore) ||
        !EspMapResidentLifecycle_capture(&w->residentBefore) ||
        !residentCanonical(&w->residentBefore)) {
        failProbe("pre-move snapshots");
        return 0;
    }

    dispatch = EspNativeGameplayDispatch_prepareMove(
        intent, &w->beforeView, &w->afterView, &w->result);

    if (dispatch == ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_BLOCKED) {
        heapAfter = heap8();
        largestAfter = largest8();
        if (EspPlayerView_view() == NULL || EspNativeGameplayDispatch_view() == NULL ||
            memcmp(EspPlayerView_view(), &w->liveBefore, sizeof(w->liveBefore)) != 0 ||
            memcmp(EspNativeGameplayDispatch_view(), &w->turnBefore,
                   sizeof(w->turnBefore)) != 0 ||
            frameFNV() != frameBefore || heapAfter != heapBefore ||
            largestAfter != largestBefore ||
            !legacySnapshot(probeState.doomRpg, &w->legacyAfter) ||
            !legacyEqual(&w->legacyBefore, &w->legacyAfter) ||
            !EspMapResidentLifecycle_capture(&w->residentAfter) ||
            memcmp(&w->residentBefore, &w->residentAfter,
                   sizeof(w->residentBefore)) != 0 ||
            !runtimeBoundary(probeState.doomRpg) || EspAssetPack_isOpen()) {
            failProbe("blocked move mutated state");
            return 0;
        }
        ++probeState.blockedMoves;
        printf("[MOVE] BLOCKED n=%u seq=%u action=%s delta=%d,%d tile=%u->%u collision=%s blocker=%u type=%u frame=%08x exact=yes heap=%u->%u largest=%u->%u turnAdvance=no tileDispatch=no\n",
               (unsigned int)probeState.blockedMoves,
               (unsigned int)w->result.sequence,
               EspNativeGameplayInput_actionName(w->result.action),
               (int)w->result.deltaX, (int)w->result.deltaY,
               (unsigned int)w->result.sourceTile,
               (unsigned int)w->result.destTile,
               EspNativeGameplayCollision_statusName(
                   (EspNativeGameplayCollisionStatus)w->result.collisionStatus),
               (unsigned int)w->result.blockerSpriteIndex,
               (unsigned int)w->result.blockerType,
               (unsigned int)frameBefore,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter);
        return 1;
    }

    if (dispatch != ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED ||
        memcmp(&w->beforeView, &w->liveBefore, sizeof(w->beforeView)) != 0) {
        failProbe(dispatch == ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_UNSUPPORTED
                      ? "collision unsupported"
                      : "dispatch prepare");
        return 0;
    }

    if (EspNativeGameplayDispatch_commitMove(
            &w->beforeView, &w->afterView, &w->result) !=
            ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        failProbe("dispatch commit");
        return 0;
    }

    printf("[MOVESTACK] beforeRender highWater=%u execScratch=%uB pos=%d,%d angle=%d\n",
           stackHighWater(), (unsigned int)sizeof(executionWorkspace),
           (int)w->afterView.viewX, (int)w->afterView.viewY,
           (int)w->afterView.viewAngle);
    if (!EspNativeGameplayFrame_renderTurn(
            probeState.doomRpg->render, (uint8_t)w->afterView.viewAngle,
            &w->frameStats)) {
        (void)EspNativeGameplayDispatch_rollbackMove(
            &w->afterView, &w->beforeView, &w->result);
        failProbe("native frame render");
        return 0;
    }

    frameAfter = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (EspNativeGameplayDispatch_view() == NULL) {
        failProbe("turn owner missing");
        return 0;
    }
    w->turnAfter = *EspNativeGameplayDispatch_view();
    printf("[MOVE] PHASE action=%s pos=%d,%d->%d,%d viewport=%08x->world:%08x->sprites:%08x hud=%08x->preserved:%08x->after:%08x frame=%08x->%08x worldRouteNoPresent=%u finalPresent=%u\n",
           EspNativeGameplayInput_actionName(w->result.action),
           (int)w->beforeView.viewX, (int)w->beforeView.viewY,
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
        memcmp(&w->turnBefore, &w->turnAfter, sizeof(w->turnBefore)) != 0 ||
        !runtimeBoundary(probeState.doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter == frameBefore || w->frameStats.frameAfterFNV != frameAfter ||
        w->frameStats.temporaryHudBytes != 0U ||
        w->frameStats.worldRouteNoPresent != 1U ||
        w->frameStats.finalPresented != 1U || EspAssetPack_isOpen()) {
        failProbe("post-move integrity");
        return 0;
    }

    positionRoundTrip = w->afterView.viewX == SPAWN_X &&
                        w->afterView.viewY == SPAWN_Y &&
                        w->afterView.viewAngle == 64;
    if (positionRoundTrip &&
        (w->frameStats.viewportAfterSpritesFNV != EXPECTED_BASE_VIEWPORT_FNV ||
         w->frameStats.hudBandsAfterFNV != EXPECTED_BASE_HUD_BANDS_FNV ||
         frameAfter != EXPECTED_BASE_FRAME_FNV)) {
        failProbe("spawn round-trip mismatch");
        return 0;
    }

    ++probeState.moves;
    printf("[MOVE] OK n=%u seq=%u action=%s delta=%d,%d tile=%u->%u pos=%d,%d->%d,%d viewFNV=%08x->%08x frame=%08x->%08x spawnRoundTrip=%s\n",
           (unsigned int)probeState.moves,
           (unsigned int)w->result.sequence,
           EspNativeGameplayInput_actionName(w->result.action),
           (int)w->result.deltaX, (int)w->result.deltaY,
           (unsigned int)w->result.sourceTile,
           (unsigned int)w->result.destTile,
           (int)w->beforeView.viewX, (int)w->beforeView.viewY,
           (int)w->afterView.viewX, (int)w->afterView.viewY,
           (unsigned int)fnv1a(&w->beforeView, sizeof(w->beforeView)),
           (unsigned int)fnv1a(EspPlayerView_view(), sizeof(EspPlayerViewState)),
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           positionRoundTrip ? "exact" : "no");
    printf("[MOVE] RENDER world=%08x walls=%u/%u planes=%u sprites=%u/%u glows=%u/%u spriteReads=%u hudReads=%u tempHud=%uB routeNoPresent=%u final=%u timeUs=world:%u sprite:%u hud:%u present:%u total:%u heap=%u->%u largest=%u->%u stackHighWater=%u legacyStable=yes residentStable=yes orientationStable=yes turnAdvance=no tileDispatch=no facingRefresh=deferred\n",
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

static int queueRestoredMove(void) {
    const uint32_t baseline = probeState.feedbackBaselineFNV;
    if (!probeState.feedbackMovePending || probeState.pendingIntentValid ||
        probeState.failed || baseline == 0U || frameFNV() != baseline) return 0;
    probeState.pendingIntent = probeState.restoredMove;
    probeState.pendingIntentValid = 1U;
    memset(&probeState.restoredMove, 0, sizeof(probeState.restoredMove));
    probeState.feedbackMovePending = 0U;
    probeState.feedbackBaselineFNV = 0U;
    printf("[MOVEPROBE] QUEUED seq=%u action=%s baseline=%08x origin=restored-input-probe execute=next-service\n",
           (unsigned int)probeState.pendingIntent.sequence,
           EspNativeGameplayInput_actionName(probeState.pendingIntent.action),
           (unsigned int)baseline);
    return 1;
}

void Esp32JunctionMoveCollisionProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) {
    uint32_t baseline;
    if (!probeState.initialized || probeState.failed ||
        probeState.feedbackMovePending || probeState.pendingIntentValid ||
        intent == NULL || !movementAction(intent->action)) return;
    baseline = frameFNV();
    if (baseline == 0U) return;
    probeState.restoredMove = *intent;
    probeState.feedbackBaselineFNV = baseline;
    probeState.feedbackMovePending = 1U;
    ++probeState.observedMoves;
    printf("[MOVEPROBE] HANDOFF n=%u seq=%u action=%s baseline=%08x waitingForFeedbackRestore=yes\n",
           (unsigned int)probeState.observedMoves,
           (unsigned int)intent->sequence,
           EspNativeGameplayInput_actionName(intent->action),
           (unsigned int)baseline);
}

void Esp32JunctionMoveCollisionProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    memset(&executionWorkspace, 0, sizeof(executionWorkspace));
}

int Esp32JunctionMoveCollisionProbe_isActive(void) {
    return probeState.initialized != 0U && probeState.failed == 0U;
}

void Esp32JunctionMoveCollisionProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    static const uint8_t actions[4] = {
        ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD,
        ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK,
        ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT,
        ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT
    };
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    unsigned int i;

    if (probeState.failed) return;

    if (!probeState.initialized) {
        const EspPlayerViewState* view;
        unsigned int clearCount = 0U;
        unsigned int blockedCount = 0U;
        if (!Esp32JunctionTurnDispatchProbe_isActive()) return;

        printf("\n=== Doom RPG ESP32-native cardinal movement + dynamic line collision / viewport hot path ===\n");
        printf("[MOVEPROBE] CONTRACT cardinal MOVE semantics and WALL/sprite collision stay unchanged. Line collision now consumes EspMapLineState per-line open bits: a closed line participates, an open line is skipped, and closing relinks it logically without Entity_t/entityDb. Activation temporarily toggles canonical arrival-door line 35 and requires CLOSED->BLOCK, OPEN->CLEAR, CLOSE->identical BLOCK with exact line-state FNV rollback, heap/frame/view/legacy stability plus resident equality except the intentional line-state FNV while OPEN. No SELECT, sound, animation, renderer mutation, tile events, turn advance, entity activation or facing refresh are enabled.\n");

        heapBefore = heap8();
        largestBefore = largest8();
        view = EspPlayerView_view();
        if (doomRpg == NULL || view == NULL ||
            fnv1a(view, sizeof(*view)) != EXPECTED_INITIAL_VIEW_FNV ||
            frameFNV() != EXPECTED_BASE_FRAME_FNV ||
            !runtimeBoundary(doomRpg)) {
            failProbe("activation predecessor");
            return;
        }

        if (!verifyDynamicLineCollision(doomRpg)) {
            failProbe("dynamic line collision witness");
            return;
        }

        for (i = 0U; i < 4U; ++i) {
            EspNativeGameplayCollisionResult collision;
            const int outcome = scanNeighbor(actions[i], &collision);
            if (outcome == 0) {
                failProbe("neighbor collision query");
                return;
            }
            if (outcome == 1) {
                ++clearCount;
                if (probeState.recommendedAction == ESP_NATIVE_GAMEPLAY_ACTION_NONE) {
                    probeState.recommendedAction = actions[i];
                    probeState.recommendedInverse = inverseAction(actions[i]);
                }
            }
            else {
                ++blockedCount;
                if (probeState.blockedWitnessAction == ESP_NATIVE_GAMEPLAY_ACTION_NONE) {
                    probeState.blockedWitnessAction = actions[i];
                }
            }
        }
        if (clearCount == 0U || probeState.recommendedInverse ==
                                  ESP_NATIVE_GAMEPLAY_ACTION_NONE) {
            failProbe("spawn has no clear movement neighbor");
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

        printf("[MOVEPROBE] READY collisionBytes=%u moveResultBytes=%u viewBytes=%u frameStatsBytes=%u execScratchBytes=%u neighbors=clear:%u blocked:%u recommended=%s then %s blockedWitness=%s heap=%u->%u largest=%u->%u stackHighWater=%u dynamicLines=per-line-open-skip dynLineWitness=line35:closed-open-close-exact renderFromCallback=no gameplayWorldPresent=none tempHud=0\n",
               (unsigned int)sizeof(EspNativeGameplayCollisionResult),
               (unsigned int)sizeof(EspNativeGameplayMoveResult),
               (unsigned int)sizeof(EspPlayerViewState),
               (unsigned int)sizeof(EspNativeGameplayFrameStats),
               (unsigned int)sizeof(executionWorkspace),
               clearCount, blockedCount,
               EspNativeGameplayInput_actionName(probeState.recommendedAction),
               EspNativeGameplayInput_actionName(probeState.recommendedInverse),
               EspNativeGameplayInput_actionName(probeState.blockedWitnessAction),
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               stackHighWater());
        printf("[MOVEPROBE] PARK tap recommended move + inverse and TURN at moved positions; successful render logs must show tempHud=0 routeNoPresent=1 plus phase timings.\n");
        return;
    }

    if (probeState.feedbackMovePending && !probeState.pendingIntentValid) {
        const uint32_t baseline = probeState.feedbackBaselineFNV;
        if (baseline == 0U) {
            failProbe("missing feedback baseline");
            return;
        }
        if (frameFNV() != baseline || EspNativeGameplayInput_peek()->pending) return;
        if (!queueRestoredMove()) {
            failProbe("restored move queue");
            return;
        }
        printf("[MOVEPROBE] RESTORED frame=%08x exact=yes queued=yes lifecycleReturnBeforeExecute=yes\n",
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
