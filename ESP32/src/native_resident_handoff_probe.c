#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "native_resident_handoff_probe.h"
#include "native_transition_preflight_final_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define SOURCE_RESOURCE "/intro.bsp"
#define TARGET_RESOURCE "/junction.bsp"

#define EXPECTED_SNAPSHOT_BYTES 96U
#define EXPECTED_SOURCE_SNAPSHOT_FNV 0xb3811f3dU
#define HISTORICAL_SOURCE_HEAP_COST 18008U
#define MAX_SOURCE_ALLOCATOR_OVERHEAD 256U
#define EXPECTED_SOURCE_PAYLOAD 17891U
#define EXPECTED_SOURCE_RUNTIME_BYTES 14095U
#define EXPECTED_SOURCE_MAP_BYTES 1024U
#define EXPECTED_SOURCE_SCRIPT_BYTES 81U
#define EXPECTED_SOURCE_LINE_BYTES 120U
#define EXPECTED_SOURCE_TEXTURE_BYTES 60U
#define EXPECTED_SOURCE_AUTOMAP_BYTES 103U
#define EXPECTED_SOURCE_TOPOLOGY_BYTES 2408U
#define EXPECTED_SOURCE_ARENA_FNV 0xc3882516U
#define EXPECTED_SOURCE_MAP_FNV 0xcd99b98eU
#define EXPECTED_SOURCE_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_SOURCE_LINE_FNV 0xe5e74861U
#define EXPECTED_SOURCE_TEXTURE_FNV 0xf1fc1875U
#define EXPECTED_SOURCE_AUTOMAP_FNV 0x669b1aa7U
#define EXPECTED_SOURCE_TOPOLOGY_FNV 0x3f321e43U
#define EXPECTED_SOURCE_NODES 223U
#define EXPECTED_SOURCE_LINES 480U
#define EXPECTED_SOURCE_SPRITES 344U
#define EXPECTED_SOURCE_EVENTS 93U
#define EXPECTED_SOURCE_BYTECODES 265U
#define EXPECTED_SOURCE_STRINGS 94U
#define EXPECTED_SOURCE_ENTITIES 220U
#define EXPECTED_SOURCE_ENEMIES 30U
#define EXPECTED_SOURCE_DESTRUCTIBLES 13U
#define EXPECTED_SOURCE_BYTES 21823U
#define EXPECTED_SOURCE_CRC32 0x623f34e4U

#define EXPECTED_TARGET_RUNTIME_BYTES 8867U
#define EXPECTED_TARGET_MAP_BYTES 1024U
#define EXPECTED_TARGET_SCRIPT_BYTES 73U
#define EXPECTED_TARGET_LINE_BYTES 52U
#define EXPECTED_TARGET_TEXTURE_BYTES 26U
#define EXPECTED_TARGET_AUTOMAP_BYTES 32U
#define EXPECTED_TARGET_TOPOLOGY_BYTES 336U
#define EXPECTED_TARGET_PAYLOAD 10410U
#define EXPECTED_TARGET_BYTES 21051U
#define EXPECTED_TARGET_CRC32 0x4a2c5800U
#define EXPECTED_TARGET_SOURCE_FNV 0xfefaf5caU
#define EXPECTED_TARGET_NODES 77U
#define EXPECTED_TARGET_LINES 207U
#define EXPECTED_TARGET_SPRITES 48U
#define EXPECTED_TARGET_EVENTS 66U
#define EXPECTED_TARGET_BYTECODES 319U
#define EXPECTED_TARGET_STRINGS 126U
#define EXPECTED_TARGET_GAMEPLAY_ID 2U

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

static uint32_t transitionWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[8];

    if (doomRpg == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL) return 0U;
    values[0] = (uint32_t)doomRpg->game->changeMapParam;
    values[1] = (uint32_t)doomRpg->game->spawnParam;
    values[2] = (uint32_t)(uint16_t)doomRpg->menu->mapNameId;
    values[3] = (uint32_t)doomRpg->menuSystem->menu;
    values[4] = (uint32_t)doomRpg->doomCanvas->state;
    values[5] = (uint32_t)doomRpg->doomCanvas->storyPage;
    values[6] = (uint32_t)(uint16_t)doomRpg->doomCanvas->loadMapID;
    values[7] = (uint32_t)doomRpg->doomCanvas->loadType;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t values[8];

    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->totalTime;
    values[1] = (uint32_t)player->totalMoves;
    values[2] = (uint32_t)player->completedLevels;
    values[3] = (uint32_t)player->killedMonstersLevels;
    values[4] = (uint32_t)player->foundSecretsLevels;
    values[5] = (uint32_t)player->berserkerTics;
    values[6] = (uint32_t)(uintptr_t)player->dogFamiliar;
    values[7] = (uint32_t)player->moves;
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

static int snapshotIsZero(const EspMapResidentSnapshot* snapshot) {
    EspMapResidentSnapshot zero;
    if (snapshot == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(snapshot, &zero, sizeof(zero)) == 0;
}

static int sourceSnapshotMatches(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL &&
           snapshot->runtimeArenaBytes == EXPECTED_SOURCE_RUNTIME_BYTES &&
           snapshot->mapStateBytes == EXPECTED_SOURCE_MAP_BYTES &&
           snapshot->scriptStateBytes == EXPECTED_SOURCE_SCRIPT_BYTES &&
           snapshot->lineStateBytes == EXPECTED_SOURCE_LINE_BYTES &&
           snapshot->textureStateBytes == EXPECTED_SOURCE_TEXTURE_BYTES &&
           snapshot->automapStateBytes == EXPECTED_SOURCE_AUTOMAP_BYTES &&
           snapshot->topologyBytes == EXPECTED_SOURCE_TOPOLOGY_BYTES &&
           snapshot->totalPayloadBytes == EXPECTED_SOURCE_PAYLOAD &&
           snapshot->runtimeFNV1a == EXPECTED_SOURCE_ARENA_FNV &&
           snapshot->mapStateFNV1a == EXPECTED_SOURCE_MAP_FNV &&
           snapshot->scriptStateFNV1a == EXPECTED_SOURCE_SCRIPT_FNV &&
           snapshot->lineStateFNV1a == EXPECTED_SOURCE_LINE_FNV &&
           snapshot->textureStateFNV1a == EXPECTED_SOURCE_TEXTURE_FNV &&
           snapshot->automapStateFNV1a == EXPECTED_SOURCE_AUTOMAP_FNV &&
           snapshot->topologyFNV1a == EXPECTED_SOURCE_TOPOLOGY_FNV &&
           snapshot->nodeCount == EXPECTED_SOURCE_NODES &&
           snapshot->lineCount == EXPECTED_SOURCE_LINES &&
           snapshot->spriteCount == EXPECTED_SOURCE_SPRITES &&
           snapshot->eventCount == EXPECTED_SOURCE_EVENTS &&
           snapshot->byteCodeCount == EXPECTED_SOURCE_BYTECODES &&
           snapshot->stringCount == EXPECTED_SOURCE_STRINGS &&
           snapshot->entityCount == EXPECTED_SOURCE_ENTITIES &&
           snapshot->enemyCount == EXPECTED_SOURCE_ENEMIES &&
           snapshot->destructibleCount == EXPECTED_SOURCE_DESTRUCTIBLES;
}

static int targetSnapshotShapeMatches(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL &&
           snapshot->runtimeArenaBytes == EXPECTED_TARGET_RUNTIME_BYTES &&
           snapshot->mapStateBytes == EXPECTED_TARGET_MAP_BYTES &&
           snapshot->scriptStateBytes == EXPECTED_TARGET_SCRIPT_BYTES &&
           snapshot->lineStateBytes == EXPECTED_TARGET_LINE_BYTES &&
           snapshot->textureStateBytes == EXPECTED_TARGET_TEXTURE_BYTES &&
           snapshot->automapStateBytes == EXPECTED_TARGET_AUTOMAP_BYTES &&
           snapshot->topologyBytes == EXPECTED_TARGET_TOPOLOGY_BYTES &&
           snapshot->totalPayloadBytes == EXPECTED_TARGET_PAYLOAD &&
           snapshot->nodeCount == EXPECTED_TARGET_NODES &&
           snapshot->lineCount == EXPECTED_TARGET_LINES &&
           snapshot->spriteCount == EXPECTED_TARGET_SPRITES &&
           snapshot->eventCount == EXPECTED_TARGET_EVENTS &&
           snapshot->byteCodeCount == EXPECTED_TARGET_BYTECODES &&
           snapshot->stringCount == EXPECTED_TARGET_STRINGS &&
           snapshot->runtimeFNV1a != 0U && snapshot->mapStateFNV1a != 0U &&
           snapshot->scriptStateFNV1a != 0U && snapshot->lineStateFNV1a != 0U &&
           snapshot->textureStateFNV1a != 0U &&
           snapshot->automapStateFNV1a != 0U &&
           snapshot->topologyFNV1a != 0U &&
           snapshot->entityCount <= snapshot->spriteCount &&
           snapshot->enemyCount <= snapshot->entityCount &&
           snapshot->destructibleCount <= snapshot->entityCount;
}

static int introInventoryMatches(const EspBspInventory* inventory) {
    return inventory != NULL &&
           inventory->sourceBytes == EXPECTED_SOURCE_BYTES &&
           inventory->crc32 == EXPECTED_SOURCE_CRC32 &&
           inventory->plan.persistentBytes == EXPECTED_SOURCE_RUNTIME_BYTES &&
           inventory->nodes == EXPECTED_SOURCE_NODES &&
           inventory->lines == EXPECTED_SOURCE_LINES &&
           inventory->mapSprites == EXPECTED_SOURCE_SPRITES &&
           inventory->events == EXPECTED_SOURCE_EVENTS &&
           inventory->byteCodes == EXPECTED_SOURCE_BYTECODES &&
           inventory->strings == EXPECTED_SOURCE_STRINGS &&
           inventory->loadMapId == 1U;
}

static int targetInventoryMatches(const EspBspInventory* inventory) {
    return inventory != NULL &&
           inventory->sourceBytes == EXPECTED_TARGET_BYTES &&
           inventory->crc32 == EXPECTED_TARGET_CRC32 &&
           inventory->fnv1a32 == EXPECTED_TARGET_SOURCE_FNV &&
           inventory->plan.persistentBytes == EXPECTED_TARGET_RUNTIME_BYTES &&
           inventory->nodes == EXPECTED_TARGET_NODES &&
           inventory->lines == EXPECTED_TARGET_LINES &&
           inventory->mapSprites == EXPECTED_TARGET_SPRITES &&
           inventory->events == EXPECTED_TARGET_EVENTS &&
           inventory->byteCodes == EXPECTED_TARGET_BYTECODES &&
           inventory->strings == EXPECTED_TARGET_STRINGS &&
           inventory->loadMapId == EXPECTED_TARGET_GAMEPLAY_ID;
}

void Esp32ResidentHandoffProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32ResidentHandoffProbe_isDone(void) {
    return probeState.done;
}

void Esp32ResidentHandoffProbe_service(struct DoomRPG_s* doomRpg) {
    EspBspInventory introInventory;
    EspBspInventory targetInventory;
    EspBspInventory dummyInventory;
    EspMapResidentSnapshot source;
    EspMapResidentSnapshot sourceCheck;
    EspMapResidentSnapshot target;
    EspMapResidentSnapshot targetCheck;
    EspMapResidentSnapshot restored;
    EspMapResidentSnapshot gateResult;
    EspMapResidentSnapshot busyResult;
    EspMapResidentLifecycleStatus status;
    EspMapResidentLifecycleStatus restoreStatus;
    uint32_t heapSource, heapEmpty1, heapAfterBusy, heapTarget, heapEmpty2, heapRestored;
    uint32_t largestSource, largestEmpty1, largestAfterBusy, largestTarget;
    uint32_t largestEmpty2, largestRestored;
    uint32_t frameBefore, frameAfter;
    uint32_t transitionBefore, transitionAfter;
    uint32_t playerBefore, playerAfter;
    uint32_t sourceSnapshotFNV, targetSnapshotFNV, restoredSnapshotFNV;
    uint32_t started, elapsed;
    uint32_t sourceReleasedCost, targetHeapCost;
    int notEmptyGate, invalidGate, nullCapture, packBusy, busyZero;
    int callerOwnsPack, emptyAtomic, targetExact, emptyExact, restoreExact;
    int sourceReleased = 0;
    int introInventoryReady = 0;
    int passReady = 0;
    int sourceCapture;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32TransitionPreflightFinalProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[RESIDENTHANDOFFPROBE] ARMED Junction preflight proven; reversible resident handoff starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native reversible resident handoff ===\n");
    printf("[RESIDENTHANDOFFPROBE] CONTRACT explicit resetAll + loadFromEmpty lifecycle only: capture Entrance, reject hidden teardown, release all native owners, build full Junction resident runtime+owners, release it, rebuild Entrance exactly; PAK closed at boundaries; no legacy map load/menu/player mutation, no committed map swap, no gameplay\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menu == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->player == NULL) {
        printf("[RESIDENTHANDOFFPROBE] FAILED missing source witness objects\n");
        probeState.done = 1;
        return;
    }

    if (doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspMapResidentSnapshot) != EXPECTED_SNAPSHOT_BYTES) {
        printf("[RESIDENTHANDOFFPROBE] SOURCEBOUNDARY state=%d expected=%d page=%d entities=%d monsters=%d legacyClear=%d packOpen=%d snapshotBytes=%u expectedBytes=%u\n",
               doomRpg->doomCanvas->state, ST_INTRO,
               doomRpg->doomCanvas->storyPage,
               doomRpg->game->numEntities, doomRpg->game->numMonsters,
               legacyRuntimeIsClear(doomRpg->render), EspAssetPack_isOpen(),
               (unsigned int)sizeof(EspMapResidentSnapshot),
               (unsigned int)EXPECTED_SNAPSHOT_BYTES);
        printf("[RESIDENTHANDOFFPROBE] FAILED unsafe source legacy/lifecycle boundary\n");
        probeState.done = 1;
        return;
    }

    memset(&source, 0, sizeof(source));
    sourceCapture = EspMapResidentLifecycle_capture(&source);
    if (!sourceCapture) {
        printf("[RESIDENTHANDOFFPROBE] SOURCEOWNERS capture=0 runtime=%d map=%d script=%d line=%d texture=%d automap=%d topology=%d empty=%d\n",
               EspMapRuntime_isLoaded(), EspMapState_isReady(),
               EspMapScriptState_isReady(), EspMapLineState_isReady(),
               EspMapLineTextureState_isReady(), EspMapAutomapState_isReady(),
               EspMapSpriteTopology_isReady(), EspMapResidentLifecycle_isEmpty());
        printf("[RESIDENTHANDOFFPROBE] FAILED source resident capture\n");
        probeState.done = 1;
        return;
    }

    if (!sourceSnapshotMatches(&source)) {
        printf("[RESIDENTHANDOFFPROBE] SOURCESNAPSHOT payload=%u/%u arena=%u/%u map=%u/%u script=%u/%u line=%u/%u texture=%u/%u automap=%u/%u topology=%u/%u\n",
               (unsigned int)source.totalPayloadBytes, (unsigned int)EXPECTED_SOURCE_PAYLOAD,
               (unsigned int)source.runtimeArenaBytes, (unsigned int)EXPECTED_SOURCE_RUNTIME_BYTES,
               (unsigned int)source.mapStateBytes, (unsigned int)EXPECTED_SOURCE_MAP_BYTES,
               (unsigned int)source.scriptStateBytes, (unsigned int)EXPECTED_SOURCE_SCRIPT_BYTES,
               (unsigned int)source.lineStateBytes, (unsigned int)EXPECTED_SOURCE_LINE_BYTES,
               (unsigned int)source.textureStateBytes, (unsigned int)EXPECTED_SOURCE_TEXTURE_BYTES,
               (unsigned int)source.automapStateBytes, (unsigned int)EXPECTED_SOURCE_AUTOMAP_BYTES,
               (unsigned int)source.topologyBytes, (unsigned int)EXPECTED_SOURCE_TOPOLOGY_BYTES);
        printf("[RESIDENTHANDOFFPROBE] SOURCEFNV arena=%08x/%08x map=%08x/%08x script=%08x/%08x line=%08x/%08x texture=%08x/%08x automap=%08x/%08x topology=%08x/%08x\n",
               (unsigned int)source.runtimeFNV1a, (unsigned int)EXPECTED_SOURCE_ARENA_FNV,
               (unsigned int)source.mapStateFNV1a, (unsigned int)EXPECTED_SOURCE_MAP_FNV,
               (unsigned int)source.scriptStateFNV1a, (unsigned int)EXPECTED_SOURCE_SCRIPT_FNV,
               (unsigned int)source.lineStateFNV1a, (unsigned int)EXPECTED_SOURCE_LINE_FNV,
               (unsigned int)source.textureStateFNV1a, (unsigned int)EXPECTED_SOURCE_TEXTURE_FNV,
               (unsigned int)source.automapStateFNV1a, (unsigned int)EXPECTED_SOURCE_AUTOMAP_FNV,
               (unsigned int)source.topologyFNV1a, (unsigned int)EXPECTED_SOURCE_TOPOLOGY_FNV);
        printf("[RESIDENTHANDOFFPROBE] SOURCECOUNTS nodes=%u/%u lines=%u/%u sprites=%u/%u events=%u/%u byteCodes=%u/%u strings=%u/%u entities=%u/%u enemies=%u/%u destructibles=%u/%u\n",
               (unsigned int)source.nodeCount, (unsigned int)EXPECTED_SOURCE_NODES,
               (unsigned int)source.lineCount, (unsigned int)EXPECTED_SOURCE_LINES,
               (unsigned int)source.spriteCount, (unsigned int)EXPECTED_SOURCE_SPRITES,
               (unsigned int)source.eventCount, (unsigned int)EXPECTED_SOURCE_EVENTS,
               (unsigned int)source.byteCodeCount, (unsigned int)EXPECTED_SOURCE_BYTECODES,
               (unsigned int)source.stringCount, (unsigned int)EXPECTED_SOURCE_STRINGS,
               (unsigned int)source.entityCount, (unsigned int)EXPECTED_SOURCE_ENTITIES,
               (unsigned int)source.enemyCount, (unsigned int)EXPECTED_SOURCE_ENEMIES,
               (unsigned int)source.destructibleCount, (unsigned int)EXPECTED_SOURCE_DESTRUCTIBLES);
        printf("[RESIDENTHANDOFFPROBE] FAILED source resident snapshot mismatch\n");
        probeState.done = 1;
        return;
    }

    sourceSnapshotFNV = hashBytes(&source, sizeof(source));
    if (sourceSnapshotFNV != EXPECTED_SOURCE_SNAPSHOT_FNV) {
        printf("[RESIDENTHANDOFFPROBE] FAILED source snapshot FNV=%08x expected=%08x\n",
               (unsigned int)sourceSnapshotFNV,
               (unsigned int)EXPECTED_SOURCE_SNAPSHOT_FNV);
        probeState.done = 1;
        return;
    }

    heapSource = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestSource = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    transitionBefore = transitionWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);

    memset(&dummyInventory, 0, sizeof(dummyInventory));
    memset(&gateResult, 0xa5, sizeof(gateResult));
    notEmptyGate =
        EspMapResidentLifecycle_loadFromEmpty(TARGET_RESOURCE, &dummyInventory,
                                              &gateResult) ==
            ESP_MAP_RESIDENT_NOT_EMPTY &&
        snapshotIsZero(&gateResult) &&
        EspMapResidentLifecycle_capture(&sourceCheck) &&
        memcmp(&sourceCheck, &source, sizeof(source)) == 0 &&
        !EspAssetPack_isOpen();
    memset(&gateResult, 0xa5, sizeof(gateResult));
    invalidGate =
        EspMapResidentLifecycle_loadFromEmpty(NULL, &dummyInventory, &gateResult) ==
            ESP_MAP_RESIDENT_INVALID && snapshotIsZero(&gateResult);
    nullCapture = !EspMapResidentLifecycle_capture(NULL);
    if (!notEmptyGate || !invalidGate || !nullCapture) {
        printf("[RESIDENTHANDOFFPROBE] FAILED pre-teardown gates\n");
        probeState.done = 1;
        return;
    }

    memset(&introInventory, 0, sizeof(introInventory));
    memset(&targetInventory, 0, sizeof(targetInventory));
    if (!EspBspReader_inventoryPackEntry(SOURCE_RESOURCE, &introInventory) ||
        !introInventoryMatches(&introInventory)) {
        printf("[RESIDENTHANDOFFPROBE] FAILED source inventory\n");
        probeState.done = 1;
        return;
    }
    introInventoryReady = 1;
    if (!EspBspReader_inventoryPackEntry(TARGET_RESOURCE, &targetInventory) ||
        !targetInventoryMatches(&targetInventory) || EspAssetPack_isOpen()) {
        printf("[RESIDENTHANDOFFPROBE] FAILED target inventory\n");
        probeState.done = 1;
        return;
    }

    EspMapResidentLifecycle_resetAll();
    sourceReleased = 1;
    heapEmpty1 = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestEmpty1 = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    sourceReleasedCost = heapEmpty1 - heapSource;
    memset(&gateResult, 0xa5, sizeof(gateResult));
    emptyAtomic = EspMapResidentLifecycle_isEmpty() &&
                  !EspMapResidentLifecycle_capture(&gateResult) &&
                  snapshotIsZero(&gateResult) &&
                  sourceReleasedCost >= source.totalPayloadBytes &&
                  (sourceReleasedCost - source.totalPayloadBytes) <=
                      MAX_SOURCE_ALLOCATOR_OVERHEAD;
    if (!emptyAtomic) {
        printf("[RESIDENTHANDOFFPROBE] FAILED explicit source release\n");
        goto recover;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[RESIDENTHANDOFFPROBE] FAILED busy-gate PAK open\n");
        goto recover;
    }
    memset(&busyResult, 0xa5, sizeof(busyResult));
    packBusy =
        EspMapResidentLifecycle_loadFromEmpty(TARGET_RESOURCE, &targetInventory,
                                              &busyResult) ==
            ESP_MAP_RESIDENT_PACK_BUSY;
    busyZero = snapshotIsZero(&busyResult);
    callerOwnsPack = EspAssetPack_isOpen();
    EspAssetPack_close();
    heapAfterBusy = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfterBusy =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (!packBusy || !busyZero || !callerOwnsPack ||
        !EspMapResidentLifecycle_isEmpty() ||
        heapAfterBusy != heapEmpty1 || largestAfterBusy != largestEmpty1) {
        printf("[RESIDENTHANDOFFPROBE] FAILED empty PACK_BUSY gate\n");
        goto recover;
    }

    started = DoomRPG_GetUpTimeMS();
    status = EspMapResidentLifecycle_loadFromEmpty(TARGET_RESOURCE,
                                                   &targetInventory, &target);
    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapTarget = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestTarget = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    targetHeapCost = heapEmpty1 - heapTarget;
    targetSnapshotFNV = hashBytes(&target, sizeof(target));
    targetExact = status == ESP_MAP_RESIDENT_OK &&
                  targetSnapshotShapeMatches(&target) &&
                  EspMapResidentLifecycle_isReady() &&
                  EspMapResidentLifecycle_capture(&targetCheck) &&
                  memcmp(&targetCheck, &target, sizeof(target)) == 0 &&
                  !EspAssetPack_isOpen() && targetHeapCost >= target.totalPayloadBytes;
    if (!targetExact) {
        printf("[RESIDENTHANDOFFPROBE] FAILED Junction resident build status=%u\n",
               (unsigned int)status);
        goto recover;
    }

    EspMapResidentLifecycle_resetAll();
    heapEmpty2 = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestEmpty2 = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    emptyExact = EspMapResidentLifecycle_isEmpty() &&
                 heapEmpty2 == heapEmpty1 && largestEmpty2 == largestEmpty1;
    if (!emptyExact) {
        printf("[RESIDENTHANDOFFPROBE] FAILED target release/fragmentation\n");
        goto recover;
    }

    restoreStatus = EspMapResidentLifecycle_loadFromEmpty(SOURCE_RESOURCE,
                                                          &introInventory,
                                                          &restored);
    heapRestored = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestRestored =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    restoredSnapshotFNV = hashBytes(&restored, sizeof(restored));
    restoreExact = restoreStatus == ESP_MAP_RESIDENT_OK &&
                   sourceSnapshotMatches(&restored) &&
                   memcmp(&restored, &source, sizeof(source)) == 0 &&
                   restoredSnapshotFNV == EXPECTED_SOURCE_SNAPSHOT_FNV &&
                   heapRestored == heapSource && largestRestored == largestSource &&
                   !EspAssetPack_isOpen();
    if (!restoreExact) {
        printf("[RESIDENTHANDOFFPROBE] FAILED source restoration status=%u\n",
               (unsigned int)restoreStatus);
        goto recover;
    }
    sourceReleased = 0;

    frameAfter = framebufferHash();
    transitionAfter = transitionWitness(doomRpg);
    playerAfter = playerWitness(doomRpg->player);
    passReady = frameAfter == frameBefore &&
                transitionAfter == transitionBefore &&
                playerAfter == playerBefore &&
                legacyRuntimeIsClear(doomRpg->render) &&
                doomRpg->game->numEntities == 0 &&
                doomRpg->game->numMonsters == 0 &&
                doomRpg->doomCanvas->state == ST_INTRO &&
                doomRpg->doomCanvas->storyPage == 3 &&
                !EspAssetPack_isOpen();
    if (!passReady) {
        printf("[RESIDENTHANDOFFPROBE] FAILED legacy/frame final integrity\n");
        probeState.done = 1;
        return;
    }

    printf("[RESIDENTHANDOFF] SOURCE snapshotBytes=%u payload=%u snapshotFNV=%08x heap8=%u largest8=%u arena=%u state=%u script=%u line=%u texture=%u automap=%u topology=%u\n",
           (unsigned int)sizeof(source), (unsigned int)source.totalPayloadBytes,
           (unsigned int)sourceSnapshotFNV, (unsigned int)heapSource,
           (unsigned int)largestSource, (unsigned int)source.runtimeArenaBytes,
           (unsigned int)source.mapStateBytes, (unsigned int)source.scriptStateBytes,
           (unsigned int)source.lineStateBytes, (unsigned int)source.textureStateBytes,
           (unsigned int)source.automapStateBytes, (unsigned int)source.topologyBytes);
    printf("[RESIDENTHANDOFF] EMPTY1 heap8=%u largest8=%u released=%u sourcePayload=%u allocatorOverhead=%u historicalCost=%u allOwnersEmpty=yes\n",
           (unsigned int)heapEmpty1, (unsigned int)largestEmpty1,
           (unsigned int)sourceReleasedCost, (unsigned int)source.totalPayloadBytes,
           (unsigned int)(sourceReleasedCost - source.totalPayloadBytes),
           (unsigned int)HISTORICAL_SOURCE_HEAP_COST);
    printf("[RESIDENTHANDOFF] GATES notEmpty=%d invalid=%d nullCapture=%d packBusy=%d busyZero=%d callerOwnsPack=%d emptyAtomic=yes\n",
           notEmptyGate, invalidGate, nullCapture, packBusy, busyZero,
           callerOwnsPack);
    printf("[RESIDENTHANDOFF] JUNCTION snapshotBytes=%u payload=%u heapCost=%u allocatorOverhead=%u snapshotFNV=%08x elapsed=%ums arena=%u state=%u script=%u line=%u texture=%u automap=%u topology=%u\n",
           (unsigned int)sizeof(target), (unsigned int)target.totalPayloadBytes,
           (unsigned int)targetHeapCost,
           (unsigned int)(targetHeapCost - target.totalPayloadBytes),
           (unsigned int)targetSnapshotFNV, (unsigned int)elapsed,
           (unsigned int)target.runtimeArenaBytes,
           (unsigned int)target.mapStateBytes,
           (unsigned int)target.scriptStateBytes,
           (unsigned int)target.lineStateBytes,
           (unsigned int)target.textureStateBytes,
           (unsigned int)target.automapStateBytes,
           (unsigned int)target.topologyBytes);
    printf("[RESIDENTHANDOFF] JUNCTIONFNV arena=%08x map=%08x script=%08x line=%08x texture=%08x automap=%08x topology=%08x\n",
           (unsigned int)target.runtimeFNV1a,
           (unsigned int)target.mapStateFNV1a,
           (unsigned int)target.scriptStateFNV1a,
           (unsigned int)target.lineStateFNV1a,
           (unsigned int)target.textureStateFNV1a,
           (unsigned int)target.automapStateFNV1a,
           (unsigned int)target.topologyFNV1a);
    printf("[RESIDENTHANDOFF] JUNCTIONTOPO nodes=%u lines=%u sprites=%u events=%u byteCodes=%u strings=%u entities=%u enemies=%u destructibles=%u heap8=%u largest8=%u\n",
           (unsigned int)target.nodeCount, (unsigned int)target.lineCount,
           (unsigned int)target.spriteCount, (unsigned int)target.eventCount,
           (unsigned int)target.byteCodeCount, (unsigned int)target.stringCount,
           (unsigned int)target.entityCount, (unsigned int)target.enemyCount,
           (unsigned int)target.destructibleCount, (unsigned int)heapTarget,
           (unsigned int)largestTarget);
    printf("[RESIDENTHANDOFF] EMPTY2 heap8=%u largest8=%u emptyExact=1 targetReleased=%u fragmentationDelta=0\n",
           (unsigned int)heapEmpty2, (unsigned int)largestEmpty2,
           (unsigned int)targetHeapCost);
    printf("[RESIDENTHANDOFF] RESTORE snapshotFNV=%08x exact=1 heap8=%u->%u largest8=%u->%u sourceCost=%u payload=%u\n",
           (unsigned int)restoredSnapshotFNV,
           (unsigned int)heapSource, (unsigned int)heapRestored,
           (unsigned int)largestSource, (unsigned int)largestRestored,
           (unsigned int)sourceReleasedCost,
           (unsigned int)restored.totalPayloadBytes);
    printf("[RESIDENTHANDOFF] RAM source=%u empty1=%u junction=%u empty2=%u restored=%u sourceCost=%u junctionCost=%u finalDelta=%d largest=%u/%u/%u/%u/%u\n",
           (unsigned int)heapSource, (unsigned int)heapEmpty1,
           (unsigned int)heapTarget, (unsigned int)heapEmpty2,
           (unsigned int)heapRestored, (unsigned int)sourceReleasedCost,
           (unsigned int)targetHeapCost, (int)heapSource - (int)heapRestored,
           (unsigned int)largestSource, (unsigned int)largestEmpty1,
           (unsigned int)largestTarget, (unsigned int)largestEmpty2,
           (unsigned int)largestRestored);
    printf("[RESIDENTHANDOFF] LEGACY playerFNV=%08x->%08x transitionFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes sourceTeardownNativeOnly=yes DoomCanvas_loadMapCalled=no menuMutation=no legacyPlayerMutation=no\n",
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter);
    printf("[RESIDENTHANDOFF] PARK state=%d page=%d nativeResidentLifecycle=yes reversibleHandoff=yes junctionResidentProven=yes sourceRestored=yes targetLeftResident=no packClosed=yes persistentBytes=%u mapSwapCommitted=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sourceReleasedCost,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);

    probeState.done = 1;
    return;

recover:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (sourceReleased && introInventoryReady) {
        EspMapResidentLifecycle_resetAll();
        memset(&restored, 0, sizeof(restored));
        restoreStatus = EspMapResidentLifecycle_loadFromEmpty(
            SOURCE_RESOURCE, &introInventory, &restored);
        printf("[RESIDENTHANDOFF] RECOVERY status=%u sourceRestored=%s snapshotFNV=%08x packClosed=%s\n",
               (unsigned int)restoreStatus,
               (restoreStatus == ESP_MAP_RESIDENT_OK &&
                sourceSnapshotMatches(&restored)) ? "yes" : "no",
               (unsigned int)hashBytes(&restored, sizeof(restored)),
               EspAssetPack_isOpen() ? "no" : "yes");
    }
    probeState.done = 1;
}
