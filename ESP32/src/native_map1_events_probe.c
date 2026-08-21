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

#include <esp_heap_caps.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_events_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_STATE_BYTES 1024U
#define EXPECTED_STATE_FNV 0xcd99b98eU
#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_FIRST_EVENT_TILE 68U
#define EXPECTED_FIRST_EVENT_VALUE 0x00080044U
#define EVENT_TRIGGER_MASK 0x01f80000U

typedef struct Esp32Map1EventsProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1EventsProbeState;

static Esp32Map1EventsProbeState probeState;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t hashU16(uint32_t hash, uint16_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t hashU32(uint32_t hash, uint32_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL) {
        return 0U;
    }
    for (i = 0U; i < length; ++i) {
        hash = hashByte(hash, data[i]);
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
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

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

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() &&
           Esp32Map1AccessProbe_isDone() &&
           Esp32Map1StateProbe_isDone() &&
           EspMapRuntime_isLoaded() && EspMapState_isReady() &&
           !Esp32IntroClock_isActive() && !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int validateEventLookup(const EspMapRuntimeView* runtime,
                               const EspMapStateView* state,
                               uint32_t* outLookupFNV,
                               uint32_t* outFirstTile,
                               uint32_t* outLastTile,
                               uint32_t* outLastValue) {
    EspMapEventRef found;
    EspMapEventRef first;
    uint32_t value;
    uint32_t previousTile = 0U;
    uint32_t firstTile = ESP_MAP_EVENT_TILE_COUNT;
    uint32_t lastTile = ESP_MAP_EVENT_TILE_COUNT;
    uint32_t lastValue = 0U;
    uint32_t cursor = 0U;
    uint32_t expectedValue = 0U;
    uint32_t expectedTile = ESP_MAP_EVENT_TILE_COUNT;
    uint32_t tile;
    uint32_t i;
    uint32_t hash = 2166136261U;
    uint8_t flags;
    int expectedFound;
    int actualFound;

    if (runtime == NULL || state == NULL || outLookupFNV == NULL ||
        outFirstTile == NULL || outLastTile == NULL || outLastValue == NULL ||
        runtime->eventCount != EXPECTED_EVENT_COUNT ||
        state->tileCount != ESP_MAP_EVENT_TILE_COUNT ||
        state->eventRefs != EXPECTED_EVENT_COUNT ||
        state->eventCells != EXPECTED_EVENT_COUNT) {
        return 0;
    }

    for (i = 0U; i < runtime->eventCount; ++i) {
        if (!EspMapRuntime_getEvent(i, &value)) {
            return 0;
        }
        tile = value & ESP_MAP_EVENT_TILE_MASK;
        if ((value & EVENT_TRIGGER_MASK) == 0U ||
            (i > 0U && tile <= previousTile)) {
            return 0;
        }
        if (i == 0U) {
            firstTile = tile;
        }
        previousTile = tile;
        lastTile = tile;
        lastValue = value;
    }

    if (firstTile != EXPECTED_FIRST_EVENT_TILE ||
        !EspMapEvents_findByTile(EXPECTED_FIRST_EVENT_TILE, &first) ||
        first.index != 0U || first.tileIndex != EXPECTED_FIRST_EVENT_TILE ||
        first.value != EXPECTED_FIRST_EVENT_VALUE) {
        return 0;
    }

    if (runtime->eventCount > 0U) {
        if (!EspMapRuntime_getEvent(0U, &expectedValue)) {
            return 0;
        }
        expectedTile = expectedValue & ESP_MAP_EVENT_TILE_MASK;
    }

    for (tile = 0U; tile < ESP_MAP_EVENT_TILE_COUNT; ++tile) {
        expectedFound = cursor < runtime->eventCount && expectedTile == tile;
        actualFound = EspMapEvents_findByTile(tile, &found);
        if (!EspMapState_getTileFlags(tile, &flags) ||
            actualFound != expectedFound ||
            (((flags & ESP_MAP_TILE_EVENTS) != 0U) != expectedFound)) {
            return 0;
        }

        hash = hashU16(hash, (uint16_t)tile);
        hash = hashByte(hash, (uint8_t)(actualFound ? 1U : 0U));

        if (expectedFound) {
            if (found.index != cursor || found.tileIndex != tile ||
                found.value != expectedValue) {
                return 0;
            }
            hash = hashU16(hash, found.index);
            hash = hashU32(hash, found.value);

            ++cursor;
            if (cursor < runtime->eventCount) {
                if (!EspMapRuntime_getEvent(cursor, &expectedValue)) {
                    return 0;
                }
                expectedTile = expectedValue & ESP_MAP_EVENT_TILE_MASK;
            }
            else {
                expectedTile = ESP_MAP_EVENT_TILE_COUNT;
            }
        }
    }

    if (cursor != runtime->eventCount ||
        EspMapEvents_findByTile(ESP_MAP_EVENT_TILE_COUNT, &found) ||
        EspMapEvents_findByTile(EXPECTED_FIRST_EVENT_TILE, NULL)) {
        return 0;
    }

    *outLookupFNV = hash;
    *outFirstTile = firstTile;
    *outLastTile = lastTile;
    *outLastValue = lastValue;
    return 1;
}

void Esp32Map1EventsProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1EventsProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* state;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t arenaBefore;
    uint32_t arenaAfter;
    uint32_t stateBefore;
    uint32_t stateAfter;
    uint32_t lookupFNV;
    uint32_t firstTile;
    uint32_t lastTile;
    uint32_t lastValue;
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }
    if (!Esp32Map1StateProbe_isDone()) {
        return;
    }
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPEVENTPROBE] ARMED native tile state proven; allocation-free tile-event lookup starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO tile event lookup ===\n");
    printf("[MAPEVENTPROBE] CONTRACT binary search directly over compact immutable event records; 0 persistent bytes; no bytecode execution, entities, rendering or gameplay\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPEVENTPROBE] FAILED precondition heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(), (unsigned int)largest8Block());
        return;
    }

    runtime = EspMapRuntime_view();
    state = EspMapState_view();
    if (runtime == NULL || state == NULL ||
        runtime->arenaBytes != EXPECTED_ARENA_BYTES ||
        runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        state->tileCount != EXPECTED_STATE_BYTES ||
        state->stateFNV1a != EXPECTED_STATE_FNV) {
        printf("[MAPEVENTPROBE] FAILED inherited regression arena=%u/%08x state=%u/%08x\n",
               runtime != NULL ? (unsigned int)runtime->arenaBytes : 0U,
               runtime != NULL ? (unsigned int)runtime->arenaFNV1a : 0U,
               state != NULL ? (unsigned int)state->tileCount : 0U,
               state != NULL ? (unsigned int)state->stateFNV1a : 0U);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    stateBefore = fnv1a32(state->tileFlags, state->tileCount);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!validateEventLookup(runtime, state, &lookupFNV,
                             &firstTile, &lastTile, &lastValue)) {
        printf("[MAPEVENTPROBE] FAILED lookup validation\n");
        return;
    }

    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    state = EspMapState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    stateAfter = state != NULL ? fnv1a32(state->tileFlags, state->tileCount) : 0U;

    if (!boundaryIsSafe(doomRpg) || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || arenaAfter != EXPECTED_ARENA_FNV ||
        stateAfter != stateBefore || stateAfter != EXPECTED_STATE_FNV) {
        printf("[MAPEVENTPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x state=%08x->%08x\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)stateBefore, (unsigned int)stateAfter);
        return;
    }

    probeState.done = 1;
    printf("[MAPEVENTS] READY events=%u firstTile=%u lastTile=%u sortedUnique=yes persistentBytes=0\n",
           (unsigned int)runtime->eventCount,
           (unsigned int)firstTile,
           (unsigned int)lastTile);
    printf("[MAPEVENTPROBE] READY lookupFNV=%08x elapsed=%ums hits=%u misses=%u first=%u/0/%08x last=%u/%u/%08x stateEvents=%u/%u\n",
           (unsigned int)lookupFNV,
           (unsigned int)elapsedMs,
           (unsigned int)runtime->eventCount,
           (unsigned int)(ESP_MAP_EVENT_TILE_COUNT - runtime->eventCount),
           (unsigned int)firstTile,
           (unsigned int)EXPECTED_FIRST_EVENT_VALUE,
           (unsigned int)lastTile,
           (unsigned int)(runtime->eventCount - 1U),
           (unsigned int)lastValue,
           (unsigned int)state->eventRefs,
           (unsigned int)state->eventCells);
    printf("[MAPEVENTPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x stateFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)stateBefore, (unsigned int)stateAfter);
    printf("[MAPEVENTPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes persistentBytes=0 entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1EventsProbe_isDone(void) {
    return probeState.done;
}
