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
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_view_state.h"
#include "native_junction_initial_tile_probe.h"
#include "native_junction_player_setup_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_TILE_STATE_BYTES 24U
#define EXPECTED_VIEW_STATE_BYTES 44U
#define EXPECTED_VIEW_BEFORE_FNV 0xc21fba3cU
#define EXPECTED_VIEW_AFTER_FNV 0x1bd0f09bU
#define EXPECTED_SETUP_SEMANTIC_FNV 0x3b27c6a1U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_TILE_INDEX 943U
#define EXPECTED_INPUT_FLAGS 0x1000040fUL

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

static uint32_t setupSemanticFNV(const EspPlayerFreshMapState* state) {
    EspPlayerFreshMapState normalized;
    if (state == NULL) return 0U;
    normalized = *state;
    normalized.levelStartTimeMs = 0U;
    return hashBytes(&normalized, sizeof(normalized));
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

static int residentBeforeCanonical(const EspMapResidentSnapshot* snapshot) {
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

static int viewBeforeCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_STATE_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 &&
           view->destY == 1888 && view->destAngle == 64 &&
           view->viewZOld == 4 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->spawnApplied == 1U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 1U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_BEFORE_FNV;
}

static int viewAfterCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_STATE_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 &&
           view->destY == 1888 && view->destAngle == 64 &&
           view->viewZOld == 4 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->spawnApplied == 1U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_AFTER_FNV;
}

static int setupCanonical(const EspPlayerFreshMapState* setup) {
    return setup != NULL && setup->active == 1U && setup->setupApplied == 1U &&
           setup->targetMapId == 9U && setup->gameplayLoadMapId == 2U &&
           setup->loadType == 0U && setup->moves == 0U &&
           setup->xpGained == 0U && setup->berserkerTics == 0U &&
           setup->familiarActive == 0U && setup->notebookEmpty == 1U &&
           setup->weaponRestorePerformed == 0U &&
           setupSemanticFNV(setup) == EXPECTED_SETUP_SEMANTIC_FNV;
}

static void printDeferredDiscovery(uint8_t codeId, uint8_t commandOffset) {
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;

    if (EspMapEvents_findByTile(EXPECTED_TILE_INDEX, &eventRef) &&
        EspMapEvents_describe(&eventRef, &descriptor) &&
        commandOffset < descriptor.commandCount &&
        EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
        printf("[JUNCTIONTILE] DEFERRED tile=%u flags=%08x eventIndex=%u eventValue=%08x initialState=%u eventFlags=%u commandCount=%u commandOffset=%u code=%u arg1=%08x arg2=%08x failClosed=yes\n",
               (unsigned)EXPECTED_TILE_INDEX, (unsigned)EXPECTED_INPUT_FLAGS,
               (unsigned)eventRef.index, (unsigned)eventRef.value,
               (unsigned)descriptor.initialState, (unsigned)descriptor.flags,
               (unsigned)descriptor.commandCount, (unsigned)commandOffset,
               (unsigned)codeId, (unsigned)command.arg1,
               (unsigned)command.arg2);
    }
    else {
        printf("[JUNCTIONTILE] DEFERRED tile=%u flags=%08x commandOffset=%u code=%u descriptorUnavailable=yes failClosed=yes\n",
               (unsigned)EXPECTED_TILE_INDEX, (unsigned)EXPECTED_INPUT_FLAGS,
               (unsigned)commandOffset, (unsigned)codeId);
    }
}

void Esp32JunctionInitialTileProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerInitialTile_reset();
}

int Esp32JunctionInitialTileProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionInitialTileProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspPlayerFreshMapState setupBefore;
    EspPlayerFreshMapState badSetup;
    EspPlayerInitialTileState scratch;
    EspPlayerInitialTileState prepared;
    EspPlayerInitialTileState stateBeforeRepeat;
    EspPlayerViewState viewBeforeRepeat;
    EspPlayerInitialTileStatus status;
    const EspPlayerViewState* liveView;
    const EspPlayerFreshMapState* liveSetup;
    const EspPlayerInitialTileState* liveTile;
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
    uint32_t playerKeys;
    uint8_t executionBlocked;
    uint8_t deferredCode;
    uint8_t deferredOffset;
    int nullViewGate;
    int nullSetupGate;
    int nullOutputGate;
    int angleGate;
    int blockedGate;
    int missingTileGate;
    int missingFacingGate;
    int setupMismatchGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPlayerSetupProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONTILEPROBE] ARMED hardware-proven Player_setup active; bounded native initial tile-enter starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction initial tile-enter ===\n");
    printf("[JUNCTIONTILEPROBE] CONTRACT recover first fresh-map Game_executeTile at world=992/1888 tile=943 with exact angle64 flags=0x1000040f; use immutable event lookup + mutable script overlay + recovered side-effect-free filter; execute only already-supported state opcodes 11/19/20, otherwise fail closed and report exact deferred opcode; consume only tileEnterPending after a complete native dispatch; keep finishRotation/second tile/final facing/ST_PLAYING deferred; no legacy Game/Player/Hud/DoomCanvas/Render mutation, no presentation and no allocation\n");

    liveView = EspPlayerView_view();
    liveSetup = EspPlayerFreshMap_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->game->skipAdvanceTurn != false ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspPlayerInitialTileState) != EXPECTED_TILE_STATE_BYTES ||
        !viewBeforeCanonical(liveView) || !setupCanonical(liveSetup) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentBeforeCanonical(&residentBefore)) {
        printf("[JUNCTIONTILEPROBE] FAILED unsafe post-Player_setup boundary\n");
        probeState.done = 1;
        return;
    }

    playerKeys = (uint32_t)doomRpg->player->keys;
    executionBlocked = doomRpg->game->f658b ? 1U : 0U;
    if (executionBlocked != 0U) {
        printf("[JUNCTIONTILEPROBE] FAILED real fresh path has Game.f658b set\n");
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    setupBefore = *liveSetup;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);

    EspPlayerInitialTile_reset();

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspPlayerInitialTile_prepare(NULL, &setupBefore, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_INVALID;
    memset(&scratch, 0xa5, sizeof(scratch));
    nullSetupGate =
        EspPlayerInitialTile_prepare(&viewBefore, NULL, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_INVALID;
    nullOutputGate =
        EspPlayerInitialTile_prepare(&viewBefore, &setupBefore, playerKeys, 0U,
                                     NULL, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_INVALID;

    badView = viewBefore;
    badView.destAngle = 0;
    angleGate =
        EspPlayerInitialTile_prepare(&badView, &setupBefore, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_CONTEXT;
    blockedGate =
        EspPlayerInitialTile_prepare(&viewBefore, &setupBefore, playerKeys, 1U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_CONTEXT;

    badView = viewBefore;
    badView.tileEnterPending = 0U;
    missingTileGate =
        EspPlayerInitialTile_prepare(&badView, &setupBefore, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_ORDER;
    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    missingFacingGate =
        EspPlayerInitialTile_prepare(&badView, &setupBefore, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_ORDER;

    badSetup = setupBefore;
    badSetup.targetMapId = 1U;
    setupMismatchGate =
        EspPlayerInitialTile_prepare(&viewBefore, &badSetup, playerKeys, 0U,
                                     &scratch, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_SETUP_INVALID;

    deferredCode = 0U;
    deferredOffset = 0U;
    status = EspPlayerInitialTile_prepare(
        &viewBefore, &setupBefore, playerKeys, executionBlocked, &prepared,
        &deferredCode, &deferredOffset);

    if (!EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        memcmp(&setupBefore, EspPlayerFreshMap_view(), sizeof(setupBefore)) != 0 ||
        EspPlayerInitialTile_isReady()) {
        printf("[JUNCTIONTILEPROBE] FAILED prepare mutated live ownership\n");
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    if (status == ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED) {
        printDeferredDiscovery(deferredCode, deferredOffset);
        printf("[JUNCTIONTILE] DISCOVERY playerKeys=%08x viewFNV=%08x setupSemanticFNV=%08x scriptFNV=%08x tilePending=%u facingPending=%u noMutation=yes\n",
               (unsigned)playerKeys,
               (unsigned)hashBytes(&viewBefore, sizeof(viewBefore)),
               (unsigned)setupSemanticFNV(&setupBefore),
               (unsigned)residentBefore.scriptStateFNV1a,
               (unsigned)viewBefore.tileEnterPending,
               (unsigned)viewBefore.facingRefreshPending);
        printf("[JUNCTIONTILE] FAILCLOSED nullView=%d nullSetup=%d nullOutput=%d angle=%d blocked=%d missingTile=%d missingFacing=%d setupMismatch=%d prepareAtomic=%s\n",
               nullViewGate, nullSetupGate, nullOutputGate, angleGate,
               blockedGate, missingTileGate, missingFacingGate,
               setupMismatchGate, prepareAtomic ? "yes" : "no");
        printf("[JUNCTIONTILE] PARK deferredOpcode=%u tileEnterPending=yes finishRotationPending=yes finalFacingPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
               (unsigned)deferredCode);
        probeState.done = 1;
        return;
    }

    if (status != ESP_PLAYER_INITIAL_TILE_OK || prepared.active != 1U ||
        prepared.tileIndex != EXPECTED_TILE_INDEX ||
        prepared.inputFlags != EXPECTED_INPUT_FLAGS ||
        prepared.targetMapId != 9U || prepared.gameplayLoadMapId != 2U ||
        prepared.loadType != 0U || prepared.skipAdvanceTurn != 0U) {
        printf("[JUNCTIONTILEPROBE] FAILED pure initial tile preparation status=%u deferredCode=%u offset=%u\n",
               (unsigned)status, (unsigned)deferredCode,
               (unsigned)deferredOffset);
        probeState.done = 1;
        return;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    status = EspPlayerInitialTile_route(playerKeys, executionBlocked,
                                        &deferredCode, &deferredOffset);
    if (status != ESP_PLAYER_INITIAL_TILE_OK ||
        !viewAfterCanonical(EspPlayerView_view()) ||
        !setupCanonical(EspPlayerFreshMap_view()) ||
        !EspPlayerInitialTile_isReady() || EspPlayerInitialTile_view() == NULL) {
        printf("[JUNCTIONTILEPROBE] FAILED live initial tile routing status=%u deferredCode=%u offset=%u\n",
               (unsigned)status, (unsigned)deferredCode,
               (unsigned)deferredOffset);
        probeState.done = 1;
        return;
    }

    liveTile = EspPlayerInitialTile_view();
    stateBeforeRepeat = *liveTile;
    viewBeforeRepeat = *EspPlayerView_view();
    repeatGate =
        EspPlayerInitialTile_route(playerKeys, executionBlocked, NULL, NULL) ==
        ESP_PLAYER_INITIAL_TILE_ALREADY_ACTIVE;
    repeatAtomic =
        memcmp(&stateBeforeRepeat, EspPlayerInitialTile_view(),
               sizeof(stateBeforeRepeat)) == 0 &&
        memcmp(&viewBeforeRepeat, EspPlayerView_view(),
               sizeof(viewBeforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentNonScriptStable(&residentBefore, &residentAfter) ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        !viewAfterCanonical(EspPlayerView_view()) ||
        !setupCanonical(EspPlayerFreshMap_view())) {
        printf("[JUNCTIONTILEPROBE] FAILED integrity after initial tile routing\n");
        probeState.done = 1;
        return;
    }

    liveTile = EspPlayerInitialTile_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONTILE] READY stateBytes=%u stateFNV=%08x tile=%u flags=%08x eventFound=%u eventIndex=%u eventState=%u eventFlags=%u blocked=%u eligible=%u executed=%u removed=%u skipAdvanceTurn=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u playerKeys=%08x\n",
           (unsigned)sizeof(*liveTile),
           (unsigned)hashBytes(liveTile, sizeof(*liveTile)),
           (unsigned)liveTile->tileIndex, (unsigned)liveTile->inputFlags,
           (unsigned)liveTile->eventFound, (unsigned)liveTile->eventIndex,
           (unsigned)liveTile->eventState, (unsigned)liveTile->eventFlags,
           (unsigned)liveTile->eventBlocked,
           (unsigned)liveTile->eligibleCommands,
           (unsigned)liveTile->executedCommands,
           (unsigned)liveTile->removedCommands,
           (unsigned)liveTile->skipAdvanceTurn, (unsigned)liveTile->active,
           (unsigned)liveTile->targetMapId,
           (unsigned)liveTile->gameplayLoadMapId,
           (unsigned)liveTile->loadType, (unsigned)playerKeys);
    printf("[JUNCTIONTILE] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x hudPending=%u facingPending=%u playerSetupPending=%u tileEnterPending=%u placementExact=yes\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_BEFORE_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->hudRefreshPending,
           (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->playerSetupPending,
           (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONTILE] SCRIPT beforeFNV=%08x afterFNV=%08x changed=%s immutableRuntimeFNV=%08x mapStateFNV=%08x lineFNV=%08x textureFNV=%08x automapFNV=%08x topologyFNV=%08x\n",
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
    printf("[JUNCTIONTILE] ORDER hudOwned=yes playerSetupOwned=yes initialTileOwned=yes finishRotationDeferred=yes secondTileDeferred=yes finalFacingDeferred=yes\n");
    printf("[JUNCTIONTILE] FAILCLOSED nullView=%d nullSetup=%d nullOutput=%d angle=%d blocked=%d missingTile=%d missingFacing=%d setupMismatch=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullSetupGate, nullOutputGate, angleGate,
           blockedGate, missingTileGate, missingFacingGate, setupMismatchGate,
           prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONTILE] RESIDENT runtimeStable=yes nonScriptMutableStable=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONTILE] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONTILE] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONTILE] PARK state=%d page=%d targetMap=9 junctionResident=yes nativePlayerView=yes nativePlayerSetup=yes nativeInitialTile=yes tileEnterPending=no facingPending=yes finishRotationPending=yes secondTilePending=yes finalFacingPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
