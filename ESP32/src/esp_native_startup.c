#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_catalog.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_session.h"
#include "esp_player_facing_state.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"

/*
 * Permanent new-game bootstrap.
 *
 * Historical MAP1/Entrance probes used to build the resident owner set one
 * milestone at a time. That was useful evidence while the native engine was
 * being recovered, but it is not a production architecture. The permanent
 * runtime already owns a generic resident loader and generic spawn primitives;
 * this bridge composes those APIs directly.
 *
 * New BSPs therefore do not require another lifecycle bridge. Map identity is
 * resolved through EspMapCatalog, resident data is loaded from the native pack,
 * and all subsequent gameplay selects its map from EspPlayerView/runtime state.
 */

typedef enum EspNativeStartupStage_e {
    ESP_NATIVE_STARTUP_WAIT_INTRO = 0,
    ESP_NATIVE_STARTUP_RESIDENT_READY = 1,
    ESP_NATIVE_STARTUP_GAMEPLAY_READY = 2,
    ESP_NATIVE_STARTUP_FAILED = 255
} EspNativeStartupStage;

typedef struct EspNativeStartupState_s {
    uint8_t stage;
    uint8_t targetMapId;
    uint8_t waitLogged;
    uint8_t reserved;
} EspNativeStartupState;

static EspNativeStartupState startupState;

void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL &&
           canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->tileEvents == NULL &&
           render->mapByteCode == NULL &&
           render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int startupBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap >= MAP_INTRO &&
           canvas->startupMap <= MAP_END_GAME &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0 &&
           !EspAssetPack_isOpen();
}

static void resetSpawnOwners(void) {
    EspPlayerView_reset();
    EspHudRefresh_reset();
    EspPlayerFreshMap_reset();
    EspPlayerInitialTile_reset();
    EspPlayerOrientation_reset();
    EspPlayerFinishRotationTile_reset();
    EspPlayerFacing_reset();
    EspHudPostLoadClear_reset();
    EspNativeGameplayDispatch_reset();
}

static void failStartup(const char* reason) {
    startupState.stage = ESP_NATIVE_STARTUP_FAILED;
    printf("[NATIVEBOOT] FAILED reason=%s map=%u resident=%d packOpen=%d\n",
           reason != NULL ? reason : "unknown",
           (unsigned int)startupState.targetMapId,
           EspMapResidentLifecycle_isReady(),
           EspAssetPack_isOpen());
}

static int loadStartupResident(DoomRPG_t* doomRpg) {
    EspBspInventory inventory;
    EspMapResidentSnapshot snapshot;
    EspMapResidentLifecycleStatus status;
    const char* resourceName;
    uint8_t targetMapId = 0U;

    if (!startupBoundaryIsSafe(doomRpg) ||
        !EspMapResidentLifecycle_isEmpty()) {
        return 0;
    }

    resourceName = doomRpg->game->mapFiles[doomRpg->doomCanvas->startupMap - 1];
    if (resourceName == NULL || resourceName[0] == '\0' ||
        !EspMapCatalog_idForName(resourceName, &targetMapId) ||
        !EspMapCatalog_isValidId(targetMapId)) {
        failStartup("startup map is not in native catalog");
        return -1;
    }

    memset(&inventory, 0, sizeof(inventory));
    memset(&snapshot, 0, sizeof(snapshot));
    if (!EspBspReader_inventoryPackEntry(resourceName, &inventory) ||
        inventory.sourceBytes == 0U ||
        inventory.consumedBytes != inventory.sourceBytes ||
        inventory.trailingBytes != 0U ||
        inventory.crc32 == 0U ||
        inventory.crc32 != inventory.expectedCrc32 ||
        inventory.plan.persistentBytes == 0U ||
        EspAssetPack_isOpen()) {
        failStartup("native BSP inventory");
        return -1;
    }

    status = EspMapResidentLifecycle_loadFromEmpty(
        resourceName, &inventory, &snapshot);
    if (status != ESP_MAP_RESIDENT_OK ||
        !EspMapResidentLifecycle_isReady() ||
        snapshot.runtimeArenaBytes != inventory.plan.persistentBytes ||
        snapshot.nodeCount != inventory.nodes ||
        snapshot.lineCount != inventory.lines ||
        snapshot.spriteCount != inventory.mapSprites ||
        snapshot.eventCount != inventory.events ||
        snapshot.byteCodeCount != inventory.byteCodes ||
        snapshot.stringCount != inventory.strings ||
        EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0) {
        failStartup("generic resident load");
        return -1;
    }

    startupState.targetMapId = targetMapId;
    startupState.stage = ESP_NATIVE_STARTUP_RESIDENT_READY;
    printf("[NATIVEBOOT] RESIDENT map=%u file=%s source=%u crc=%08x arena=%u payload=%u nodes=%u lines=%u sprites=%u events=%u byteCodes=%u strings=%u entities=%u enemies=%u destructibles=%u\n",
           (unsigned int)targetMapId,
           resourceName,
           (unsigned int)inventory.sourceBytes,
           (unsigned int)inventory.crc32,
           (unsigned int)snapshot.runtimeArenaBytes,
           (unsigned int)snapshot.totalPayloadBytes,
           (unsigned int)snapshot.nodeCount,
           (unsigned int)snapshot.lineCount,
           (unsigned int)snapshot.spriteCount,
           (unsigned int)snapshot.eventCount,
           (unsigned int)snapshot.byteCodeCount,
           (unsigned int)snapshot.stringCount,
           (unsigned int)snapshot.entityCount,
           (unsigned int)snapshot.enemyCount,
           (unsigned int)snapshot.destructibleCount);
    return 1;
}

static int routeInitialSpawn(DoomRPG_t* doomRpg) {
    EspBspInventory inventory;
    EspMapResidentSnapshot resident;
    EspPlayerSpawnState spawn;
    EspPlayerSpawnStatus spawnStatus;
    EspPlayerViewApplyStatus viewStatus;
    EspHudRefreshStatus hudStatus;
    EspPlayerFreshMapStatus freshStatus;
    EspPlayerInitialTileStatus firstTileStatus;
    EspPlayerOrientationStatus orientationStatus;
    EspPlayerFinishRotationTileStatus secondTileStatus;
    EspPlayerFacingStatus facingStatus;
    EspHudPostLoadClearStatus clearStatus;
    const EspPlayerViewState* view;
    const EspNativeGameplayTurnState* turn;
    const char* resourceName;
    uint8_t deferredCode = 0U;
    uint8_t deferredOffset = 0U;

    if (doomRpg == NULL || doomRpg->render == NULL || doomRpg->game == NULL ||
        startupState.stage != ESP_NATIVE_STARTUP_RESIDENT_READY ||
        !EspMapCatalog_isValidId(startupState.targetMapId) ||
        !EspMapResidentLifecycle_capture(&resident) ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen()) {
        failStartup("unsafe spawn boundary");
        return 0;
    }

    resourceName = EspMapCatalog_nameForId(startupState.targetMapId);
    memset(&inventory, 0, sizeof(inventory));
    memset(&spawn, 0, sizeof(spawn));
    if (resourceName == NULL ||
        !EspBspReader_inventoryPackEntry(resourceName, &inventory) ||
        EspAssetPack_isOpen()) {
        failStartup("spawn inventory");
        return 0;
    }

    spawnStatus = EspPlayerSpawn_prepareInitial(
        startupState.targetMapId,
        &inventory,
        ESP_PLAYER_SPAWN_LOAD_FRESH_MAP,
        0U,
        &spawn);
    if (spawnStatus != ESP_PLAYER_SPAWN_OK) {
        failStartup("initial spawn preparation");
        return 0;
    }

    viewStatus = EspPlayerView_applySpawn(&spawn);
    hudStatus = viewStatus == ESP_PLAYER_VIEW_APPLY_OK
                    ? EspHudRefresh_routePostSpawn()
                    : ESP_HUD_REFRESH_VIEW_INVALID;
    freshStatus = hudStatus == ESP_HUD_REFRESH_OK
                      ? EspPlayerFreshMap_route(DoomRPG_GetUpTimeMS(), 0U)
                      : ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    if (viewStatus != ESP_PLAYER_VIEW_APPLY_OK ||
        hudStatus != ESP_HUD_REFRESH_OK ||
        freshStatus != ESP_PLAYER_FRESH_MAP_OK) {
        failStartup("post-spawn setup");
        return 0;
    }

    firstTileStatus = EspPlayerInitialTile_route(
        0U, 0U, &deferredCode, &deferredOffset);
    if (firstTileStatus != ESP_PLAYER_INITIAL_TILE_OK) {
        printf("[NATIVEBOOT] BLOCKED stage=FIRST_TILE status=%u opcode=%u commandOffset=%u\n",
               (unsigned int)firstTileStatus,
               (unsigned int)deferredCode,
               (unsigned int)deferredOffset);
        failStartup("unsupported initial tile event");
        return 0;
    }

    orientationStatus = EspPlayerOrientation_route();
    if (orientationStatus != ESP_PLAYER_ORIENTATION_OK) {
        failStartup("initial orientation");
        return 0;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    secondTileStatus = EspPlayerFinishRotationTile_route(
        0U, 0U, &deferredCode, &deferredOffset);
    if (secondTileStatus != ESP_PLAYER_FINISH_ROTATION_TILE_OK) {
        printf("[NATIVEBOOT] BLOCKED stage=SECOND_TILE status=%u opcode=%u commandOffset=%u\n",
               (unsigned int)secondTileStatus,
               (unsigned int)deferredCode,
               (unsigned int)deferredOffset);
        failStartup("unsupported finish-rotation tile event");
        return 0;
    }

    facingStatus = EspPlayerFacing_route();
    clearStatus = facingStatus == ESP_PLAYER_FACING_OK
                      ? EspHudPostLoadClear_route()
                      : ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;
    if (facingStatus != ESP_PLAYER_FACING_OK ||
        clearStatus != ESP_HUD_POST_LOAD_CLEAR_OK ||
        !EspNativeGameplayDispatch_adoptView()) {
        failStartup("final spawn ownership");
        return 0;
    }

    view = EspPlayerView_view();
    turn = EspNativeGameplayDispatch_view();
    if (view == NULL || turn == NULL || view->active != 1U ||
        view->targetMapId != startupState.targetMapId ||
        view->hudRefreshPending != 0U ||
        view->facingRefreshPending != 0U ||
        view->playerSetupPending != 0U ||
        view->tileEnterPending != 0U ||
        view->viewAngle != view->destAngle ||
        EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render)) {
        failStartup("settled player view");
        return 0;
    }

    startupState.stage = ESP_NATIVE_STARTUP_GAMEPLAY_READY;
    printf("[NATIVEBOOT] READY map=%u gameplayLoadMapId=%u spawnTile=%u pos=%u,%u angle=%u step=%d,%d genericResident=yes genericSpawn=yes probesRequired=no shapeData=%p mediaTexels=%p\n",
           (unsigned int)view->targetMapId,
           (unsigned int)view->gameplayLoadMapId,
           (unsigned int)spawn.tileIndex,
           (unsigned int)spawn.worldX,
           (unsigned int)spawn.worldY,
           (unsigned int)spawn.angle,
           (int)turn->viewStepX,
           (int)turn->viewStepY,
           (void*)doomRpg->render->shapeData,
           (void*)doomRpg->render->mediaTexels);
    return 1;
}

void __wrap_Esp32IntroDispose_reset(void) {
    static const EspNativeGameplaySessionConfig freshGame = {
        30U, /* health */
        30U, /* maxHealth */
        0U,  /* armor */
        20U, /* maxArmor */
        8U,  /* ammo */
        2U,  /* weapon */
        1U,  /* ammoType */
        1U   /* weaponsPresent */
    };

    __real_Esp32IntroDispose_reset();

    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();
    memset(&startupState, 0, sizeof(startupState));

    if (!EspNativeGameplaySession_configure(&freshGame)) {
        failStartup("gameplay session configuration");
        return;
    }

    printf("[NATIVEBOOT] RESET generic resident bootstrap armed\n");
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    int loadResult;

    __real_Esp32IntroDispose_service(doomRpgBase);

    if (startupState.stage == ESP_NATIVE_STARTUP_FAILED || doomRpg == NULL) {
        return;
    }

    if (startupState.stage == ESP_NATIVE_STARTUP_WAIT_INTRO) {
        if (!Esp32IntroDispose_isDone()) return;
        if (!startupBoundaryIsSafe(doomRpg)) {
            if (!startupState.waitLogged) {
                printf("[NATIVEBOOT] WAIT intro disposed but native startup boundary not settled yet\n");
                startupState.waitLogged = 1U;
            }
            return;
        }

        loadResult = loadStartupResident(doomRpg);
        if (loadResult <= 0) return;
        return; /* one bounded ownership stage per service pass */
    }

    if (startupState.stage == ESP_NATIVE_STARTUP_RESIDENT_READY) {
        if (!routeInitialSpawn(doomRpg)) return;
        return; /* first frame/cache/gameplay starts on the following pass */
    }

    if (startupState.stage == ESP_NATIVE_STARTUP_GAMEPLAY_READY) {
        EspNativeGameplaySession_service(doomRpgBase);
    }
}
