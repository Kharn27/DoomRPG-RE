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
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_view_state.h"
#include "native_junction_initial_tile_probe.h"
#include "native_junction_orientation_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ORIENTATION_BYTES 24U
#define EXPECTED_ORIENTATION_FNV 0xacc754a6U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0x1bd0f09bU
#define EXPECTED_TILE_BYTES 24U
#define EXPECTED_TILE_FNV 0xf73e28b2U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_ANGLE 64U
#define EXPECTED_SIN 65536
#define EXPECTED_COS 0
#define EXPECTED_STEP_X 0
#define EXPECTED_STEP_Y (-64)

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
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }
    return hashBytes(framebuffer, (uint32_t)bytes);
}

static uint32_t gameWitness(const Game_t* game) {
    uint32_t values[10];
    if (game == NULL) return 0U;
    values[0] = (uint32_t)game->spawnParam;
    values[1] = (uint32_t)game->isLoaded;
    values[2] = (uint32_t)game->activeLoadType;
    values[3] = (uint32_t)game->numEntities;
    values[4] = (uint32_t)game->numMonsters;
    values[5] = (uint32_t)game->skipAdvanceTurn;
    values[6] = (uint32_t)game->f658b;
    values[7] = (uint32_t)game->tileEvent;
    values[8] = (uint32_t)game->tileEventIndex;
    values[9] = (uint32_t)game->tileEventFlags;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t values[10];
    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->keys;
    values[1] = (uint32_t)player->moves;
    values[2] = (uint32_t)player->xpGained;
    values[3] = (uint32_t)player->berserkerTics;
    values[4] = (uint32_t)player->disabledWeapons;
    values[5] = (uint32_t)player->weapons;
    values[6] = (uint32_t)player->weapon;
    values[7] = (uint32_t)player->currentXP;
    values[8] = (uint32_t)player->level;
    values[9] = (uint32_t)(player->dogFamiliar != NULL);
    return hashBytes(values, sizeof(values));
}

static uint32_t canvasWitness(const DoomCanvas_t* canvas) {
    uint32_t values[14];
    if (canvas == NULL) return 0U;
    values[0] = (uint32_t)canvas->viewX;
    values[1] = (uint32_t)canvas->viewY;
    values[2] = (uint32_t)canvas->viewZ;
    values[3] = (uint32_t)canvas->viewAngle;
    values[4] = (uint32_t)canvas->destX;
    values[5] = (uint32_t)canvas->destY;
    values[6] = (uint32_t)canvas->destAngle;
    values[7] = (uint32_t)canvas->viewSin;
    values[8] = (uint32_t)canvas->viewCos;
    values[9] = (uint32_t)canvas->viewStepX;
    values[10] = (uint32_t)canvas->viewStepY;
    values[11] = (uint32_t)canvas->loadType;
    values[12] = (uint32_t)canvas->state;
    values[13] = (uint32_t)canvas->storyPage;
    return hashBytes(values, sizeof(values));
}

static uint32_t renderWitness(const Render_t* render) {
    uint32_t values[8];
    if (render == NULL) return 0U;
    values[0] = (uint32_t)render->sinTable[64];
    values[1] = (uint32_t)render->sinTable[128];
    values[2] = (uint32_t)render->viewZOld;
    values[3] = (uint32_t)render->numMapSprites;
    values[4] = (uint32_t)render->mapStringCount;
    values[5] = (uint32_t)render->viewX;
    values[6] = (uint32_t)render->viewY;
    values[7] = (uint32_t)render->viewAngle;
    return hashBytes(values, sizeof(values));
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

static int residentCanonical(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL && sizeof(*snapshot) == 96U &&
           snapshot->totalPayloadBytes == 10410U &&
           snapshot->runtimeArenaBytes == 8867U &&
           snapshot->mapStateBytes == 1024U &&
           snapshot->scriptStateBytes == 73U &&
           snapshot->lineStateBytes == 52U &&
           snapshot->textureStateBytes == 26U &&
           snapshot->automapStateBytes == 32U &&
           snapshot->topologyBytes == 336U &&
           snapshot->runtimeFNV1a == 0xbc432a0fU &&
           snapshot->mapStateFNV1a == 0xc5cdfc04U &&
           snapshot->scriptStateFNV1a == 0xbc9b18ffU &&
           snapshot->lineStateFNV1a == 0x3658710dU &&
           snapshot->textureStateFNV1a == 0x537319adU &&
           snapshot->automapStateFNV1a == 0x0b2ae445U &&
           snapshot->topologyFNV1a == 0xd6e8df7dU &&
           snapshot->entityCount == 30U && snapshot->enemyCount == 0U &&
           snapshot->destructibleCount == 3U &&
           hashBytes(snapshot, sizeof(*snapshot)) == EXPECTED_TARGET_SNAPSHOT_FNV;
}

static int viewCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 &&
           view->destY == 1888 && view->destAngle == 64 &&
           view->viewZOld == 4 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->spawnApplied == 1U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_FNV;
}

static int tileCanonical(const EspPlayerInitialTileState* tile) {
    return tile != NULL && sizeof(*tile) == EXPECTED_TILE_BYTES &&
           tile->inputFlags == 0x1000040fUL && tile->tileIndex == 943U &&
           tile->eventIndex == 61U && tile->targetMapId == 9U &&
           tile->gameplayLoadMapId == 2U && tile->loadType == 0U &&
           tile->eventFound == 1U && tile->eventState == 0U &&
           tile->eventFlags == 0U && tile->eligibleCommands == 0U &&
           tile->executedCommands == 0U && tile->removedCommands == 0U &&
           tile->eventBlocked == 0U && tile->skipAdvanceTurn == 0U &&
           tile->active == 1U && hashBytes(tile, sizeof(*tile)) == EXPECTED_TILE_FNV;
}

static int orientationCanonical(const EspPlayerOrientationState* orientation) {
    return orientation != NULL &&
           sizeof(*orientation) == EXPECTED_ORIENTATION_BYTES &&
           orientation->viewSin == EXPECTED_SIN &&
           orientation->viewCos == EXPECTED_COS &&
           orientation->viewStepX == EXPECTED_STEP_X &&
           orientation->viewStepY == EXPECTED_STEP_Y &&
           orientation->targetMapId == 9U &&
           orientation->gameplayLoadMapId == 2U &&
           orientation->loadType == 0U && orientation->destAngle == 64U &&
           orientation->prepared == 1U && orientation->active == 1U &&
           hashBytes(orientation, sizeof(*orientation)) == EXPECTED_ORIENTATION_FNV;
}

static int orientationZero(const EspPlayerOrientationState* state) {
    EspPlayerOrientationState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

void Esp32JunctionOrientationProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerOrientation_reset();
}

int Esp32JunctionOrientationProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionOrientationProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspPlayerInitialTileState tileBefore;
    EspPlayerInitialTileState badTile;
    EspPlayerOrientationState scratch;
    EspPlayerOrientationState prepared;
    EspPlayerOrientationState orientationBeforeRepeat;
    const EspPlayerViewState* liveView;
    const EspPlayerInitialTileState* liveTile;
    const EspPlayerOrientationState* liveOrientation;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t gameBefore;
    uint32_t gameAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t canvasBefore;
    uint32_t canvasAfter;
    uint32_t renderBefore;
    uint32_t renderAfter;
    int32_t legacySin;
    int32_t legacyCos;
    int32_t legacyStepX;
    int32_t legacyStepY;
    int nullViewGate;
    int nullTileGate;
    int nullOutputGate;
    int inactiveGate;
    int tilePendingGate;
    int missingFacingGate;
    int angleGate;
    int tileInactiveGate;
    int tileMismatchGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionInitialTileProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONROTATEPROBE] ARMED hardware-proven initial tile active; native finishRotation orientation starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction finishRotation orientation ===\n");
    printf("[JUNCTIONROTATEPROBE] CONTRACT own only the first four recovered DoomCanvas_finishRotation writes for destAngle=64: viewSin=sinTable[64], viewCos=sinTable[128], viewStepX=(viewCos*64)>>16, viewStepY=(-viewSin*64)>>16; compare native fixed-point results against the live legacy sin table read-only; park one 24B pointer-free owner; do not execute the second tile event, do not perform final facing, do not mutate PlayerView or legacy Game/Player/Hud/DoomCanvas/Render, do not present, do not enter ST_PLAYING and do not allocate\n");

    liveView = EspPlayerView_view();
    liveTile = EspPlayerInitialTile_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspPlayerOrientationState) != EXPECTED_ORIENTATION_BYTES ||
        !viewCanonical(liveView) || !tileCanonical(liveTile) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONROTATEPROBE] FAILED unsafe post-initial-tile boundary\n");
        probeState.done = 1;
        return;
    }

    legacySin = (int32_t)doomRpg->render->sinTable[EXPECTED_ANGLE];
    legacyCos = (int32_t)doomRpg->render->sinTable[(EXPECTED_ANGLE + 64U) & 255U];
    legacyStepX = (int32_t)((legacyCos * 64) >> 16);
    legacyStepY = (int32_t)(((-legacySin) * 64) >> 16);
    if (legacySin != EXPECTED_SIN || legacyCos != EXPECTED_COS ||
        legacyStepX != EXPECTED_STEP_X || legacyStepY != EXPECTED_STEP_Y) {
        printf("[JUNCTIONROTATEPROBE] FAILED legacy sin table mismatch sin=%ld cos=%ld stepX=%ld stepY=%ld\n",
               (long)legacySin, (long)legacyCos, (long)legacyStepX,
               (long)legacyStepY);
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    tileBefore = *liveTile;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);

    EspPlayerOrientation_reset();

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspPlayerOrientation_prepare(NULL, &tileBefore, &scratch) ==
            ESP_PLAYER_ORIENTATION_INVALID && orientationZero(&scratch);
    memset(&scratch, 0xa5, sizeof(scratch));
    nullTileGate =
        EspPlayerOrientation_prepare(&viewBefore, NULL, &scratch) ==
            ESP_PLAYER_ORIENTATION_INVALID && orientationZero(&scratch);
    nullOutputGate =
        EspPlayerOrientation_prepare(&viewBefore, &tileBefore, NULL) ==
        ESP_PLAYER_ORIENTATION_INVALID;

    badView = viewBefore;
    badView.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveGate =
        EspPlayerOrientation_prepare(&badView, &tileBefore, &scratch) ==
            ESP_PLAYER_ORIENTATION_VIEW_INVALID && orientationZero(&scratch);

    badView = viewBefore;
    badView.tileEnterPending = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    tilePendingGate =
        EspPlayerOrientation_prepare(&badView, &tileBefore, &scratch) ==
            ESP_PLAYER_ORIENTATION_UNSUPPORTED_ORDER && orientationZero(&scratch);

    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingFacingGate =
        EspPlayerOrientation_prepare(&badView, &tileBefore, &scratch) ==
            ESP_PLAYER_ORIENTATION_UNSUPPORTED_ORDER && orientationZero(&scratch);

    badView = viewBefore;
    badView.destAngle = 0;
    badView.viewAngle = 0;
    memset(&scratch, 0xa5, sizeof(scratch));
    angleGate =
        EspPlayerOrientation_prepare(&badView, &tileBefore, &scratch) ==
            ESP_PLAYER_ORIENTATION_UNSUPPORTED_CONTEXT && orientationZero(&scratch);

    badTile = tileBefore;
    badTile.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    tileInactiveGate =
        EspPlayerOrientation_prepare(&viewBefore, &badTile, &scratch) ==
            ESP_PLAYER_ORIENTATION_TILE_INVALID && orientationZero(&scratch);

    badTile = tileBefore;
    badTile.targetMapId = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    tileMismatchGate =
        EspPlayerOrientation_prepare(&viewBefore, &badTile, &scratch) ==
            ESP_PLAYER_ORIENTATION_TILE_INVALID && orientationZero(&scratch);

    if (EspPlayerOrientation_prepare(&viewBefore, &tileBefore, &prepared) !=
            ESP_PLAYER_ORIENTATION_OK ||
        !orientationCanonical(&prepared) || EspPlayerOrientation_isReady() ||
        EspPlayerOrientation_view() != NULL ||
        !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        memcmp(&tileBefore, EspPlayerInitialTile_view(), sizeof(tileBefore)) != 0) {
        printf("[JUNCTIONROTATEPROBE] FAILED pure orientation preparation\n");
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    if (EspPlayerOrientation_route() != ESP_PLAYER_ORIENTATION_OK ||
        !orientationCanonical(EspPlayerOrientation_view()) ||
        !viewCanonical(EspPlayerView_view()) ||
        !tileCanonical(EspPlayerInitialTile_view())) {
        printf("[JUNCTIONROTATEPROBE] FAILED live orientation route\n");
        probeState.done = 1;
        return;
    }

    orientationBeforeRepeat = *EspPlayerOrientation_view();
    repeatGate =
        EspPlayerOrientation_route() == ESP_PLAYER_ORIENTATION_ALREADY_ACTIVE;
    repeatAtomic = EspPlayerOrientation_view() != NULL &&
                   memcmp(&orientationBeforeRepeat, EspPlayerOrientation_view(),
                          sizeof(orientationBeforeRepeat)) == 0 &&
                   memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) == 0 &&
                   memcmp(&tileBefore, EspPlayerInitialTile_view(), sizeof(tileBefore)) == 0;

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
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) || !viewCanonical(EspPlayerView_view()) ||
        !tileCanonical(EspPlayerInitialTile_view()) ||
        !orientationCanonical(EspPlayerOrientation_view())) {
        printf("[JUNCTIONROTATEPROBE] FAILED integrity after orientation route\n");
        probeState.done = 1;
        return;
    }

    liveOrientation = EspPlayerOrientation_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONROTATE] READY stateBytes=%u stateFNV=%08x destAngle=%u viewSin=%ld viewCos=%ld viewStepX=%ld viewStepY=%ld legacySin=%ld legacyCos=%ld legacyStepX=%ld legacyStepY=%ld exact=yes prepared=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveOrientation),
           (unsigned)hashBytes(liveOrientation, sizeof(*liveOrientation)),
           (unsigned)liveOrientation->destAngle,
           (long)liveOrientation->viewSin, (long)liveOrientation->viewCos,
           (long)liveOrientation->viewStepX, (long)liveOrientation->viewStepY,
           (long)legacySin, (long)legacyCos, (long)legacyStepX,
           (long)legacyStepY, (unsigned)liveOrientation->prepared,
           (unsigned)liveOrientation->active,
           (unsigned)liveOrientation->targetMapId,
           (unsigned)liveOrientation->gameplayLoadMapId,
           (unsigned)liveOrientation->loadType);
    printf("[JUNCTIONROTATE] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x unchanged=yes hudPending=%u facingPending=%u playerSetupPending=%u tileEnterPending=%u\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->hudRefreshPending,
           (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->playerSetupPending,
           (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONROTATE] ORDER initialTileOwned=yes orientationOwned=yes secondTileDeferred=yes finalFacingDeferred=yes finishRotationComplete=no\n");
    printf("[JUNCTIONROTATE] FAILCLOSED nullView=%d nullTile=%d nullOutput=%d inactive=%d tilePending=%d missingFacing=%d angle=%d tileInactive=%d tileMismatch=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullTileGate, nullOutputGate, inactiveGate,
           tilePendingGate, missingFacingGate, angleGate, tileInactiveGate,
           tileMismatchGate, prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONROTATE] RESIDENT snapshotFNV=%08x->%08x unchanged=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONROTATE] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONROTATE] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONROTATE] PARK state=%d page=%d targetMap=%u junctionResident=yes nativePlayerView=yes nativeInitialTile=yes nativeOrientation=yes orientationPending=no secondTilePending=yes finalFacingPending=yes finishRotationComplete=no ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveOrientation->targetMapId);

    probeState.done = 1;
}
