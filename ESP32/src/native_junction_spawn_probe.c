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
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_map_committed_transition.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_player_spawn_state.h"
#include "native_committed_transition_probe.h"
#include "native_junction_spawn_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define TARGET_RESOURCE "/junction.bsp"
#define EXPECTED_STATE_BYTES 24U
#define EXPECTED_STATE_FNV 0xba6af4a7U
#define EXPECTED_OVERRIDE_FNV 0xe0a5110bU
#define EXPECTED_TRANSITION_FNV 0x2c595a62U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_OVERRIDE_PARAM 0x00030167UL

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

static EspPlayerSpawnState parkedSpawnState;

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

static uint32_t placementWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[16];
    const DoomCanvas_t* canvas;
    const Game_t* game;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->hud == NULL) return 0U;

    canvas = doomRpg->doomCanvas;
    game = doomRpg->game;
    render = doomRpg->render;
    values[0] = (uint32_t)game->spawnParam;
    values[1] = (uint32_t)game->isLoaded;
    values[2] = (uint32_t)canvas->viewX;
    values[3] = (uint32_t)canvas->viewY;
    values[4] = (uint32_t)canvas->viewZ;
    values[5] = (uint32_t)canvas->viewAngle;
    values[6] = (uint32_t)canvas->destX;
    values[7] = (uint32_t)canvas->destY;
    values[8] = (uint32_t)canvas->destAngle;
    values[9] = (uint32_t)(uint16_t)canvas->loadMapID;
    values[10] = (uint32_t)canvas->loadType;
    values[11] = (uint32_t)canvas->state;
    values[12] = (uint32_t)canvas->storyPage;
    values[13] = (uint32_t)render->viewZOld;
    values[14] = (uint32_t)doomRpg->hud->isUpdate;
    values[15] = (uint32_t)game->activeLoadType;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (player == NULL) return 0U;
    hash ^= (uint32_t)player->weapon;
    hash *= 16777619U;
    hash ^= (uint32_t)player->weapons;
    hash *= 16777619U;
    hash ^= (uint32_t)player->totalTime;
    hash *= 16777619U;
    hash ^= (uint32_t)player->totalMoves;
    hash *= 16777619U;
    for (i = 0U; i < (uint32_t)(sizeof(player->ammo) / sizeof(player->ammo[0])); ++i) {
        hash ^= (uint32_t)player->ammo[i];
        hash *= 16777619U;
    }
    return hash;
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

static int stateIsZero(const EspPlayerSpawnState* state) {
    EspPlayerSpawnState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

static void buildCommittedTransition(EspMapCommittedTransitionState* state) {
    memset(state, 0, sizeof(*state));
    state->targetSourceBytes = 21051U;
    state->targetSourceCrc32 = 0x4a2c5800U;
    state->targetSourceFNV1a = 0xfefaf5caU;
    state->spawnParam = 0U;
    state->sourceMapId = ESP_MAP_ID_INTRO;
    state->targetMapId = ESP_MAP_ID_JUNCTION;
    state->targetGameplayLoadMapId = 2U;
    state->menuKind = ESP_STATS_MENU_KIND_LEVEL;
    state->phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED;
    state->pendingConsumed = 1U;
    state->statsAcknowledged = 1U;
    state->committed = 1U;
}

static int targetSnapshotCanonical(const EspMapResidentSnapshot* snapshot) {
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

static int realSpawnCanonical(const EspPlayerSpawnState* state) {
    return state != NULL && sizeof(*state) == EXPECTED_STATE_BYTES &&
           state->sourceSpawnParam == 0U && state->tileIndex == 943U &&
           state->worldX == 992U && state->worldY == 1888U &&
           state->tileX == 15U && state->tileY == 29U &&
           state->angle == 64U && state->viewZ == 36U &&
           state->viewZOld == 4U &&
           state->spawnSource == ESP_PLAYER_SPAWN_SOURCE_HEADER &&
           state->loadType == ESP_PLAYER_SPAWN_LOAD_FRESH_MAP &&
           state->overrideUsed == 0U &&
           state->facingRefreshPending == 1U &&
           state->playerSetupPending == 1U &&
           state->tileEnterPending == 1U && state->active == 1U &&
           state->targetMapId == ESP_MAP_ID_JUNCTION &&
           state->gameplayLoadMapId == 2U &&
           hashBytes(state, sizeof(*state)) == EXPECTED_STATE_FNV;
}

static int overrideSpawnCanonical(const EspPlayerSpawnState* state) {
    return state != NULL && state->sourceSpawnParam == EXPECTED_OVERRIDE_PARAM &&
           state->tileIndex == 359U && state->worldX == 480U &&
           state->worldY == 736U && state->tileX == 7U && state->tileY == 11U &&
           state->angle == 192U && state->viewZ == 36U && state->viewZOld == 4U &&
           state->spawnSource == ESP_PLAYER_SPAWN_SOURCE_OVERRIDE &&
           state->loadType == ESP_PLAYER_SPAWN_LOAD_FRESH_MAP &&
           state->overrideUsed == 1U &&
           state->facingRefreshPending == 1U &&
           state->playerSetupPending == 1U &&
           state->tileEnterPending == 1U && state->active == 1U &&
           state->targetMapId == ESP_MAP_ID_JUNCTION &&
           state->gameplayLoadMapId == 2U &&
           hashBytes(state, sizeof(*state)) == EXPECTED_OVERRIDE_FNV;
}

void Esp32JunctionSpawnProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    memset(&parkedSpawnState, 0, sizeof(parkedSpawnState));
}

int Esp32JunctionSpawnProbe_isDone(void) {
    return probeState.done;
}

int Esp32JunctionSpawnProbe_getState(EspPlayerSpawnState* outState) {
    if (outState == NULL || !probeState.done || parkedSpawnState.active != 1U) {
        return 0;
    }
    *outState = parkedSpawnState;
    return 1;
}

void Esp32JunctionSpawnProbe_service(struct DoomRPG_s* doomRpg) {
    EspBspInventory inventory;
    EspBspInventory badInventory;
    EspBspInventory runtimeMismatchInventory;
    EspBspInventory badSpawnInventory;
    EspMapCommittedTransitionState transition;
    EspMapCommittedTransitionState overrideTransition;
    EspMapCommittedTransitionState notCommittedTransition;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPlayerSpawnState spawn;
    EspPlayerSpawnState overrideSpawn;
    EspPlayerSpawnState scratch;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t placementBefore;
    uint32_t placementAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t spawnFNV;
    uint32_t overrideFNV;
    int invalidNullTransition;
    int invalidNullInventory;
    int invalidNullOutput;
    int notCommitted;
    int loadTypeGate;
    int loadedGate;
    int targetMismatch;
    int runtimeMismatch;
    int spawnInvalid;
    int resetProof;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32CommittedTransitionProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONSPAWNPROBE] ARMED committed Junction residency proven; native spawn/load projection starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction player spawn projection ===\n");
    printf("[JUNCTIONSPAWNPROBE] CONTRACT project committed fresh-map Game_spawnPlayer placement into one 24B pointer-free native state: spawnParam zero uses BSP header, nonzero override uses packed x/y/angle; retain facing/Player_setup/tile-enter as pending followups; loadType=0 only; no legacy Game/Player/Render/DoomCanvas mutation, no ST_PLAYING, no persistent allocation\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP ||
        doomRpg->game->isLoaded != 0 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || !legacyRuntimeIsClear(doomRpg->render) ||
        EspAssetPack_isOpen() || sizeof(EspPlayerSpawnState) != EXPECTED_STATE_BYTES ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !targetSnapshotCanonical(&residentBefore)) {
        printf("[JUNCTIONSPAWNPROBE] FAILED unsafe committed Junction boundary\n");
        probeState.done = 1;
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    placementBefore = placementWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);

    if (!EspBspReader_inventoryPackEntry(TARGET_RESOURCE, &inventory) ||
        EspAssetPack_isOpen() || inventory.sourceBytes != 21051U ||
        inventory.crc32 != 0x4a2c5800U || inventory.fnv1a32 != 0xfefaf5caU ||
        inventory.loadMapId != 2U || inventory.spawnIndex != 943U ||
        inventory.spawnDirection != 64U) {
        printf("[JUNCTIONSPAWNPROBE] FAILED Junction inventory\n");
        probeState.done = 1;
        return;
    }

    buildCommittedTransition(&transition);
    if (hashBytes(&transition, sizeof(transition)) != EXPECTED_TRANSITION_FNV ||
        EspPlayerSpawn_prepareCommitted(
            &transition, &inventory, (uint8_t)doomRpg->doomCanvas->loadType,
            (uint8_t)doomRpg->game->isLoaded, &spawn) != ESP_PLAYER_SPAWN_OK ||
        !realSpawnCanonical(&spawn)) {
        printf("[JUNCTIONSPAWNPROBE] FAILED real Junction spawn projection\n");
        probeState.done = 1;
        return;
    }
    spawnFNV = hashBytes(&spawn, sizeof(spawn));

    overrideTransition = transition;
    overrideTransition.spawnParam = EXPECTED_OVERRIDE_PARAM;
    if (EspPlayerSpawn_prepareCommitted(
            &overrideTransition, &inventory, ESP_PLAYER_SPAWN_LOAD_FRESH_MAP, 0U,
            &overrideSpawn) != ESP_PLAYER_SPAWN_OK ||
        !overrideSpawnCanonical(&overrideSpawn)) {
        printf("[JUNCTIONSPAWNPROBE] FAILED override decode\n");
        probeState.done = 1;
        return;
    }
    overrideFNV = hashBytes(&overrideSpawn, sizeof(overrideSpawn));

    scratch = spawn;
    EspPlayerSpawn_reset(&scratch);
    resetProof = stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidNullTransition =
        EspPlayerSpawn_prepareCommitted(NULL, &inventory, 0U, 0U, &scratch) ==
            ESP_PLAYER_SPAWN_INVALID &&
        stateIsZero(&scratch);
    memset(&scratch, 0xa5, sizeof(scratch));
    invalidNullInventory =
        EspPlayerSpawn_prepareCommitted(&transition, NULL, 0U, 0U, &scratch) ==
            ESP_PLAYER_SPAWN_INVALID &&
        stateIsZero(&scratch);
    invalidNullOutput =
        EspPlayerSpawn_prepareCommitted(&transition, &inventory, 0U, 0U, NULL) ==
        ESP_PLAYER_SPAWN_INVALID;

    notCommittedTransition = transition;
    notCommittedTransition.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_READY;
    notCommittedTransition.committed = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    notCommitted =
        EspPlayerSpawn_prepareCommitted(&notCommittedTransition, &inventory, 0U,
                                        0U, &scratch) ==
            ESP_PLAYER_SPAWN_NOT_COMMITTED &&
        stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    loadTypeGate =
        EspPlayerSpawn_prepareCommitted(&transition, &inventory, 1U, 0U,
                                        &scratch) ==
            ESP_PLAYER_SPAWN_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);
    memset(&scratch, 0xa5, sizeof(scratch));
    loadedGate =
        EspPlayerSpawn_prepareCommitted(&transition, &inventory, 0U, 1U,
                                        &scratch) ==
            ESP_PLAYER_SPAWN_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    badInventory = inventory;
    badInventory.crc32 ^= 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    targetMismatch =
        EspPlayerSpawn_prepareCommitted(&transition, &badInventory, 0U, 0U,
                                        &scratch) ==
            ESP_PLAYER_SPAWN_TARGET_MISMATCH &&
        stateIsZero(&scratch);

    runtimeMismatchInventory = inventory;
    runtimeMismatchInventory.nodes += 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    runtimeMismatch =
        EspPlayerSpawn_prepareCommitted(&transition, &runtimeMismatchInventory,
                                        0U, 0U, &scratch) ==
            ESP_PLAYER_SPAWN_RUNTIME_MISMATCH &&
        stateIsZero(&scratch);

    badSpawnInventory = inventory;
    badSpawnInventory.spawnIndex = ESP_PLAYER_SPAWN_TILE_COUNT;
    memset(&scratch, 0xa5, sizeof(scratch));
    spawnInvalid =
        EspPlayerSpawn_prepareCommitted(&transition, &badSpawnInventory, 0U, 0U,
                                        &scratch) ==
            ESP_PLAYER_SPAWN_SPAWN_INVALID &&
        stateIsZero(&scratch);

    if (!invalidNullTransition || !invalidNullInventory || !invalidNullOutput ||
        !notCommitted || !loadTypeGate || !loadedGate || !targetMismatch ||
        !runtimeMismatch || !spawnInvalid || !resetProof ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        !targetSnapshotCanonical(&residentAfter) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONSPAWNPROBE] FAILED fail-closed/resident preservation\n");
        probeState.done = 1;
        return;
    }

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    placementAfter = placementWitness(doomRpg);
    playerAfter = playerWitness(doomRpg->player);

    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || placementAfter != placementBefore ||
        playerAfter != playerBefore || doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || !legacyRuntimeIsClear(doomRpg->render)) {
        printf("[JUNCTIONSPAWNPROBE] FAILED mutation/RAM boundary\n");
        probeState.done = 1;
        return;
    }

    parkedSpawnState = spawn;
    probeState.done = 1;

    printf("[JUNCTIONSPAWN] READY stateBytes=%u stateFNV=%08lx targetMap=%u gameplayLoadMapId=%u spawnParam=%08lx source=HEADER tileIndex=%u tile=%u/%u world=%u/%u angle=%u viewZ=%u viewZOld=%u loadType=%u active=%u\n",
           (unsigned int)sizeof(spawn), (unsigned long)spawnFNV,
           (unsigned int)spawn.targetMapId,
           (unsigned int)spawn.gameplayLoadMapId,
           (unsigned long)spawn.sourceSpawnParam,
           (unsigned int)spawn.tileIndex, (unsigned int)spawn.tileX,
           (unsigned int)spawn.tileY, (unsigned int)spawn.worldX,
           (unsigned int)spawn.worldY, (unsigned int)spawn.angle,
           (unsigned int)spawn.viewZ, (unsigned int)spawn.viewZOld,
           (unsigned int)spawn.loadType, (unsigned int)spawn.active);
    printf("[JUNCTIONSPAWN] FOLLOWUPS facingRefresh=%u playerSetup=%u tileEnter=%u spawnApplied=no facingApplied=no playerSetupApplied=no tileEnterApplied=no\n",
           (unsigned int)spawn.facingRefreshPending,
           (unsigned int)spawn.playerSetupPending,
           (unsigned int)spawn.tileEnterPending);
    printf("[JUNCTIONSPAWN] OVERRIDE param=%08lx tileIndex=%u tile=%u/%u world=%u/%u angle=%u source=OVERRIDE overrideUsed=%u stateFNV=%08lx headerIgnored=yes\n",
           (unsigned long)overrideSpawn.sourceSpawnParam,
           (unsigned int)overrideSpawn.tileIndex,
           (unsigned int)overrideSpawn.tileX,
           (unsigned int)overrideSpawn.tileY,
           (unsigned int)overrideSpawn.worldX,
           (unsigned int)overrideSpawn.worldY,
           (unsigned int)overrideSpawn.angle,
           (unsigned int)overrideSpawn.overrideUsed,
           (unsigned long)overrideFNV);
    printf("[JUNCTIONSPAWN] LOADSEMANTIC loadType=%d gameIsLoaded=%d normalMapLoad=yes savedGameLoad=no activeLoadType=%d loadTypeMutation=no\n",
           doomRpg->doomCanvas->loadType, doomRpg->game->isLoaded,
           doomRpg->game->activeLoadType);
    printf("[JUNCTIONSPAWN] FAILCLOSED nullTransition=%d nullInventory=%d nullOutput=%d notCommitted=%d loadType=%d loadedWorld=%d targetMismatch=%d runtimeMismatch=%d badHeaderSpawn=%d reset=%d outputAtomic=yes\n",
           invalidNullTransition, invalidNullInventory, invalidNullOutput,
           notCommitted, loadTypeGate, loadedGate, targetMismatch,
           runtimeMismatch, spawnInvalid, resetProof);
    printf("[JUNCTIONSPAWN] RESIDENT snapshotFNV=%08lx->%08lx targetLeftResident=yes payload=%lu entities=%lu enemies=%lu destructibles=%lu packClosed=yes\n",
           (unsigned long)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned long)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned long)residentAfter.totalPayloadBytes,
           (unsigned long)residentAfter.entityCount,
           (unsigned long)residentAfter.enemyCount,
           (unsigned long)residentAfter.destructibleCount);
    printf("[JUNCTIONSPAWN] RAM heap8=%lu->%lu delta=%ld largest8=%lu->%lu delta=%ld persistentHeapBytes=0\n",
           (unsigned long)heapBefore, (unsigned long)heapAfter,
           (long)heapAfter - (long)heapBefore,
           (unsigned long)largestBefore, (unsigned long)largestAfter,
           (long)largestAfter - (long)largestBefore);
    printf("[JUNCTIONSPAWN] LEGACY placementFNV=%08lx->%08lx playerFNV=%08lx->%08lx frameFNV=%08lx->%08lx legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no\n",
           (unsigned long)placementBefore, (unsigned long)placementAfter,
           (unsigned long)playerBefore, (unsigned long)playerAfter,
           (unsigned long)frameBefore, (unsigned long)frameAfter);
    printf("[JUNCTIONSPAWN] PARK state=%d page=%d committedTransition=yes mapSwapCommitted=yes targetMap=9 junctionResident=yes nativeSpawnState=yes spawnProjected=yes spawnApplied=no loadType=0 facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);
}
