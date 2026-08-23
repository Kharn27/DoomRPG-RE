#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_player_facing_state.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_view_state.h"
#include "native_junction_facing_probe.h"
#include "native_junction_finish_rotation_tile_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_FACING_BYTES 32U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_BEFORE_FNV 0x1bd0f09bU
#define EXPECTED_VIEW_AFTER_FNV 0xafcdcf74U
#define EXPECTED_INITIAL_FNV 0xf73e28b2U
#define EXPECTED_ORIENTATION_FNV 0xacc754a6U
#define EXPECTED_SECOND_TILE_FNV 0x09e58e0dU
#define EXPECTED_SNAPSHOT_FNV 0xbc9071e9U

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

static uint32_t hashBytes(const void* data, uint32_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) return 0U;
    return hashBytes(framebuffer, (uint32_t)bytes);
}

static uint32_t gameWitness(const Game_t* game) {
    uint32_t v[12];
    if (game == NULL) return 0U;
    v[0] = (uint32_t)game->spawnParam;
    v[1] = (uint32_t)game->isLoaded;
    v[2] = (uint32_t)game->activeLoadType;
    v[3] = (uint32_t)game->numEntities;
    v[4] = (uint32_t)game->numMonsters;
    v[5] = (uint32_t)game->skipAdvanceTurn;
    v[6] = (uint32_t)game->f658b;
    v[7] = (uint32_t)game->waitTime;
    v[8] = (uint32_t)game->tileEvent;
    v[9] = (uint32_t)game->tileEventIndex;
    v[10] = (uint32_t)game->tileEventFlags;
    v[11] = (uint32_t)game->saveTileEvent;
    return hashBytes(v, sizeof(v));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t v[13];
    if (player == NULL) return 0U;
    v[0] = (uint32_t)player->keys;
    v[1] = (uint32_t)player->moves;
    v[2] = (uint32_t)player->xpGained;
    v[3] = (uint32_t)player->berserkerTics;
    v[4] = (uint32_t)player->disabledWeapons;
    v[5] = (uint32_t)player->weapons;
    v[6] = (uint32_t)player->weapon;
    v[7] = (uint32_t)player->currentXP;
    v[8] = (uint32_t)player->level;
    v[9] = (uint32_t)player->credits;
    v[10] = (uint32_t)player->completedLevels;
    v[11] = (uint32_t)(player->dogFamiliar != NULL);
    v[12] = (uint32_t)(player->facingEntity != NULL);
    return hashBytes(v, sizeof(v));
}

static uint32_t canvasWitness(const DoomCanvas_t* canvas) {
    uint32_t v[14];
    if (canvas == NULL) return 0U;
    v[0] = (uint32_t)canvas->viewX;
    v[1] = (uint32_t)canvas->viewY;
    v[2] = (uint32_t)canvas->viewZ;
    v[3] = (uint32_t)canvas->viewAngle;
    v[4] = (uint32_t)canvas->destX;
    v[5] = (uint32_t)canvas->destY;
    v[6] = (uint32_t)canvas->destAngle;
    v[7] = (uint32_t)canvas->viewSin;
    v[8] = (uint32_t)canvas->viewCos;
    v[9] = (uint32_t)canvas->viewStepX;
    v[10] = (uint32_t)canvas->viewStepY;
    v[11] = (uint32_t)canvas->loadType;
    v[12] = (uint32_t)canvas->state;
    v[13] = (uint32_t)canvas->storyPage;
    return hashBytes(v, sizeof(v));
}

static uint32_t renderWitness(const Render_t* render) {
    uint32_t v[8];
    if (render == NULL) return 0U;
    v[0] = (uint32_t)render->sinTable[64];
    v[1] = (uint32_t)render->sinTable[128];
    v[2] = (uint32_t)render->viewZOld;
    v[3] = (uint32_t)render->numMapSprites;
    v[4] = (uint32_t)render->mapStringCount;
    v[5] = (uint32_t)render->viewX;
    v[6] = (uint32_t)render->viewY;
    v[7] = (uint32_t)render->viewAngle;
    return hashBytes(v, sizeof(v));
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U && s->totalPayloadBytes == 10410U &&
           s->runtimeArenaBytes == 8867U && s->mapStateBytes == 1024U &&
           s->scriptStateBytes == 73U && s->lineStateBytes == 52U &&
           s->textureStateBytes == 26U && s->automapStateBytes == 32U &&
           s->topologyBytes == 336U && s->runtimeFNV1a == 0xbc432a0fU &&
           s->mapStateFNV1a == 0xc5cdfc04U && s->scriptStateFNV1a == 0xbc9b18ffU &&
           s->lineStateFNV1a == 0x3658710dU && s->textureStateFNV1a == 0x537319adU &&
           s->automapStateFNV1a == 0x0b2ae445U && s->topologyFNV1a == 0xd6e8df7dU &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U && hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_FNV;
}

static int viewBeforeCanonical(const EspPlayerViewState* v) {
    return v != NULL && sizeof(*v) == EXPECTED_VIEW_BYTES &&
           v->viewX == 992 && v->viewY == 1888 && v->viewZ == 36 &&
           v->viewAngle == 64 && v->destX == 992 && v->destY == 1888 &&
           v->destAngle == 64 && v->viewZOld == 4 && v->targetMapId == 9U &&
           v->gameplayLoadMapId == 2U && v->loadType == 0U &&
           v->spawnApplied == 1U && v->hudRefreshPending == 0U &&
           v->facingRefreshPending == 1U && v->playerSetupPending == 0U &&
           v->tileEnterPending == 0U && v->active == 1U &&
           hashBytes(v, sizeof(*v)) == EXPECTED_VIEW_BEFORE_FNV;
}

static int viewAfterCanonical(const EspPlayerViewState* v) {
    return v != NULL && sizeof(*v) == EXPECTED_VIEW_BYTES &&
           v->viewX == 992 && v->viewY == 1888 && v->viewZ == 36 &&
           v->viewAngle == 64 && v->destX == 992 && v->destY == 1888 &&
           v->destAngle == 64 && v->viewZOld == 4 && v->targetMapId == 9U &&
           v->gameplayLoadMapId == 2U && v->loadType == 0U &&
           v->spawnApplied == 1U && v->hudRefreshPending == 0U &&
           v->facingRefreshPending == 0U && v->playerSetupPending == 0U &&
           v->tileEnterPending == 0U && v->active == 1U &&
           hashBytes(v, sizeof(*v)) == EXPECTED_VIEW_AFTER_FNV;
}

static int inputOwnersCanonical(void) {
    const EspPlayerInitialTileState* initial = EspPlayerInitialTile_view();
    const EspPlayerOrientationState* orientation = EspPlayerOrientation_view();
    const EspPlayerFinishRotationTileState* second = EspPlayerFinishRotationTile_view();
    return initial != NULL && orientation != NULL && second != NULL &&
           hashBytes(initial, sizeof(*initial)) == EXPECTED_INITIAL_FNV &&
           hashBytes(orientation, sizeof(*orientation)) == EXPECTED_ORIENTATION_FNV &&
           hashBytes(second, sizeof(*second)) == EXPECTED_SECOND_TILE_FNV;
}

static const char* kindName(uint8_t kind) {
    if (kind == ESP_PLAYER_FACING_KIND_SPRITE) return "sprite";
    if (kind == ESP_PLAYER_FACING_KIND_LINE) return "line";
    if (kind == ESP_PLAYER_FACING_KIND_WALL) return "wall";
    return "none";
}

void Esp32JunctionFacingProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerFacing_reset();
}

int Esp32JunctionFacingProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionFacingProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspPlayerInitialTileState badInitial;
    EspPlayerOrientationState badOrientation;
    EspPlayerFinishRotationTileState badSecond;
    EspPlayerFacingState scratch;
    EspPlayerFacingState prepared;
    EspPlayerFacingState facingBeforeRepeat;
    EspPlayerViewState viewBeforeRepeat;
    const EspPlayerViewState* liveView;
    const EspPlayerFacingState* liveFacing;
    EspPlayerFacingStatus status;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter;
    uint32_t initialBefore, orientationBefore, secondBefore;
    int nullViewGate, nullInitialGate, nullOrientationGate, nullSecondGate;
    int nullOutputGate, missingFacingGate, tilePendingGate, angleGate;
    int initialMismatchGate, orientationInactiveGate, secondInactiveGate;
    int prepareAtomic, repeatGate, repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionFinishRotationTileProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONFACINGPROBE] ARMED hardware-proven second finishRotation tile active; durable native facing starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction durable facing ===\n");
    printf("[JUNCTIONFACINGPROBE] CONTRACT recover only final DoomCanvas_checkFacingEntity after the hardware-proven second tile: exact angle64 ray start=(992,1857) end=(992,1665), traceFlags=0x1f6ff; reconstruct legacy tile-head order from compact sprite topology plus native line geometry/state and wall sentinel semantics; resolve line EntityDef type through bounded /entities.db PAK reads; park one 32B pointer-free facing owner and consume only facingRefreshPending; never call legacy Game_trace, never assign Player.facingEntity, keep ST_PLAYING deferred, no presentation and no persistent allocation\n");

    liveView = EspPlayerView_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPlayerFacingState) != EXPECTED_FACING_BYTES ||
        !viewBeforeCanonical(liveView) || !inputOwnersCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) || !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONFACINGPROBE] FAILED unsafe post-second-tile boundary\n");
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    initialBefore = hashBytes(EspPlayerInitialTile_view(), sizeof(EspPlayerInitialTileState));
    orientationBefore = hashBytes(EspPlayerOrientation_view(), sizeof(EspPlayerOrientationState));
    secondBefore = hashBytes(EspPlayerFinishRotationTile_view(), sizeof(EspPlayerFinishRotationTileState));
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate = EspPlayerFacing_prepare(NULL, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_INVALID;
    nullInitialGate = EspPlayerFacing_prepare(&viewBefore, NULL,
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_INVALID;
    nullOrientationGate = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        NULL, EspPlayerFinishRotationTile_view(), &scratch) == ESP_PLAYER_FACING_INVALID;
    nullSecondGate = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), NULL, &scratch) == ESP_PLAYER_FACING_INVALID;
    nullOutputGate = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), NULL) ==
        ESP_PLAYER_FACING_INVALID;

    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    missingFacingGate = EspPlayerFacing_prepare(&badView, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_UNSUPPORTED_ORDER;
    badView = viewBefore;
    badView.tileEnterPending = 1U;
    tilePendingGate = EspPlayerFacing_prepare(&badView, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_UNSUPPORTED_ORDER;
    badView = viewBefore;
    badView.destAngle = 0;
    badView.viewAngle = 0;
    angleGate = EspPlayerFacing_prepare(&badView, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_UNSUPPORTED_CONTEXT;

    badInitial = *EspPlayerInitialTile_view();
    badInitial.targetMapId = 1U;
    initialMismatchGate = EspPlayerFacing_prepare(&viewBefore, &badInitial,
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_INITIAL_INVALID;
    badOrientation = *EspPlayerOrientation_view();
    badOrientation.active = 0U;
    orientationInactiveGate = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        &badOrientation, EspPlayerFinishRotationTile_view(), &scratch) ==
        ESP_PLAYER_FACING_ORIENTATION_INVALID;
    badSecond = *EspPlayerFinishRotationTile_view();
    badSecond.active = 0U;
    secondInactiveGate = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), &badSecond, &scratch) ==
        ESP_PLAYER_FACING_SECOND_TILE_INVALID;

    status = EspPlayerFacing_prepare(&viewBefore, EspPlayerInitialTile_view(),
        EspPlayerOrientation_view(), EspPlayerFinishRotationTile_view(), &prepared);
    if (status != ESP_PLAYER_FACING_OK || prepared.active != 1U ||
        prepared.traceStartX != 992 || prepared.traceStartY != 1857 ||
        prepared.traceEndX != 992 || prepared.traceEndY != 1665 ||
        prepared.targetMapId != 9U || prepared.gameplayLoadMapId != 2U ||
        prepared.loadType != 0U || prepared.kind > ESP_PLAYER_FACING_KIND_WALL ||
        EspAssetPack_isOpen() || EspPlayerFacing_isReady() ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        !inputOwnersCanonical() || !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0) {
        printf("[JUNCTIONFACINGPROBE] FAILED pure durable facing preparation status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    status = EspPlayerFacing_route();
    if (status != ESP_PLAYER_FACING_OK || !EspPlayerFacing_isReady() ||
        EspPlayerFacing_view() == NULL || !viewAfterCanonical(EspPlayerView_view()) ||
        !inputOwnersCanonical() || EspAssetPack_isOpen()) {
        printf("[JUNCTIONFACINGPROBE] FAILED live durable facing route status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }

    facingBeforeRepeat = *EspPlayerFacing_view();
    viewBeforeRepeat = *EspPlayerView_view();
    repeatGate = EspPlayerFacing_route() == ESP_PLAYER_FACING_ALREADY_ACTIVE;
    repeatAtomic = memcmp(&facingBeforeRepeat, EspPlayerFacing_view(),
                          sizeof(facingBeforeRepeat)) == 0 &&
                   memcmp(&viewBeforeRepeat, EspPlayerView_view(),
                          sizeof(viewBeforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter || playerBefore != playerAfter ||
        canvasBefore != canvasAfter || renderBefore != renderAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) || !viewAfterCanonical(EspPlayerView_view()) ||
        hashBytes(EspPlayerInitialTile_view(), sizeof(EspPlayerInitialTileState)) != initialBefore ||
        hashBytes(EspPlayerOrientation_view(), sizeof(EspPlayerOrientationState)) != orientationBefore ||
        hashBytes(EspPlayerFinishRotationTile_view(), sizeof(EspPlayerFinishRotationTileState)) != secondBefore) {
        printf("[JUNCTIONFACINGPROBE] FAILED integrity after durable facing route\n");
        probeState.done = 1;
        return;
    }

    liveFacing = EspPlayerFacing_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONFACING] READY stateBytes=%u stateFNV=%08x kind=%u kindName=%s hitIndex=%u hitTile=%u entityType=%u entitySubType=%u legacyIdentity=%08x traceEntities=%u traceFlags=%08x start=%ld/%ld end=%ld/%ld active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveFacing), (unsigned)hashBytes(liveFacing, sizeof(*liveFacing)),
           (unsigned)liveFacing->kind, kindName(liveFacing->kind),
           (unsigned)liveFacing->hitIndex, (unsigned)liveFacing->hitTile,
           (unsigned)liveFacing->entityType, (unsigned)liveFacing->entitySubType,
           (unsigned)liveFacing->legacyIdentity,
           (unsigned)liveFacing->traceEntityCount,
           (unsigned)ESP_PLAYER_FACING_TRACE_FLAGS,
           (long)liveFacing->traceStartX, (long)liveFacing->traceStartY,
           (long)liveFacing->traceEndX, (long)liveFacing->traceEndY,
           (unsigned)liveFacing->active, (unsigned)liveFacing->targetMapId,
           (unsigned)liveFacing->gameplayLoadMapId, (unsigned)liveFacing->loadType);
    printf("[JUNCTIONFACING] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x hudPending=%u facingPending=%u playerSetupPending=%u tileEnterPending=%u consumedOnlyFacing=yes\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_BEFORE_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->hudRefreshPending, (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->playerSetupPending, (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONFACING] OWNERS initialTileFNV=%08x orientationFNV=%08x secondTileFNV=%08x unchanged=yes durableFacingOwned=yes finishRotationComplete=yes ST_PLAYINGDeferred=yes\n",
           (unsigned)initialBefore, (unsigned)orientationBefore, (unsigned)secondBefore);
    printf("[JUNCTIONFACING] FAILCLOSED nullView=%d nullInitial=%d nullOrientation=%d nullSecond=%d nullOutput=%d missingFacing=%d tilePending=%d angle=%d initialMismatch=%d orientationInactive=%d secondInactive=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullInitialGate, nullOrientationGate, nullSecondGate,
           nullOutputGate, missingFacingGate, tilePendingGate, angleGate,
           initialMismatchGate, orientationInactiveGate, secondInactiveGate,
           prepareAtomic ? "yes" : "no", repeatGate, repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONFACING] RESIDENT snapshotFNV=%08x->%08x unchanged=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes, (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount, (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONFACING] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONFACING] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no FacingEntityMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONFACING] PARK state=%d page=%d targetMap=%u junctionResident=yes nativePlayerView=yes nativeInitialTile=yes nativeOrientation=yes nativeSecondTile=yes nativeFacing=yes facingPending=no finishRotationComplete=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveFacing->targetMapId);

    probeState.done = 1;
}
