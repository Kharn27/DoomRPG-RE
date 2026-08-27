#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Game.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_catalog.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_player_facing_state.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"
#include "native_entrance_spawn_chain_probe.h"
#include "native_entrance_startup_route_probe.h"

#define ENTRANCE_RESOURCE "/intro.bsp"
#define ENTRANCE_MAP_ID 1U
#define ENTRANCE_SOURCE_BYTES 21823U
#define ENTRANCE_CRC32 0x623f34e4U
#define ENTRANCE_SPAWN_TILE 904U
#define ENTRANCE_SPAWN_X 544U
#define ENTRANCE_SPAWN_Y 1824U
#define ENTRANCE_SPAWN_ANGLE 64U
#define ENTRANCE_SNAPSHOT_FNV 0xb3811f3dU

typedef struct Esp32EntranceSpawnChainProbeState_s {
    uint8_t attempted;
    uint8_t ready;
} Esp32EntranceSpawnChainProbeState;

static Esp32EntranceSpawnChainProbeState probeState;

static uint32_t hashBytes(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t h = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
}

static int legacyMapRuntimeClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

void Esp32EntranceSpawnChainProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
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

int Esp32EntranceSpawnChainProbe_isReady(void) {
    return probeState.ready != 0U;
}

void Esp32EntranceSpawnChainProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspBspInventory inventory;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPlayerSpawnState spawn;
    const EspPlayerViewState* view;
    const EspHudRefreshState* hudRefresh;
    const EspPlayerFreshMapState* fresh;
    const EspPlayerInitialTileState* initialTile;
    const EspPlayerOrientationState* orientation;
    const EspPlayerFinishRotationTileState* secondTile;
    const EspPlayerFacingState* facing;
    const EspHudPostLoadClearState* hudClear;
    const EspNativeGameplayTurnState* turn;
    EspPlayerSpawnStatus spawnStatus;
    EspPlayerViewApplyStatus viewStatus;
    EspHudRefreshStatus hudStatus;
    EspPlayerFreshMapStatus freshStatus;
    EspPlayerInitialTileStatus initialStatus;
    EspPlayerOrientationStatus orientationStatus;
    EspPlayerFinishRotationTileStatus secondStatus;
    EspPlayerFacingStatus facingStatus;
    EspHudPostLoadClearStatus clearStatus;
    uint8_t deferredCode = 0U;
    uint8_t deferredOffset = 0U;
    uint32_t residentBeforeFNV;
    uint32_t residentAfterFNV;

    if (probeState.attempted || probeState.ready || doomRpg == NULL) return;
    if (!Esp32EntranceStartupRouteProbe_isDone()) return;
    probeState.attempted = 1U;

    printf("\n=== Doom RPG ESP32-native Entrance spawn chain ===\n");
    printf("[ENTRANCESPAWN] CONTRACT initial startup uses resident /intro.bsp header spawn, then recovered fresh-map order Hud.isUpdate -> Player_setup -> first tile -> finishRotation orientation -> second tile -> durable facing -> HUD clear. Fail closed on any unsupported eligible opcode; no renderer, input or legacy entity world yet.\n");

    memset(&inventory, 0, sizeof(inventory));
    memset(&residentBefore, 0, sizeof(residentBefore));
    memset(&residentAfter, 0, sizeof(residentAfter));
    memset(&spawn, 0, sizeof(spawn));

    if (doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyMapRuntimeClear(doomRpg->render) || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        hashBytes(&residentBefore, sizeof(residentBefore)) != ENTRANCE_SNAPSHOT_FNV) {
        printf("[ENTRANCESPAWN] FAILED unsafe source boundary resident=%08x legacyClear=%d entities=%d monsters=%d packOpen=%d\n",
               (unsigned int)hashBytes(&residentBefore, sizeof(residentBefore)),
               doomRpg->render != NULL ? legacyMapRuntimeClear(doomRpg->render) : 0,
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1,
               EspAssetPack_isOpen());
        return;
    }
    residentBeforeFNV = hashBytes(&residentBefore, sizeof(residentBefore));

    if (!EspBspReader_inventoryPackEntry(ENTRANCE_RESOURCE, &inventory) ||
        inventory.sourceBytes != ENTRANCE_SOURCE_BYTES ||
        inventory.crc32 != ENTRANCE_CRC32 ||
        inventory.spawnIndex != ENTRANCE_SPAWN_TILE ||
        inventory.spawnDirection != ENTRANCE_SPAWN_ANGLE ||
        inventory.loadMapId == 0U || inventory.loadMapId > 32U ||
        EspAssetPack_isOpen()) {
        printf("[ENTRANCESPAWN] FAILED inventory bytes=%u crc=%08x spawn=%u dir=%u loadId=%u packOpen=%d\n",
               (unsigned int)inventory.sourceBytes,
               (unsigned int)inventory.crc32,
               (unsigned int)inventory.spawnIndex,
               (unsigned int)inventory.spawnDirection,
               (unsigned int)inventory.loadMapId,
               EspAssetPack_isOpen());
        return;
    }

    spawnStatus = EspPlayerSpawn_prepareInitial(
        ENTRANCE_MAP_ID, &inventory, ESP_PLAYER_SPAWN_LOAD_FRESH_MAP, 0U, &spawn);
    if (spawnStatus != ESP_PLAYER_SPAWN_OK ||
        spawn.tileIndex != ENTRANCE_SPAWN_TILE ||
        spawn.worldX != ENTRANCE_SPAWN_X || spawn.worldY != ENTRANCE_SPAWN_Y ||
        spawn.angle != ENTRANCE_SPAWN_ANGLE ||
        spawn.spawnSource != ESP_PLAYER_SPAWN_SOURCE_HEADER ||
        spawn.sourceSpawnParam != 0U || spawn.overrideUsed != 0U) {
        printf("[ENTRANCESPAWN] FAILED spawn status=%u tile=%u pos=%u,%u angle=%u source=%u param=%u\n",
               (unsigned int)spawnStatus, (unsigned int)spawn.tileIndex,
               (unsigned int)spawn.worldX, (unsigned int)spawn.worldY,
               (unsigned int)spawn.angle, (unsigned int)spawn.spawnSource,
               (unsigned int)spawn.sourceSpawnParam);
        return;
    }

    viewStatus = EspPlayerView_applySpawn(&spawn);
    hudStatus = viewStatus == ESP_PLAYER_VIEW_APPLY_OK
                    ? EspHudRefresh_routePostSpawn()
                    : ESP_HUD_REFRESH_VIEW_INVALID;
    freshStatus = hudStatus == ESP_HUD_REFRESH_OK
                      ? EspPlayerFreshMap_route(DoomRPG_GetUpTimeMS(), 0U)
                      : ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    if (viewStatus != ESP_PLAYER_VIEW_APPLY_OK ||
        hudStatus != ESP_HUD_REFRESH_OK || freshStatus != ESP_PLAYER_FRESH_MAP_OK) {
        printf("[ENTRANCESPAWN] FAILED setup view=%u hud=%u fresh=%u\n",
               (unsigned int)viewStatus, (unsigned int)hudStatus,
               (unsigned int)freshStatus);
        return;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    initialStatus = EspPlayerInitialTile_route(0U, 0U, &deferredCode, &deferredOffset);
    if (initialStatus != ESP_PLAYER_INITIAL_TILE_OK) {
        printf("[ENTRANCESPAWN] BLOCKED stage=FIRST_TILE status=%u deferredOpcode=%u commandOffset=%u tile=%u pos=%u,%u angle=%u\n",
               (unsigned int)initialStatus, (unsigned int)deferredCode,
               (unsigned int)deferredOffset, (unsigned int)spawn.tileIndex,
               (unsigned int)spawn.worldX, (unsigned int)spawn.worldY,
               (unsigned int)spawn.angle);
        return;
    }

    orientationStatus = EspPlayerOrientation_route();
    if (orientationStatus != ESP_PLAYER_ORIENTATION_OK) {
        printf("[ENTRANCESPAWN] FAILED stage=ORIENTATION status=%u\n",
               (unsigned int)orientationStatus);
        return;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    secondStatus = EspPlayerFinishRotationTile_route(
        0U, 0U, &deferredCode, &deferredOffset);
    if (secondStatus != ESP_PLAYER_FINISH_ROTATION_TILE_OK) {
        printf("[ENTRANCESPAWN] BLOCKED stage=SECOND_TILE status=%u deferredOpcode=%u commandOffset=%u tile=%u\n",
               (unsigned int)secondStatus, (unsigned int)deferredCode,
               (unsigned int)deferredOffset, (unsigned int)spawn.tileIndex);
        return;
    }

    facingStatus = EspPlayerFacing_route();
    clearStatus = facingStatus == ESP_PLAYER_FACING_OK
                      ? EspHudPostLoadClear_route()
                      : ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;
    if (facingStatus != ESP_PLAYER_FACING_OK ||
        clearStatus != ESP_HUD_POST_LOAD_CLEAR_OK) {
        printf("[ENTRANCESPAWN] FAILED stage=FACING facing=%u hudClear=%u packOpen=%d\n",
               (unsigned int)facingStatus, (unsigned int)clearStatus,
               EspAssetPack_isOpen());
        return;
    }

    if (!EspNativeGameplayDispatch_adoptView()) {
        printf("[ENTRANCESPAWN] FAILED gameplay dispatch adopt\n");
        return;
    }

    view = EspPlayerView_view();
    hudRefresh = EspHudRefresh_view();
    fresh = EspPlayerFreshMap_view();
    initialTile = EspPlayerInitialTile_view();
    orientation = EspPlayerOrientation_view();
    secondTile = EspPlayerFinishRotationTile_view();
    facing = EspPlayerFacing_view();
    hudClear = EspHudPostLoadClear_view();
    turn = EspNativeGameplayDispatch_view();

    if (view == NULL || hudRefresh == NULL || fresh == NULL ||
        initialTile == NULL || orientation == NULL || secondTile == NULL ||
        facing == NULL || hudClear == NULL || turn == NULL ||
        view->targetMapId != ENTRANCE_MAP_ID ||
        view->viewX != (int32_t)ENTRANCE_SPAWN_X ||
        view->viewY != (int32_t)ENTRANCE_SPAWN_Y ||
        view->viewAngle != (int32_t)ENTRANCE_SPAWN_ANGLE ||
        view->hudRefreshPending != 0U || view->facingRefreshPending != 0U ||
        view->playerSetupPending != 0U || view->tileEnterPending != 0U ||
        turn->viewStepX != 0 || turn->viewStepY != -64 ||
        EspAssetPack_isOpen() || !legacyMapRuntimeClear(doomRpg->render) ||
        !EspMapResidentLifecycle_capture(&residentAfter)) {
        printf("[ENTRANCESPAWN] FAILED final owner boundary view=%p turn=%p packOpen=%d\n",
               (const void*)view, (const void*)turn, EspAssetPack_isOpen());
        return;
    }

    residentAfterFNV = hashBytes(&residentAfter, sizeof(residentAfter));

    printf("[ENTRANCESPAWN] SPAWN tile=%u tileXY=%u,%u pos=%u,%u z=%u oldZ=%u angle=%u targetMap=%u gameplayLoadMapId=%u source=HEADER\n",
           (unsigned int)spawn.tileIndex, (unsigned int)spawn.tileX,
           (unsigned int)spawn.tileY, (unsigned int)spawn.worldX,
           (unsigned int)spawn.worldY, (unsigned int)spawn.viewZ,
           (unsigned int)spawn.viewZOld, (unsigned int)spawn.angle,
           (unsigned int)spawn.targetMapId,
           (unsigned int)spawn.gameplayLoadMapId);
    printf("[ENTRANCESPAWN] TILE firstEvent=%u/%u eligible=%u executed=%u removed=%u secondEvent=%u/%u eligible=%u executed=%u removed=%u\n",
           (unsigned int)initialTile->eventFound,
           (unsigned int)initialTile->eventIndex,
           (unsigned int)initialTile->eligibleCommands,
           (unsigned int)initialTile->executedCommands,
           (unsigned int)initialTile->removedCommands,
           (unsigned int)secondTile->eventFound,
           (unsigned int)secondTile->eventIndex,
           (unsigned int)secondTile->eligibleCommands,
           (unsigned int)secondTile->executedCommands,
           (unsigned int)secondTile->removedCommands);
    printf("[ENTRANCESPAWN] FACING kind=%u hitIndex=%u hitTile=%u type=%u subtype=%u trace=%u step=%d,%d hudClear=yes pending=0/0/0/0\n",
           (unsigned int)facing->kind, (unsigned int)facing->hitIndex,
           (unsigned int)facing->hitTile, (unsigned int)facing->entityType,
           (unsigned int)facing->entitySubType,
           (unsigned int)facing->traceEntityCount,
           (int)turn->viewStepX, (int)turn->viewStepY);
    printf("[ENTRANCESPAWN] RESIDENT before=%08x after=%08x scriptBefore=%08x scriptAfter=%08x changed=%s packClosed=yes shapeData=%p mediaTexels=%p legacyEntities=%d legacyMonsters=%d\n",
           (unsigned int)residentBeforeFNV, (unsigned int)residentAfterFNV,
           (unsigned int)residentBefore.scriptStateFNV1a,
           (unsigned int)residentAfter.scriptStateFNV1a,
           residentBeforeFNV == residentAfterFNV ? "no" : "yes",
           (void*)doomRpg->render->shapeData,
           (void*)doomRpg->render->mediaTexels,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);
    printf("[ENTRANCESPAWN] READY settled=yes gameplayDispatch=yes next=Entrance-world+HUD+touch\n");

    probeState.ready = 1U;
}
