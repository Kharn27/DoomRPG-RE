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
#include "esp_map_events.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_view_state.h"
#include "native_junction_finish_rotation_tile_probe.h"
#include "native_junction_orientation_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_SECOND_TILE_BYTES 24U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0x1bd0f09bU
#define EXPECTED_INITIAL_TILE_BYTES 24U
#define EXPECTED_INITIAL_TILE_FNV 0xf73e28b2U
#define EXPECTED_ORIENTATION_BYTES 24U
#define EXPECTED_ORIENTATION_FNV 0xacc754a6U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_TILE_INDEX 943U
#define EXPECTED_INPUT_FLAGS 0x10000400UL

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
    uint32_t values[12];
    if (game == NULL) return 0U;
    values[0] = (uint32_t)game->spawnParam;
    values[1] = (uint32_t)game->isLoaded;
    values[2] = (uint32_t)game->activeLoadType;
    values[3] = (uint32_t)game->numEntities;
    values[4] = (uint32_t)game->numMonsters;
    values[5] = (uint32_t)game->skipAdvanceTurn;
    values[6] = (uint32_t)game->f658b;
    values[7] = (uint32_t)game->waitTime;
    values[8] = (uint32_t)game->tileEvent;
    values[9] = (uint32_t)game->tileEventIndex;
    values[10] = (uint32_t)game->tileEventFlags;
    values[11] = (uint32_t)game->saveTileEvent;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t values[12];
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
    values[9] = (uint32_t)player->credits;
    values[10] = (uint32_t)player->completedLevels;
    values[11] = (uint32_t)(player->dogFamiliar != NULL);
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

static int residentNonScriptStable(const EspMapResidentSnapshot* before,
                                   const EspMapResidentSnapshot* after) {
    return before != NULL && after != NULL &&
           before->runtimeArenaBytes == after->runtimeArenaBytes &&
           before->mapStateBytes == after->mapStateBytes &&
           before->scriptStateBytes == after->scriptStateBytes &&
           before->lineStateBytes == after->lineStateBytes &&
           before->textureStateBytes == after->textureStateBytes &&
           before->automapStateBytes == after->automapStateBytes &&
           before->topologyBytes == after->topologyBytes &&
           before->totalPayloadBytes == after->totalPayloadBytes &&
           before->runtimeFNV1a == after->runtimeFNV1a &&
           before->mapStateFNV1a == after->mapStateFNV1a &&
           before->lineStateFNV1a == after->lineStateFNV1a &&
           before->textureStateFNV1a == after->textureStateFNV1a &&
           before->automapStateFNV1a == after->automapStateFNV1a &&
           before->topologyFNV1a == after->topologyFNV1a &&
           before->nodeCount == after->nodeCount &&
           before->lineCount == after->lineCount &&
           before->spriteCount == after->spriteCount &&
           before->eventCount == after->eventCount &&
           before->byteCodeCount == after->byteCodeCount &&
           before->stringCount == after->stringCount &&
           before->entityCount == after->entityCount &&
           before->enemyCount == after->enemyCount &&
           before->destructibleCount == after->destructibleCount;
}

static int viewCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 && view->destY == 1888 &&
           view->destAngle == 64 && view->viewZOld == 4 &&
           view->targetMapId == 9U && view->gameplayLoadMapId == 2U &&
           view->loadType == 0U && view->spawnApplied == 1U &&
           view->hudRefreshPending == 0U && view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_FNV;
}

static int initialTileCanonical(const EspPlayerInitialTileState* tile) {
    return tile != NULL && sizeof(*tile) == EXPECTED_INITIAL_TILE_BYTES &&
           tile->inputFlags == 0x1000040fUL && tile->tileIndex == 943U &&
           tile->eventIndex == 61U && tile->targetMapId == 9U &&
           tile->gameplayLoadMapId == 2U && tile->loadType == 0U &&
           tile->eventFound == 1U && tile->eventState == 0U &&
           tile->eventFlags == 0U && tile->eligibleCommands == 0U &&
           tile->executedCommands == 0U && tile->removedCommands == 0U &&
           tile->eventBlocked == 0U && tile->skipAdvanceTurn == 0U &&
           tile->active == 1U &&
           hashBytes(tile, sizeof(*tile)) == EXPECTED_INITIAL_TILE_FNV;
}

static int orientationCanonical(const EspPlayerOrientationState* orientation) {
    return orientation != NULL && sizeof(*orientation) == EXPECTED_ORIENTATION_BYTES &&
           orientation->viewSin == 65536 && orientation->viewCos == 0 &&
           orientation->viewStepX == 0 && orientation->viewStepY == -64 &&
           orientation->targetMapId == 9U &&
           orientation->gameplayLoadMapId == 2U && orientation->loadType == 0U &&
           orientation->destAngle == 64U && orientation->prepared == 1U &&
           orientation->active == 1U &&
           hashBytes(orientation, sizeof(*orientation)) == EXPECTED_ORIENTATION_FNV;
}

static int secondTileCanonical(const EspPlayerFinishRotationTileState* tile) {
    return tile != NULL && sizeof(*tile) == EXPECTED_SECOND_TILE_BYTES &&
           tile->inputFlags == EXPECTED_INPUT_FLAGS &&
           tile->tileIndex == EXPECTED_TILE_INDEX && tile->targetMapId == 9U &&
           tile->gameplayLoadMapId == 2U && tile->loadType == 0U &&
           tile->skipAdvanceTurn == 0U && tile->active == 1U;
}

static void printDeferredDiscovery(uint8_t codeId, uint8_t commandOffset) {
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;

    if (EspMapEvents_findByTile(EXPECTED_TILE_INDEX, &eventRef) &&
        EspMapEvents_describe(&eventRef, &descriptor) &&
        commandOffset < descriptor.commandCount &&
        EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
        printf("[JUNCTIONTILE2] DEFERRED tile=%u flags=%08x eventIndex=%u eventValue=%08x initialState=%u eventFlags=%u commandCount=%u commandOffset=%u code=%u arg1=%08x arg2=%08x failClosed=yes\n",
               (unsigned)EXPECTED_TILE_INDEX, (unsigned)EXPECTED_INPUT_FLAGS,
               (unsigned)eventRef.index, (unsigned)eventRef.value,
               (unsigned)descriptor.initialState, (unsigned)descriptor.flags,
               (unsigned)descriptor.commandCount, (unsigned)commandOffset,
               (unsigned)codeId, (unsigned)command.arg1,
               (unsigned)command.arg2);
    }
    else {
        printf("[JUNCTIONTILE2] DEFERRED tile=%u flags=%08x commandOffset=%u code=%u descriptorUnavailable=yes failClosed=yes\n",
               (unsigned)EXPECTED_TILE_INDEX, (unsigned)EXPECTED_INPUT_FLAGS,
               (unsigned)commandOffset, (unsigned)codeId);
    }
}

void Esp32JunctionFinishRotationTileProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerFinishRotationTile_reset();
}

int Esp32JunctionFinishRotationTileProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionFinishRotationTileProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspPlayerInitialTileState initialBefore;
    EspPlayerInitialTileState badInitial;
    EspPlayerOrientationState orientationBefore;
    EspPlayerOrientationState badOrientation;
    EspPlayerFinishRotationTileState scratch;
    EspPlayerFinishRotationTileState prepared;
    EspPlayerFinishRotationTileState stateBeforeRepeat;
    EspPlayerFinishRotationTileStatus status;
    const EspPlayerViewState* liveView;
    const EspPlayerInitialTileState* liveInitial;
    const EspPlayerOrientationState* liveOrientation;
    const EspPlayerFinishRotationTileState* liveSecondTile;
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
    uint32_t playerKeys;
    uint8_t executionBlocked;
    uint8_t deferredCode;
    uint8_t deferredOffset;
    int nullViewGate;
    int nullInitialGate;
    int nullOrientationGate;
    int nullOutputGate;
    int inactiveGate;
    int tilePendingGate;
    int missingFacingGate;
    int angleGate;
    int blockedGate;
    int initialMismatchGate;
    int orientationInactiveGate;
    int orientationMismatchGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionOrientationProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONTILE2PROBE] ARMED hardware-proven finishRotation orientation active; bounded second tile dispatch starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction finishRotation second tile ===\n");
    printf("[JUNCTIONTILE2PROBE] CONTRACT recover only the second Game_executeTile at world=992/1888 tile=943 with angle64 flags=0x10000400 after hardware-proven orientation; use immutable event lookup + mutable script overlay + recovered side-effect-free filter; execute only already-supported state opcodes 11/19/20, otherwise fail closed and report the exact deferred opcode; keep final durable facing and ST_PLAYING deferred; do not mutate PlayerView/InitialTile/Orientation or legacy Game/Player/Hud/DoomCanvas/Render, do not present and do not allocate\n");

    liveView = EspPlayerView_view();
    liveInitial = EspPlayerInitialTile_view();
    liveOrientation = EspPlayerOrientation_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->game->skipAdvanceTurn != false ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspPlayerFinishRotationTileState) != EXPECTED_SECOND_TILE_BYTES ||
        !viewCanonical(liveView) || !initialTileCanonical(liveInitial) ||
        !orientationCanonical(liveOrientation) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONTILE2PROBE] FAILED unsafe post-orientation boundary\n");
        probeState.done = 1;
        return;
    }

    playerKeys = (uint32_t)doomRpg->player->keys;
    executionBlocked = doomRpg->game->f658b ? 1U : 0U;
    if (executionBlocked != 0U) {
        printf("[JUNCTIONTILE2PROBE] FAILED real fresh path has Game.f658b set\n");
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    initialBefore = *liveInitial;
    orientationBefore = *liveOrientation;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);

    EspPlayerFinishRotationTile_reset();

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspPlayerFinishRotationTile_prepare(
            NULL, &initialBefore, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) == ESP_PLAYER_FINISH_ROTATION_TILE_INVALID;
    memset(&scratch, 0xa5, sizeof(scratch));
    nullInitialGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, NULL, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) == ESP_PLAYER_FINISH_ROTATION_TILE_INVALID;
    memset(&scratch, 0xa5, sizeof(scratch));
    nullOrientationGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &initialBefore, NULL, playerKeys, 0U,
            &scratch, NULL, NULL) == ESP_PLAYER_FINISH_ROTATION_TILE_INVALID;
    nullOutputGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &initialBefore, &orientationBefore, playerKeys, 0U,
            NULL, NULL, NULL) == ESP_PLAYER_FINISH_ROTATION_TILE_INVALID;

    badView = viewBefore;
    badView.active = 0U;
    inactiveGate =
        EspPlayerFinishRotationTile_prepare(
            &badView, &initialBefore, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) == ESP_PLAYER_FINISH_ROTATION_TILE_VIEW_INVALID;

    badView = viewBefore;
    badView.tileEnterPending = 1U;
    tilePendingGate =
        EspPlayerFinishRotationTile_prepare(
            &badView, &initialBefore, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_ORDER;

    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    missingFacingGate =
        EspPlayerFinishRotationTile_prepare(
            &badView, &initialBefore, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_ORDER;

    badView = viewBefore;
    badView.destAngle = 0;
    badView.viewAngle = 0;
    angleGate =
        EspPlayerFinishRotationTile_prepare(
            &badView, &initialBefore, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_ORIENTATION_INVALID;

    blockedGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &initialBefore, &orientationBefore, playerKeys, 1U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_CONTEXT;

    badInitial = initialBefore;
    badInitial.targetMapId = 1U;
    initialMismatchGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &badInitial, &orientationBefore, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_INITIAL_INVALID;

    badOrientation = orientationBefore;
    badOrientation.active = 0U;
    orientationInactiveGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &initialBefore, &badOrientation, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_ORIENTATION_INVALID;

    badOrientation = orientationBefore;
    badOrientation.viewStepY = -63;
    orientationMismatchGate =
        EspPlayerFinishRotationTile_prepare(
            &viewBefore, &initialBefore, &badOrientation, playerKeys, 0U,
            &scratch, NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_CONTEXT;

    deferredCode = 0U;
    deferredOffset = 0U;
    status = EspPlayerFinishRotationTile_prepare(
        &viewBefore, &initialBefore, &orientationBefore, playerKeys,
        executionBlocked, &prepared, &deferredCode, &deferredOffset);

    if (!EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        memcmp(&initialBefore, EspPlayerInitialTile_view(),
               sizeof(initialBefore)) != 0 ||
        memcmp(&orientationBefore, EspPlayerOrientation_view(),
               sizeof(orientationBefore)) != 0 ||
        EspPlayerFinishRotationTile_isReady()) {
        printf("[JUNCTIONTILE2PROBE] FAILED prepare mutated live ownership\n");
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    if (status == ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED) {
        printDeferredDiscovery(deferredCode, deferredOffset);
        printf("[JUNCTIONTILE2] DISCOVERY playerKeys=%08x viewFNV=%08x initialTileFNV=%08x orientationFNV=%08x scriptFNV=%08x facingPending=%u noMutation=yes\n",
               (unsigned)playerKeys,
               (unsigned)hashBytes(&viewBefore, sizeof(viewBefore)),
               (unsigned)hashBytes(&initialBefore, sizeof(initialBefore)),
               (unsigned)hashBytes(&orientationBefore, sizeof(orientationBefore)),
               (unsigned)residentBefore.scriptStateFNV1a,
               (unsigned)viewBefore.facingRefreshPending);
        printf("[JUNCTIONTILE2] FAILCLOSED nullView=%d nullInitial=%d nullOrientation=%d nullOutput=%d inactive=%d tilePending=%d missingFacing=%d angle=%d blocked=%d initialMismatch=%d orientationInactive=%d orientationMismatch=%d prepareAtomic=%s\n",
               nullViewGate, nullInitialGate, nullOrientationGate,
               nullOutputGate, inactiveGate, tilePendingGate,
               missingFacingGate, angleGate, blockedGate,
               initialMismatchGate, orientationInactiveGate,
               orientationMismatchGate, prepareAtomic ? "yes" : "no");
        printf("[JUNCTIONTILE2] PARK deferredOpcode=%u secondTilePending=yes finalFacingPending=yes finishRotationComplete=no ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
               (unsigned)deferredCode);
        probeState.done = 1;
        return;
    }

    if (status != ESP_PLAYER_FINISH_ROTATION_TILE_OK ||
        !secondTileCanonical(&prepared)) {
        printf("[JUNCTIONTILE2PROBE] FAILED pure second tile preparation status=%u deferredCode=%u offset=%u\n",
               (unsigned)status, (unsigned)deferredCode,
               (unsigned)deferredOffset);
        probeState.done = 1;
        return;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    status = EspPlayerFinishRotationTile_route(
        playerKeys, executionBlocked, &deferredCode, &deferredOffset);
    if (status != ESP_PLAYER_FINISH_ROTATION_TILE_OK ||
        !EspPlayerFinishRotationTile_isReady() ||
        !secondTileCanonical(EspPlayerFinishRotationTile_view()) ||
        !viewCanonical(EspPlayerView_view()) ||
        !initialTileCanonical(EspPlayerInitialTile_view()) ||
        !orientationCanonical(EspPlayerOrientation_view())) {
        printf("[JUNCTIONTILE2PROBE] FAILED live second tile routing status=%u deferredCode=%u offset=%u\n",
               (unsigned)status, (unsigned)deferredCode,
               (unsigned)deferredOffset);
        probeState.done = 1;
        return;
    }

    stateBeforeRepeat = *EspPlayerFinishRotationTile_view();
    repeatGate =
        EspPlayerFinishRotationTile_route(playerKeys, executionBlocked,
                                          NULL, NULL) ==
        ESP_PLAYER_FINISH_ROTATION_TILE_ALREADY_ACTIVE;
    repeatAtomic =
        memcmp(&stateBeforeRepeat, EspPlayerFinishRotationTile_view(),
               sizeof(stateBeforeRepeat)) == 0 &&
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) == 0 &&
        memcmp(&initialBefore, EspPlayerInitialTile_view(),
               sizeof(initialBefore)) == 0 &&
        memcmp(&orientationBefore, EspPlayerOrientation_view(),
               sizeof(orientationBefore)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentNonScriptStable(&residentBefore, &residentAfter) ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        !viewCanonical(EspPlayerView_view()) ||
        !initialTileCanonical(EspPlayerInitialTile_view()) ||
        !orientationCanonical(EspPlayerOrientation_view()) ||
        !secondTileCanonical(EspPlayerFinishRotationTile_view())) {
        printf("[JUNCTIONTILE2PROBE] FAILED integrity after second tile routing\n");
        probeState.done = 1;
        return;
    }

    liveSecondTile = EspPlayerFinishRotationTile_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONTILE2] READY stateBytes=%u stateFNV=%08x tile=%u flags=%08x eventFound=%u eventIndex=%u eventState=%u eventFlags=%u blocked=%u eligible=%u executed=%u removed=%u skipAdvanceTurn=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u playerKeys=%08x\n",
           (unsigned)sizeof(*liveSecondTile),
           (unsigned)hashBytes(liveSecondTile, sizeof(*liveSecondTile)),
           (unsigned)liveSecondTile->tileIndex,
           (unsigned)liveSecondTile->inputFlags,
           (unsigned)liveSecondTile->eventFound,
           (unsigned)liveSecondTile->eventIndex,
           (unsigned)liveSecondTile->eventState,
           (unsigned)liveSecondTile->eventFlags,
           (unsigned)liveSecondTile->eventBlocked,
           (unsigned)liveSecondTile->eligibleCommands,
           (unsigned)liveSecondTile->executedCommands,
           (unsigned)liveSecondTile->removedCommands,
           (unsigned)liveSecondTile->skipAdvanceTurn,
           (unsigned)liveSecondTile->active,
           (unsigned)liveSecondTile->targetMapId,
           (unsigned)liveSecondTile->gameplayLoadMapId,
           (unsigned)liveSecondTile->loadType,
           (unsigned)playerKeys);
    printf("[JUNCTIONTILE2] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x unchanged=yes facingPending=%u tileEnterPending=%u\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONTILE2] OWNERS initialTileFNV=%08x orientationFNV=%08x unchanged=yes secondTileOwned=yes finalFacingDeferred=yes finishRotationComplete=no\n",
           (unsigned)hashBytes(EspPlayerInitialTile_view(),
                               sizeof(*EspPlayerInitialTile_view())),
           (unsigned)hashBytes(EspPlayerOrientation_view(),
                               sizeof(*EspPlayerOrientation_view())));
    printf("[JUNCTIONTILE2] SCRIPT beforeFNV=%08x afterFNV=%08x changed=%s immutableRuntimeFNV=%08x mapStateFNV=%08x lineFNV=%08x textureFNV=%08x automapFNV=%08x topologyFNV=%08x\n",
           (unsigned)residentBefore.scriptStateFNV1a,
           (unsigned)residentAfter.scriptStateFNV1a,
           residentBefore.scriptStateFNV1a == residentAfter.scriptStateFNV1a ?
               "no" : "yes",
           (unsigned)residentAfter.runtimeFNV1a,
           (unsigned)residentAfter.mapStateFNV1a,
           (unsigned)residentAfter.lineStateFNV1a,
           (unsigned)residentAfter.textureStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.topologyFNV1a);
    printf("[JUNCTIONTILE2] FAILCLOSED nullView=%d nullInitial=%d nullOrientation=%d nullOutput=%d inactive=%d tilePending=%d missingFacing=%d angle=%d blocked=%d initialMismatch=%d orientationInactive=%d orientationMismatch=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullInitialGate, nullOrientationGate,
           nullOutputGate, inactiveGate, tilePendingGate, missingFacingGate,
           angleGate, blockedGate, initialMismatchGate,
           orientationInactiveGate, orientationMismatchGate,
           prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONTILE2] RESIDENT nonScriptStable=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONTILE2] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONTILE2] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONTILE2] PARK state=%d page=%d targetMap=%u junctionResident=yes nativePlayerView=yes nativeInitialTile=yes nativeOrientation=yes nativeSecondTile=yes secondTilePending=no finalFacingPending=yes finishRotationComplete=no ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveSecondTile->targetMapId);

    probeState.done = 1;
}
