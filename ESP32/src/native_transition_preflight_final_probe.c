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
#include "esp_map_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_map_transition_preflight.h"
#include "native_stats_menu_intent_probe.h"
#include "native_transition_preflight_final_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_LINE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_FNV 0x669b1aa7U
#define EXPECTED_TOPOLOGY_FNV 0x3f321e43U
#define EXPECTED_CATALOG_FNV 0xce322e3fU
#define EXPECTED_RESULT_BYTES 56U

#define EXPECTED_JUNCTION_ENTRY_OFFSET 1974397U
#define EXPECTED_JUNCTION_BYTES 21051U
#define EXPECTED_JUNCTION_CRC32 0x4a2c5800U
#define EXPECTED_JUNCTION_FNV 0xfefaf5caU
#define EXPECTED_JUNCTION_PLAN_BYTES 8867U
#define EXPECTED_JUNCTION_NODES 77U
#define EXPECTED_JUNCTION_LINES 207U
#define EXPECTED_JUNCTION_SPRITES 48U
#define EXPECTED_JUNCTION_EVENTS 66U
#define EXPECTED_JUNCTION_BYTECODES 319U
#define EXPECTED_JUNCTION_STRINGS 126U
#define EXPECTED_JUNCTION_STRING_DATA 12235U
#define EXPECTED_JUNCTION_GAMEPLAY_LOAD_ID 2U

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

static uint32_t hashString(uint32_t hash, const char* text) {
    const uint8_t* p = (const uint8_t*)text;
    if (p == NULL) return 0U;
    while (*p != 0U) {
        hash ^= *p++;
        hash *= 16777619U;
    }
    hash ^= 0U;
    return hash * 16777619U;
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

static int resultIsZero(const EspMapTransitionPreflightResult* result) {
    EspMapTransitionPreflightResult zero;
    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}

static int auditCatalog(uint32_t* outFNV, uint32_t* outRoundTrips) {
    static const char* const expected[ESP_MAP_CATALOG_COUNT] = {
        "/intro.bsp",
        "/level01.bsp",
        "/level02.bsp",
        "/level03.bsp",
        "/level04.bsp",
        "/level05.bsp",
        "/level06.bsp",
        "/level07.bsp",
        "/junction.bsp",
        "/junction_destroyed.bsp",
        "/items.bsp",
        "/reactor.bsp",
        "/endgame.bsp"
    };
    uint32_t hash = 2166136261U;
    uint32_t roundTrips = 0U;
    uint32_t i;

    if (outFNV == NULL || outRoundTrips == NULL) return 0;
    *outFNV = 0U;
    *outRoundTrips = 0U;

    for (i = 0U; i < ESP_MAP_CATALOG_COUNT; ++i) {
        const uint8_t id = (uint8_t)(i + ESP_MAP_CATALOG_FIRST_ID);
        const char* name = EspMapCatalog_nameForId(id);
        uint8_t resolved = 0U;
        if (name == NULL || strcmp(name, expected[i]) != 0 ||
            !EspMapCatalog_idForName(name, &resolved) || resolved != id) {
            return 0;
        }
        hash ^= id;
        hash *= 16777619U;
        hash = hashString(hash, name);
        ++roundTrips;
    }

    *outFNV = hash;
    *outRoundTrips = roundTrips;
    return 1;
}

static int junctionMatchesDiscovery(const EspMapTransitionPreflightResult* result) {
    return result != NULL && result->ready == 1U &&
           result->targetMapId == ESP_MAP_ID_JUNCTION &&
           result->gameplayLoadMapId == EXPECTED_JUNCTION_GAMEPLAY_LOAD_ID &&
           result->entryOffset == EXPECTED_JUNCTION_ENTRY_OFFSET &&
           result->sourceBytes == EXPECTED_JUNCTION_BYTES &&
           result->sourceCrc32 == EXPECTED_JUNCTION_CRC32 &&
           result->sourceFNV1a == EXPECTED_JUNCTION_FNV &&
           result->persistentPlanBytes == EXPECTED_JUNCTION_PLAN_BYTES &&
           result->nodes == EXPECTED_JUNCTION_NODES &&
           result->lines == EXPECTED_JUNCTION_LINES &&
           result->mapSprites == EXPECTED_JUNCTION_SPRITES &&
           result->events == EXPECTED_JUNCTION_EVENTS &&
           result->byteCodes == EXPECTED_JUNCTION_BYTECODES &&
           result->strings == EXPECTED_JUNCTION_STRINGS &&
           result->stringDataBytes == EXPECTED_JUNCTION_STRING_DATA &&
           result->nameHash == EspAssetPack_nameHash("/junction.bsp");
}

void Esp32TransitionPreflightFinalProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32TransitionPreflightFinalProbe_isDone(void) {
    return probeState.done;
}

void Esp32TransitionPreflightFinalProbe_service(struct DoomRPG_s* doomRpg) {
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapAutomapStateView* automapState;
    const EspMapSpriteTopologyView* topology;
    EspMapTransitionPreflightResult result;
    EspMapTransitionPreflightResult repeat;
    EspMapTransitionPreflightResult invalid;
    EspMapTransitionPreflightResult busy;
    EspMapTransitionPreflightStatus status;
    uint32_t heapBefore, heapOpen, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t transitionBefore, transitionAfter;
    uint32_t playerBefore, playerAfter;
    uint32_t arenaBefore, mapBefore, scriptBefore;
    uint32_t lineBefore, textureBefore, automapBefore, topologyBefore;
    uint32_t catalogFNV, catalogRoundTrips;
    uint32_t resultFNV, repeatFNV;
    uint32_t started, elapsed;
    uint8_t badNameId = 0xa5U;
    int invalidName;
    int target0;
    int target14;
    int nullResult;
    int packBusy;
    int busyZero;
    int repeatExact;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32StatsMenuIntentProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[TRANSITIONPREFLIGHTFINAL] ARMED corrected resource/gameplay ID model; Junction preflight starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction transition preflight v2 ===\n");
    printf("[TRANSITIONPREFLIGHTFINAL] CONTRACT resourceMapId selects /junction.bsp while BSP gameplayLoadMapId remains an independent progression semantic (Junction=2 hub gate); verify complete target CRC/structure twice, close PAK and preserve Entrance exactly; no source teardown, map swap, legacy mutation or persistent allocation\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menu == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->player == NULL) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED missing legacy witness objects\n");
        probeState.done = 1;
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    topology = EspMapSpriteTopology_view();

    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        lineState == NULL || textureState == NULL || automapState == NULL ||
        topology == NULL || runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        mapState->stateFNV1a != EXPECTED_MAP_STATE_FNV ||
        hashBytes(scriptState->storage, scriptState->storageBytes) != EXPECTED_SCRIPT_FNV ||
        lineState->stateFNV1a != EXPECTED_LINE_FNV ||
        textureState->stateFNV1a != EXPECTED_TEXTURE_FNV ||
        automapState->stateFNV1a != EXPECTED_AUTOMAP_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        MAP_JUNCTION != ESP_MAP_ID_JUNCTION ||
        MAP_END_GAME != ESP_MAP_ID_END_GAME ||
        sizeof(EspMapTransitionPreflightResult) != EXPECTED_RESULT_BYTES) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED unsafe boundary\n");
        probeState.done = 1;
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    transitionBefore = transitionWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);
    arenaBefore = runtime->arenaFNV1a;
    mapBefore = mapState->stateFNV1a;
    scriptBefore = hashBytes(scriptState->storage, scriptState->storageBytes);
    lineBefore = lineState->stateFNV1a;
    textureBefore = textureState->stateFNV1a;
    automapBefore = automapState->stateFNV1a;
    topologyBefore = topology->stateFNV1a;

    if (!auditCatalog(&catalogFNV, &catalogRoundTrips) ||
        catalogRoundTrips != ESP_MAP_CATALOG_COUNT ||
        catalogFNV != EXPECTED_CATALOG_FNV) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED catalog audit\n");
        probeState.done = 1;
        return;
    }
    invalidName = !EspMapCatalog_idForName("/not-a-map.bsp", &badNameId) &&
                  badNameId == 0U &&
                  EspMapCatalog_nameForId(0U) == NULL &&
                  EspMapCatalog_nameForId(14U) == NULL;
    if (!invalidName) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED catalog failclosed\n");
        probeState.done = 1;
        return;
    }

    started = DoomRPG_GetUpTimeMS();
    memset(&result, 0xa5, sizeof(result));
    status = EspMapTransitionPreflight_run(ESP_MAP_ID_JUNCTION, &result);
    elapsed = DoomRPG_GetUpTimeMS() - started;
    resultFNV = hashBytes(&result, sizeof(result));
    if (status != ESP_MAP_TRANSITION_PREFLIGHT_OK ||
        !junctionMatchesDiscovery(&result) || EspAssetPack_isOpen()) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED corrected Junction preflight\n");
        probeState.done = 1;
        return;
    }

    memset(&repeat, 0xa5, sizeof(repeat));
    repeatExact = EspMapTransitionPreflight_run(ESP_MAP_ID_JUNCTION, &repeat) ==
                      ESP_MAP_TRANSITION_PREFLIGHT_OK &&
                  memcmp(&repeat, &result, sizeof(result)) == 0;
    repeatFNV = hashBytes(&repeat, sizeof(repeat));
    if (!repeatExact || repeatFNV != resultFNV ||
        !junctionMatchesDiscovery(&repeat) || EspAssetPack_isOpen()) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED repeat proof\n");
        probeState.done = 1;
        return;
    }

    memset(&invalid, 0xa5, sizeof(invalid));
    target0 = EspMapTransitionPreflight_run(0U, &invalid) ==
                  ESP_MAP_TRANSITION_PREFLIGHT_INVALID && resultIsZero(&invalid);
    memset(&invalid, 0xa5, sizeof(invalid));
    target14 = EspMapTransitionPreflight_run(14U, &invalid) ==
                   ESP_MAP_TRANSITION_PREFLIGHT_INVALID && resultIsZero(&invalid);
    nullResult = EspMapTransitionPreflight_run(ESP_MAP_ID_JUNCTION, NULL) ==
                     ESP_MAP_TRANSITION_PREFLIGHT_INVALID;

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED busy-gate pack open\n");
        probeState.done = 1;
        return;
    }
    heapOpen = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestOpen = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    memset(&busy, 0xa5, sizeof(busy));
    packBusy = EspMapTransitionPreflight_run(ESP_MAP_ID_JUNCTION, &busy) ==
                   ESP_MAP_TRANSITION_PREFLIGHT_PACK_BUSY;
    busyZero = resultIsZero(&busy);
    EspAssetPack_close();

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    transitionAfter = transitionWitness(doomRpg);
    playerAfter = playerWitness(doomRpg->player);

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    topology = EspMapSpriteTopology_view();

    if (!target0 || !target14 || !nullResult || !packBusy || !busyZero ||
        EspAssetPack_isOpen() || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        transitionAfter != transitionBefore || playerAfter != playerBefore ||
        runtime == NULL || runtime->arenaFNV1a != arenaBefore ||
        mapState == NULL || mapState->stateFNV1a != mapBefore ||
        scriptState == NULL ||
        hashBytes(scriptState->storage, scriptState->storageBytes) != scriptBefore ||
        lineState == NULL || lineState->stateFNV1a != lineBefore ||
        textureState == NULL || textureState->stateFNV1a != textureBefore ||
        automapState == NULL || automapState->stateFNV1a != automapBefore ||
        topology == NULL || topology->stateFNV1a != topologyBefore ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render)) {
        printf("[TRANSITIONPREFLIGHTFINAL] FAILED failclosed/integrity audit\n");
        probeState.done = 1;
        return;
    }

    printf("[TRANSITIONPREFLIGHT] READY resultBytes=%u resourceMapId=%u gameplayLoadMapId=%u hubProgressionGate=%u entryOffset=%u size=%u crc32=%08x fnv1a=%08x planBytes=%u resultFNV=%08x elapsed=%ums ready=%u\n",
           (unsigned int)sizeof(result), (unsigned int)result.targetMapId,
           (unsigned int)result.gameplayLoadMapId,
           (unsigned int)(result.gameplayLoadMapId == 2U),
           (unsigned int)result.entryOffset, (unsigned int)result.sourceBytes,
           (unsigned int)result.sourceCrc32, (unsigned int)result.sourceFNV1a,
           (unsigned int)result.persistentPlanBytes, (unsigned int)resultFNV,
           (unsigned int)elapsed, (unsigned int)result.ready);
    printf("[TRANSITIONPREFLIGHT] STRUCT nodes=%u lines=%u mapSprites=%u events=%u byteCodes=%u strings=%u stringData=%u\n",
           (unsigned int)result.nodes, (unsigned int)result.lines,
           (unsigned int)result.mapSprites, (unsigned int)result.events,
           (unsigned int)result.byteCodes, (unsigned int)result.strings,
           (unsigned int)result.stringDataBytes);
    printf("[TRANSITIONPREFLIGHT] CATALOG count=%u roundtrip=%u catalogFNV=%08x invalidName=%d legacyIds=1 junctionName=%s\n",
           (unsigned int)ESP_MAP_CATALOG_COUNT, (unsigned int)catalogRoundTrips,
           (unsigned int)catalogFNV, invalidName,
           EspMapCatalog_nameForId(ESP_MAP_ID_JUNCTION));
    printf("[TRANSITIONPREFLIGHT] REPEAT exact=1 firstFNV=%08x repeatFNV=%08x resourceGameplayDistinct=1\n",
           (unsigned int)resultFNV, (unsigned int)repeatFNV);
    printf("[TRANSITIONPREFLIGHT] FAILCLOSED target0=%d target14=%d nullResult=%d packBusy=%d busyZero=%d stateAtomic=yes\n",
           target0, target14, nullResult, packBusy, busyZero);
    printf("[TRANSITIONPREFLIGHT] IO packIO=yes fullTargetCRC=yes window=%uB packClosed=yes heapOpen=%u transientPackCost=%d largestOpen=%u\n",
           (unsigned int)ESP_BSP_READER_BUFFER_BYTES,
           (unsigned int)heapOpen, (int)heapBefore - (int)heapOpen,
           (unsigned int)largestOpen);
    printf("[TRANSITIONPREFLIGHT] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d persistentHeapBytes=0 frameFNV=%08x->%08x arenaFNV=%08x mapStateFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x automapFNV=%08x topologyFNV=%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (int)heapBefore - (int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (int)largestBefore - (int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)mapBefore,
           (unsigned int)scriptBefore, (unsigned int)lineBefore,
           (unsigned int)textureBefore, (unsigned int)automapBefore,
           (unsigned int)topologyBefore);
    printf("[TRANSITIONPREFLIGHT] LEGACY playerFNV=%08x->%08x transitionFNV=%08x->%08x legacyRuntimeClear=yes sourceTeardown=no mapLoad=no menuMutation=no mapSwap=no\n",
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter);
    printf("[TRANSITIONPREFLIGHT] PARK state=%d page=%d nativeCatalog=yes nativeTargetPreflight=yes resourceMapId=%u gameplayLoadMapId=%u targetReady=yes sourceMapPreserved=yes packClosed=yes persistentBytes=0 mapSwap=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)result.targetMapId,
           (unsigned int)result.gameplayLoadMapId,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);

    probeState.done = 1;
}
