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

#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_BASE_0 298U
#define EXPECTED_BASE_1 697U
#define EXPECTED_BASE_2 27U
#define EXPECTED_BASE_3 2U
#define MAX_STATE_ALLOCATOR_OVERHEAD 64U
#define MIN_LARGEST8_AFTER_STATE 32768U

#define ENTRANCE_TEXTURE_ID 7U
#define EVENT_TRIGGER_MASK 0x01f80000U
#define EVENT_TILE_MASK 0x000003ffU
#define LINE_FLAG_EAST_SOUTH 8U
#define LINE_FLAG_WEST_NORTH 16U
#define LINE_FLAG_VERTICAL 256U
#define LINE_FLAG_HORIZONTAL 512U

typedef struct Esp32Map1StateProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1StateProbeState;

static Esp32Map1StateProbeState probeState;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
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
           EspMapRuntime_isLoaded() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int entranceTileForLine(const EspMapLine* line, uint32_t* outTileIndex) {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t midX;
    int32_t midY;
    uint32_t tileX;
    uint32_t tileY;

    if (line == NULL || outTileIndex == NULL) {
        return 0;
    }

    x1 = (int32_t)line->x1;
    y1 = (int32_t)line->y1;
    x2 = (int32_t)line->x2;
    y2 = (int32_t)line->y2;

    if ((line->flags & LINE_FLAG_HORIZONTAL) != 0U) {
        if ((line->flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & LINE_FLAG_WEST_NORTH) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & LINE_FLAG_VERTICAL) != 0U) {
        if ((line->flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & LINE_FLAG_WEST_NORTH) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    midX = x1 + ((x2 - x1) / 2);
    midY = y1 + ((y2 - y1) / 2);
    if (midX < 0 || midY < 0) {
        return 0;
    }

    tileX = (uint32_t)midX >> 6;
    tileY = (uint32_t)midY >> 6;
    if (tileX >= ESP_MAP_STATE_WIDTH || tileY >= ESP_MAP_STATE_HEIGHT) {
        return 0;
    }

    *outTileIndex = (tileY * ESP_MAP_STATE_WIDTH) + tileX;
    return 1;
}

static void setTileBit(uint32_t bits[32], uint32_t tileIndex) {
    bits[tileIndex >> 5] |= 1U << (tileIndex & 31U);
}

static int tileBitIsSet(const uint32_t bits[32], uint32_t tileIndex) {
    return (bits[tileIndex >> 5] & (1U << (tileIndex & 31U))) != 0U;
}

static int validateState(const EspMapStateView* state,
                         uint32_t* outFNV,
                         uint32_t* outEntranceRefs,
                         uint32_t* outEntranceCells,
                         uint32_t* outEventRefs,
                         uint32_t* outEventCells,
                         uint32_t* outFirstEntrance,
                         uint32_t* outFirstEvent) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t eventValue;
    uint32_t entranceBits[32];
    uint32_t eventBits[32];
    uint32_t baseCounts[4] = {0U, 0U, 0U, 0U};
    uint32_t entranceRefs = 0U;
    uint32_t entranceCells = 0U;
    uint32_t eventRefs = 0U;
    uint32_t eventCells = 0U;
    uint32_t firstEntrance = ESP_MAP_STATE_TILE_COUNT;
    uint32_t firstEvent = ESP_MAP_STATE_TILE_COUNT;
    uint32_t tileIndex;
    uint32_t i;
    uint32_t hash = 2166136261U;
    uint8_t flags;
    uint8_t blockValue;

    if (state == NULL || runtime == NULL || outFNV == NULL ||
        outEntranceRefs == NULL || outEntranceCells == NULL ||
        outEventRefs == NULL || outEventCells == NULL ||
        outFirstEntrance == NULL || outFirstEvent == NULL ||
        state->tileCount != ESP_MAP_STATE_TILE_COUNT) {
        return 0;
    }

    memset(entranceBits, 0, sizeof(entranceBits));
    memset(eventBits, 0, sizeof(eventBits));

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            return 0;
        }
        if (line.texture != ENTRANCE_TEXTURE_ID) {
            continue;
        }

        ++entranceRefs;
        if (!entranceTileForLine(&line, &tileIndex)) {
            return 0;
        }
        setTileBit(entranceBits, tileIndex);
    }

    for (i = 0U; i < runtime->eventCount; ++i) {
        if (!EspMapRuntime_getEvent(i, &eventValue)) {
            return 0;
        }
        if ((eventValue & EVENT_TRIGGER_MASK) == 0U) {
            continue;
        }

        ++eventRefs;
        tileIndex = eventValue & EVENT_TILE_MASK;
        if (tileIndex >= ESP_MAP_STATE_TILE_COUNT) {
            return 0;
        }
        setTileBit(eventBits, tileIndex);
    }

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &flags) ||
            !EspMapRuntime_getBlockCell(i, &blockValue) ||
            (flags & 3U) != blockValue ||
            (flags & (uint8_t)~(ESP_MAP_TILE_WALL |
                                ESP_MAP_TILE_SECRET |
                                ESP_MAP_TILE_ENTRANCE |
                                ESP_MAP_TILE_EVENTS |
                                ESP_MAP_TILE_VISITED)) != 0U ||
            (flags & ESP_MAP_TILE_VISITED) != 0U ||
            (((flags & ESP_MAP_TILE_ENTRANCE) != 0U) != tileBitIsSet(entranceBits, i)) ||
            (((flags & ESP_MAP_TILE_EVENTS) != 0U) != tileBitIsSet(eventBits, i))) {
            return 0;
        }

        ++baseCounts[blockValue];
        if ((flags & ESP_MAP_TILE_ENTRANCE) != 0U) {
            ++entranceCells;
            if (firstEntrance == ESP_MAP_STATE_TILE_COUNT) {
                firstEntrance = i;
            }
        }
        if ((flags & ESP_MAP_TILE_EVENTS) != 0U) {
            ++eventCells;
            if (firstEvent == ESP_MAP_STATE_TILE_COUNT) {
                firstEvent = i;
            }
        }

        hash ^= flags;
        hash *= 16777619U;
    }

    if (baseCounts[0] != EXPECTED_BASE_0 ||
        baseCounts[1] != EXPECTED_BASE_1 ||
        baseCounts[2] != EXPECTED_BASE_2 ||
        baseCounts[3] != EXPECTED_BASE_3 ||
        state->baseCounts[0] != baseCounts[0] ||
        state->baseCounts[1] != baseCounts[1] ||
        state->baseCounts[2] != baseCounts[2] ||
        state->baseCounts[3] != baseCounts[3] ||
        state->entranceLineRefs != entranceRefs ||
        state->entranceCells != entranceCells ||
        state->eventRefs != eventRefs ||
        state->eventCells != eventCells ||
        state->stateFNV1a != hash ||
        EspMapState_getTileFlags(ESP_MAP_STATE_TILE_COUNT, &flags)) {
        return 0;
    }

    *outFNV = hash;
    *outEntranceRefs = entranceRefs;
    *outEntranceCells = entranceCells;
    *outEventRefs = eventRefs;
    *outEventCells = eventCells;
    *outFirstEntrance = firstEntrance;
    *outFirstEvent = firstEvent;
    return 1;
}

void Esp32Map1StateProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapState_reset();
}

void Esp32Map1StateProbe_service(struct DoomRPG_s* doomRpgBase) {
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
    uint32_t startedMs;
    uint32_t elapsedMs;
    uint32_t heapUsed;
    uint32_t allocatorOverhead;
    uint32_t stateFNV;
    uint32_t entranceRefs;
    uint32_t entranceCells;
    uint32_t eventRefs;
    uint32_t eventCells;
    uint32_t firstEntrance;
    uint32_t firstEvent;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }

    if (!Esp32Map1AccessProbe_isDone()) {
        return;
    }

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPSTATEPROBE] ARMED accessor contract proven; native mutable tile state starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO mutable tile state ===\n");
    printf("[MAPSTATEPROBE] CONTRACT 1024B tileFlags from blockmap + recovered entrance/event semantics; no entities, rendering or gameplay\n");

    if (!boundaryIsSafe(doomRpg) || EspMapState_isReady()) {
        printf("[MAPSTATEPROBE] FAILED precondition heap8=%u largest8=%u stateReady=%d\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               EspMapState_isReady());
        return;
    }

    runtime = EspMapRuntime_view();
    if (runtime == NULL || runtime->arenaBytes != EXPECTED_ARENA_BYTES ||
        runtime->arenaFNV1a != EXPECTED_ARENA_FNV) {
        printf("[MAPSTATEPROBE] FAILED arena regression bytes=%u fnv=%08x\n",
               runtime != NULL ? (unsigned int)runtime->arenaBytes : 0U,
               runtime != NULL ? (unsigned int)runtime->arenaFNV1a : 0U);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    startedMs = DoomRPG_GetUpTimeMS();

    if (arenaBefore != EXPECTED_ARENA_FNV || !EspMapState_buildFromRuntime()) {
        printf("[MAPSTATEPROBE] FAILED build arenaFNV=%08x\n",
               (unsigned int)arenaBefore);
        return;
    }

    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    state = EspMapState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;

    heapUsed = heapBefore >= heapAfter ? heapBefore - heapAfter : 0U;
    allocatorOverhead = heapUsed >= ESP_MAP_STATE_BYTES ?
                        heapUsed - ESP_MAP_STATE_BYTES : UINT32_MAX;

    if (state == NULL || runtime == NULL ||
        heapAfter >= heapBefore || heapUsed < ESP_MAP_STATE_BYTES ||
        allocatorOverhead > MAX_STATE_ALLOCATOR_OVERHEAD ||
        largestAfter < MIN_LARGEST8_AFTER_STATE ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || !boundaryIsSafe(doomRpg) ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPSTATEPROBE] FAILED postcondition heap8=%u->%u used=%u overhead=%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)heapUsed,
               (unsigned int)allocatorOverhead,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (unsigned int)frameBefore,
               (unsigned int)frameAfter,
               (unsigned int)arenaBefore,
               (unsigned int)arenaAfter);
        EspMapState_reset();
        return;
    }

    if (!validateState(state,
                       &stateFNV,
                       &entranceRefs,
                       &entranceCells,
                       &eventRefs,
                       &eventCells,
                       &firstEntrance,
                       &firstEvent)) {
        printf("[MAPSTATEPROBE] FAILED semantic validation\n");
        EspMapState_reset();
        return;
    }

    probeState.done = 1;
    printf("[MAPSTATEPROBE] READY stateFNV=%08x elapsed=%ums base=%u/%u/%u/%u entrance=%u/%u events=%u/%u firstEntrance=%u firstEvent=%u\n",
           (unsigned int)stateFNV,
           (unsigned int)elapsedMs,
           (unsigned int)state->baseCounts[0],
           (unsigned int)state->baseCounts[1],
           (unsigned int)state->baseCounts[2],
           (unsigned int)state->baseCounts[3],
           (unsigned int)entranceRefs,
           (unsigned int)entranceCells,
           (unsigned int)eventRefs,
           (unsigned int)eventCells,
           (unsigned int)firstEntrance,
           (unsigned int)firstEvent);
    printf("[MAPSTATEPROBE] RAM heap8=%u->%u used=%u payload=%u allocatorOverhead=%u largest8=%u->%u frameFNV=%08x->%08x arenaFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)heapUsed,
           (unsigned int)ESP_MAP_STATE_BYTES,
           (unsigned int)allocatorOverhead,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)arenaBefore,
           (unsigned int)arenaAfter);
    printf("[MAPSTATEPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes tileBytes=%u immutableArena=yes entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)ESP_MAP_STATE_BYTES,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1StateProbe_isDone(void) {
    return probeState.done;
}
