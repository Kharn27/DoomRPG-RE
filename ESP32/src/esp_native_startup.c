#include <SDL.h>
#include "esp_native_root_state.h"
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
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_startup.h"
#include "platform_touch_events.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_intro_state.h"
#include "esp_native_transition_presentation.h"
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

/* One small, explicitly owned death recovery transaction. Unlike a manual
 * LOAD this intentionally resets sector world state from immutable PAK data,
 * then restores the captured player state (sector-entry for RETRY). */
typedef struct EspNativeDeathRouteScratch_s {
    EspBspInventory inventory;
    EspMapResidentSnapshot resident;
    EspPlayerSpawnState spawnPreflight;
    EspNativeGameplaySessionConfig config;
    EspNativeGameplayPlayerState player;
    uint8_t targetMapId;
    uint8_t queued;
} EspNativeDeathRouteScratch;
static EspNativeDeathRouteScratch deathRoute;

static int introResourcesAreReleased(const DoomRPG_t* doomRpg) {
    return doomRpg != NULL && EspNativeIntroState_view(doomRpg) == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->tileEvents == NULL &&
           render->mapByteCode == NULL &&
           render->mapStringsIDs == NULL &&
#endif
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           /* Desktop BSP I/O buffer/cursor are absent from native Render_t. */
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int startupBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || EspNativeRootOwners_live()->doomCanvas == NULL ||
        EspNativeRootOwners_live()->render == NULL ||
        EspNativeRootOwners_live()->menuSystem == NULL) {
        return 0;
    }

    canvas = EspNativeRootOwners_live()->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           EspNativeRootOwners_live()->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->startupMap >= MAP_INTRO &&
           canvas->startupMap <= MAP_END_GAME &&
           introResourcesAreReleased(doomRpg) &&
           legacyRuntimeIsClear(EspNativeRootOwners_live()->render) &&
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
    if (EspNativeTransitionPresentation_isLoadingActive()) {
        EspNativeTransitionPresentation_abortLoading(
            reason != NULL ? reason : "native-startup-failed");
    }
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

    targetMapId = (uint8_t)EspNativeRootOwners_live()->doomCanvas->startupMap;
    resourceName = EspMapCatalog_nameForId(targetMapId);
    if (resourceName == NULL ||
        (int)targetMapId != EspNativeRootOwners_live()->doomCanvas->startupMap ||
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

    if (EspNativeTransitionPresentation_isLoadingActive()) {
        EspNativeTransitionPresentation_checkpointProgress(30U, "BSP");
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
        !legacyRuntimeIsClear(EspNativeRootOwners_live()->render)) {
        failStartup("generic resident load");
        return -1;
    }

    if (EspNativeTransitionPresentation_isLoadingActive()) {
        EspNativeTransitionPresentation_checkpointProgress(60U, "RUNTIME");
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

    if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL ||
        startupState.stage != ESP_NATIVE_STARTUP_RESIDENT_READY ||
        !EspMapCatalog_isValidId(startupState.targetMapId) ||
        !EspMapResidentLifecycle_capture(&resident) ||
        !legacyRuntimeIsClear(EspNativeRootOwners_live()->render) ||
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
                      ? EspPlayerFreshMap_route(
                          DoomRPG_GetUpTimeMS(),
                          EspNativeGameplayPlayerState_view()->disabledWeapons)
                      : ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    if (viewStatus != ESP_PLAYER_VIEW_APPLY_OK ||
        hudStatus != ESP_HUD_REFRESH_OK ||
        freshStatus != ESP_PLAYER_FRESH_MAP_OK) {
        failStartup("post-spawn setup");
        return 0;
    }

    firstTileStatus = EspPlayerInitialTile_route(
        EspNativeGameplayPlayerState_view()->keys, 0U,
        &deferredCode, &deferredOffset);
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
        EspNativeGameplayPlayerState_view()->keys, 0U,
        &deferredCode, &deferredOffset);
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
        !legacyRuntimeIsClear(EspNativeRootOwners_live()->render)) {
        failStartup("settled player view");
        return 0;
    }

    if (EspNativeTransitionPresentation_isLoadingActive()) {
        EspNativeTransitionPresentation_checkpointProgress(85U, "RESTORE");
        printf("[NATIVEBOOT] LOADING-PRIME map=%u progress=85 owner=retained-until-session-active primeProgress=session-stages\n",
               (unsigned int)startupState.targetMapId);
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
           (void*)EspNativeRootOwners_live()->render->shapeData,
           (void*)EspNativeRootOwners_live()->render->mediaTexels);
    return 1;
}

/*
 * Narrow explicit composition surface consumed by native_intro_clock.c.
 * Keep resident/bootstrap ownership here rather than hiding it behind linker
 * interception of the resource-only intro disposer.
 */
void EspNativeStartup_reset(void) {
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

    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();
    memset(&startupState, 0, sizeof(startupState));
    memset(&deathRoute, 0, sizeof(deathRoute));

    if (!EspNativeGameplaySession_configure(&freshGame)) {
        failStartup("gameplay session configuration");
        return;
    }

    printf("[NATIVEBOOT] RESET generic resident bootstrap armed\n");
}

/* RETRY/JUNCTION cannot reuse legacy DoomCanvas_loadState or an
 * unrelated manual save. Fail before teardown if target/player are invalid.
 * The caller is the native death owner and queues only after a real tap. */
int EspNativeStartup_queueDeathRoute(
    uint8_t targetMapId, const EspNativeGameplayPlayerState* player) {
    if (deathRoute.queued || player == NULL || player->active != 1U ||
        (player->param1 & 0xffU) == 0U ||
        !EspMapCatalog_isValidId(targetMapId) ||
        !EspNativeGameplayPlayerDeath_isMenuReady() ||
        !EspNativeGameplaySession_isActive() ||
        !EspMapResidentLifecycle_isReady() || EspAssetPack_isOpen())
        return 0;

    deathRoute.player = *player;
    deathRoute.targetMapId = targetMapId;
    deathRoute.queued = 1U;
    return 1;
}

/* Config matches the existing native CHANGEMAP handoff: no desktop Player,
 * no world-wide entity allocations, no reassignment of saved slot paths. */
static int deathSessionConfig(void) {
    const EspNativeGameplayPlayerState* p = &deathRoute.player;
    const EspNativeGameplayWeaponSpec* w = NULL;
    uint8_t ammoType;

    /* Fresh-map setup explicitly rejects disabled weapons. Never tear down
     * the dying sector when this source-player contract cannot be re-spawned. */
    if (p->disabledWeapons != 0U) return 0;
    if (p->weapon >= ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT ||
        p->weapons == 0U ||
        (p->weapons & (uint16_t)(1U << p->weapon)) == 0U)
        return 0;
    if (p->weapon < ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS) {
        w = EspNativeGameplayCombatMath_weapon(p->weapon);
        if (w == NULL ||
            w->ammoType >= ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES)
            return 0;
        ammoType = w->ammoType;
    } else {
        ammoType = 5U; /* existing CHANGEMAP familiar ammo type */
    }
    memset(&deathRoute.config, 0, sizeof(deathRoute.config));
    deathRoute.config.health = (uint8_t)(p->param1 & 0xffU);
    deathRoute.config.maxHealth = (uint8_t)((p->param1 >> 8) & 0xffU);
    deathRoute.config.armor = (uint8_t)((p->param1 >> 16) & 0xffU);
    deathRoute.config.maxArmor = (uint8_t)((p->param1 >> 24) & 0xffU);
    deathRoute.config.ammo = p->ammo[ammoType];
    deathRoute.config.weapon = p->weapon;
    deathRoute.config.ammoType = ammoType;
    deathRoute.config.weaponsPresent = 1U;
    return deathRoute.config.maxHealth > 0U &&
           deathRoute.config.health <= deathRoute.config.maxHealth &&
           deathRoute.config.armor <= deathRoute.config.maxArmor;
}

/* Invoked once the composed gameplay service has returned to loop().
 * Never destroy a source world on the tap callback's call stack. */
void EspNativeStartup_serviceDeathRoute(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const char* name;
    EspMapResidentLifecycleStatus loadStatus;
    EspPlayerSpawnStatus spawnStatus;
    uint8_t target;

    if (!deathRoute.queued) return;
    target = deathRoute.targetMapId;
    name = EspMapCatalog_nameForId(target);
    if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL ||
        !EspNativeGameplaySession_isActive() ||
        !EspNativeGameplayPlayerDeath_isMenuReady() ||
        !EspMapResidentLifecycle_isReady() || EspAssetPack_isOpen() ||
        name == NULL || !deathSessionConfig()) {
        deathRoute.queued = 0U;
        EspNativeGameplayPlayerDeath_cancelQueuedRoute();
        printf("[DEATHROUTE] REFUSED stage=source-preflight target=%u mutation=no\n",
               (unsigned int)target);
        return;
    }

    memset(&deathRoute.inventory, 0, sizeof(deathRoute.inventory));
    if (!EspAssetPack_sourceProbeBegin(name)) {
        deathRoute.queued = 0U;
        EspNativeGameplayPlayerDeath_cancelQueuedRoute();
        printf("[DEATHROUTE] REFUSED stage=source-probe target=%u mutation=no\n",
               (unsigned int)target);
        return;
    }
    {
        const int valid = EspBspReader_inventoryPackEntry(
            name, &deathRoute.inventory);
        EspAssetPack_sourceProbeEnd();
        if (!valid || deathRoute.inventory.sourceBytes == 0U ||
            deathRoute.inventory.consumedBytes != deathRoute.inventory.sourceBytes ||
            deathRoute.inventory.trailingBytes != 0U ||
            deathRoute.inventory.crc32 == 0U ||
            deathRoute.inventory.crc32 != deathRoute.inventory.expectedCrc32 ||
            deathRoute.inventory.plan.persistentBytes == 0U ||
            EspAssetPack_isOpen() || EspAssetPack_isSourceProbeActive()) {
            deathRoute.queued = 0U;
            EspNativeGameplayPlayerDeath_cancelQueuedRoute();
            printf("[DEATHROUTE] REFUSED stage=target-inventory target=%u mutation=no\n",
                   (unsigned int)target);
            return;
        }
    }
    memset(&deathRoute.spawnPreflight, 0, sizeof(deathRoute.spawnPreflight));
    spawnStatus = EspPlayerSpawn_prepareInitial(
        target, &deathRoute.inventory, ESP_PLAYER_SPAWN_LOAD_FRESH_MAP,
        0U, &deathRoute.spawnPreflight);
    if (spawnStatus != ESP_PLAYER_SPAWN_OK) {
        deathRoute.queued = 0U;
        EspNativeGameplayPlayerDeath_cancelQueuedRoute();
        printf("[DEATHROUTE] REFUSED stage=target-spawn target=%u status=%u mutation=no\n",
               (unsigned int)target, (unsigned int)spawnStatus);
        return;
    }
    if (!EspNativeTransitionPresentation_beginLoading(target)) {
        deathRoute.queued = 0U;
        EspNativeGameplayPlayerDeath_cancelQueuedRoute();
        printf("[DEATHROUTE] REFUSED stage=loading target=%u mutation=no\n",
               (unsigned int)target);
        return;
    }

    printf("[DEATHROUTE] BEGIN target=%u source=death-menu player=compact-snapshot world=fresh-pack state=preflight-ok\n",
           (unsigned int)target);
    PlatformInput_setTapCallback(NULL);
    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();
    EspAssetPack_mapFlashSetProgressCallback(
        EspNativeTransitionPresentation_progress);
    const int prepared = EspAssetPack_mapFlashPrepare(target);
    EspAssetPack_mapFlashSetProgressCallback(NULL);
    if (!prepared || !EspAssetPack_isMapFlashActive() ||
        EspAssetPack_isOpen()) {
        failStartup("death target map-flash prepare");
        deathRoute.queued = 0U;
        return;
    }
    EspNativeTransitionPresentation_checkpointProgress(30U, "BSP");
    memset(&deathRoute.resident, 0, sizeof(deathRoute.resident));
    loadStatus = EspMapResidentLifecycle_loadFromEmpty(
        name, &deathRoute.inventory, &deathRoute.resident);
    if (loadStatus != ESP_MAP_RESIDENT_OK ||
        !EspMapResidentLifecycle_isReady() ||
        deathRoute.resident.runtimeArenaBytes !=
            deathRoute.inventory.plan.persistentBytes ||
        EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(EspNativeRootOwners_live()->render)) {
        failStartup("death target resident load");
        deathRoute.queued = 0U;
        return;
    }
    EspNativeTransitionPresentation_checkpointProgress(60U, "RUNTIME");
    if (!EspNativeGameplayPlayerState_restore(&deathRoute.player)) {
        failStartup("death player restore");
        deathRoute.queued = 0U;
        return;
    }
    startupState.targetMapId = target;
    startupState.stage = ESP_NATIVE_STARTUP_RESIDENT_READY;
    if (!routeInitialSpawn(doomRpg) ||
        !EspNativeGameplaySession_configure(&deathRoute.config)) {
        failStartup("death spawn/session");
        deathRoute.queued = 0U;
        return;
    }
    deathRoute.queued = 0U;
    printf("[DEATHROUTE] SESSION target=%u playerHP=%u/%u runtime=fresh session=reprime-pending saveWrite=no\n",
           (unsigned int)target, (unsigned int)deathRoute.config.health,
           (unsigned int)deathRoute.config.maxHealth);
}

void EspNativeStartup_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    int loadResult;

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

        {
            uint8_t targetMapId =
                (uint8_t)EspNativeRootOwners_live()->doomCanvas->startupMap;
            const char* resourceName =
                EspMapCatalog_nameForId(targetMapId);
            if (resourceName == NULL ||
                (int)targetMapId != EspNativeRootOwners_live()->doomCanvas->startupMap ||
                !EspNativeTransitionPresentation_beginLoading(targetMapId)) {
                failStartup("intro loading presentation");
                return;
            }
            printf("[NATIVEBOOT] LOADING-TAKEOVER map=%u source=intro-disposed owner=transition-presentation\n",
                   (unsigned int)targetMapId);
        }

        loadResult = loadStartupResident(doomRpg);
        if (loadResult <= 0) return;
        return; /* one bounded ownership stage per service pass */
    }

    if (startupState.stage == ESP_NATIVE_STARTUP_RESIDENT_READY) {
        if (!routeInitialSpawn(doomRpg)) return;
        return; /* first frame/cache/gameplay starts on the following pass */
    }

    /* The Arduino loop owns generic session service for both fresh starts and
     * direct MENU_MAIN checkpoint resumes. */
}
