#include <SDL.h>
#include <stddef.h>
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
#include "native_map1_event_descriptor_probe.h"
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
#define EXPECTED_BYTECODE_COUNT 265U
#define EXPECTED_FIRST_EVENT_TILE 68U
#define EXPECTED_FIRST_EVENT_VALUE 0x00080044U
#define EXPECTED_LAST_EVENT_TILE 968U
#define EXPECTED_LAST_EVENT_VALUE 0x000c23c8U
#define BYTECODE_MARK_BYTES ((EXPECTED_BYTECODE_COUNT + 7U) / 8U)

typedef struct Esp32Map1EventDescriptorProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1EventDescriptorProbeState;

typedef struct DescriptorStats_s {
    uint32_t descriptorFNV;
    uint32_t linkageFNV;
    uint32_t totalCommandRefs;
    uint32_t uniqueCommandRefs;
    uint32_t overlapCommandRefs;
    uint32_t gapCommandCount;
    uint32_t stateMask;
    uint32_t flagsMask;
    uint32_t minCommandCount;
    uint32_t maxCommandCount;
    uint32_t maxCommandEnd;
    EspMapEventDescriptor first;
    EspMapEventDescriptor last;
} DescriptorStats;

static Esp32Map1EventDescriptorProbeState probeState;

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
           Esp32Map1EventsProbe_isDone() &&
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

static int commandWasMarked(const uint8_t* marks, uint32_t index) {
    return (marks[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0U;
}

static void markCommand(uint8_t* marks, uint32_t index) {
    marks[index >> 3] |= (uint8_t)(1U << (index & 7U));
}

static int byteCodeEqual(const EspMapByteCode* a, const EspMapByteCode* b) {
    return a != NULL && b != NULL &&
           a->id == b->id && a->arg1 == b->arg1 && a->arg2 == b->arg2;
}

static int validateDescriptors(const EspMapRuntimeView* runtime,
                               DescriptorStats* outStats) {
    uint8_t referenced[BYTECODE_MARK_BYTES];
    EspMapEventRef ref;
    EspMapEventRef badRef;
    EspMapEventDescriptor descriptor;
    EspMapByteCode linked;
    EspMapByteCode direct;
    uint32_t raw;
    uint32_t expectedTile;
    uint32_t expectedFirst;
    uint32_t expectedCount;
    uint32_t expectedState;
    uint32_t expectedFlags;
    uint32_t globalIndex;
    uint32_t descriptorHash = 2166136261U;
    uint32_t linkageHash = 2166136261U;
    uint32_t totalRefs = 0U;
    uint32_t uniqueRefs = 0U;
    uint32_t overlapRefs = 0U;
    uint32_t stateMask = 0U;
    uint32_t flagsMask = 0U;
    uint32_t minCount = UINT32_MAX;
    uint32_t maxCount = 0U;
    uint32_t maxEnd = 0U;
    uint32_t i;
    uint32_t j;

    if (runtime == NULL || outStats == NULL ||
        runtime->eventCount != EXPECTED_EVENT_COUNT ||
        runtime->byteCodeCount != EXPECTED_BYTECODE_COUNT) {
        return 0;
    }

    memset(referenced, 0, sizeof(referenced));
    memset(outStats, 0, sizeof(*outStats));

    for (i = 0U; i < runtime->eventCount; ++i) {
        if (!EspMapRuntime_getEvent(i, &raw)) {
            return 0;
        }

        expectedTile = raw & ESP_MAP_EVENT_TILE_MASK;
        expectedFirst =
            (raw & ESP_MAP_EVENT_COMMAND_INDEX_MASK) >>
            ESP_MAP_EVENT_COMMAND_INDEX_SHIFT;
        expectedCount =
            (raw & ESP_MAP_EVENT_COMMAND_COUNT_MASK) >>
            ESP_MAP_EVENT_COMMAND_COUNT_SHIFT;
        expectedState =
            (raw & ESP_MAP_EVENT_STATE_MASK) >> ESP_MAP_EVENT_STATE_SHIFT;
        expectedFlags =
            (raw & ESP_MAP_EVENT_FLAGS_MASK) >> ESP_MAP_EVENT_FLAGS_SHIFT;

        if (expectedCount == 0U ||
            expectedFirst + expectedCount > runtime->byteCodeCount ||
            !EspMapEvents_findByTile(expectedTile, &ref) ||
            ref.index != i || ref.tileIndex != expectedTile || ref.value != raw ||
            !EspMapEvents_describe(&ref, &descriptor)) {
            return 0;
        }

        if (descriptor.value != raw || descriptor.eventIndex != i ||
            descriptor.tileIndex != expectedTile ||
            descriptor.firstCommandIndex != expectedFirst ||
            descriptor.commandCount != expectedCount ||
            descriptor.commandEndIndex != expectedFirst + expectedCount ||
            descriptor.initialState != expectedState ||
            descriptor.flags != expectedFlags) {
            return 0;
        }

        if (i == 0U) {
            outStats->first = descriptor;
        }
        outStats->last = descriptor;

        stateMask |= 1UL << descriptor.initialState;
        flagsMask |= 1UL << descriptor.flags;
        if (descriptor.commandCount < minCount) {
            minCount = descriptor.commandCount;
        }
        if (descriptor.commandCount > maxCount) {
            maxCount = descriptor.commandCount;
        }
        if (descriptor.commandEndIndex > maxEnd) {
            maxEnd = descriptor.commandEndIndex;
        }

        descriptorHash = hashU16(descriptorHash, descriptor.eventIndex);
        descriptorHash = hashU16(descriptorHash, descriptor.tileIndex);
        descriptorHash = hashU16(descriptorHash, descriptor.firstCommandIndex);
        descriptorHash = hashU16(descriptorHash, descriptor.commandEndIndex);
        descriptorHash = hashByte(descriptorHash, descriptor.commandCount);
        descriptorHash = hashByte(descriptorHash, descriptor.initialState);
        descriptorHash = hashByte(descriptorHash, descriptor.flags);
        descriptorHash = hashU32(descriptorHash, descriptor.value);

        for (j = 0U; j < descriptor.commandCount; ++j) {
            globalIndex = (uint32_t)descriptor.firstCommandIndex + j;
            if (!EspMapEvents_getCommand(&descriptor, j, &linked) ||
                !EspMapRuntime_getByteCode(globalIndex, &direct) ||
                !byteCodeEqual(&linked, &direct)) {
                return 0;
            }

            ++totalRefs;
            if (commandWasMarked(referenced, globalIndex)) {
                ++overlapRefs;
            }
            else {
                markCommand(referenced, globalIndex);
                ++uniqueRefs;
            }

            linkageHash = hashU16(linkageHash, descriptor.eventIndex);
            linkageHash = hashByte(linkageHash, (uint8_t)j);
            linkageHash = hashU16(linkageHash, (uint16_t)globalIndex);
            linkageHash = hashByte(linkageHash, linked.id);
            linkageHash = hashU32(linkageHash, linked.arg1);
            linkageHash = hashU32(linkageHash, linked.arg2);
        }

        if (EspMapEvents_getCommand(&descriptor,
                                    descriptor.commandCount,
                                    &linked)) {
            return 0;
        }
    }

    if (outStats->first.eventIndex != 0U ||
        outStats->first.tileIndex != EXPECTED_FIRST_EVENT_TILE ||
        outStats->first.value != EXPECTED_FIRST_EVENT_VALUE ||
        outStats->last.eventIndex != EXPECTED_EVENT_COUNT - 1U ||
        outStats->last.tileIndex != EXPECTED_LAST_EVENT_TILE ||
        outStats->last.value != EXPECTED_LAST_EVENT_VALUE) {
        return 0;
    }

    ref.index = outStats->first.eventIndex;
    ref.tileIndex = outStats->first.tileIndex;
    ref.value = outStats->first.value;
    badRef = ref;
    badRef.value ^= 1U;

    if (EspMapEvents_describe(NULL, &descriptor) ||
        EspMapEvents_describe(&ref, NULL) ||
        EspMapEvents_describe(&badRef, &descriptor) ||
        EspMapEvents_getCommand(NULL, 0U, &linked) ||
        EspMapEvents_getCommand(&outStats->first, 0U, NULL)) {
        return 0;
    }

    outStats->descriptorFNV = descriptorHash;
    outStats->linkageFNV = linkageHash;
    outStats->totalCommandRefs = totalRefs;
    outStats->uniqueCommandRefs = uniqueRefs;
    outStats->overlapCommandRefs = overlapRefs;
    outStats->gapCommandCount = runtime->byteCodeCount - uniqueRefs;
    outStats->stateMask = stateMask;
    outStats->flagsMask = flagsMask;
    outStats->minCommandCount = minCount;
    outStats->maxCommandCount = maxCount;
    outStats->maxCommandEnd = maxEnd;
    return 1;
}

void Esp32Map1EventDescriptorProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1EventDescriptorProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* state;
    DescriptorStats stats;
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
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }
    if (!Esp32Map1EventsProbe_isDone()) {
        return;
    }
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPDESCPROBE] ARMED native event lookup proven; read-only descriptor/bytecode linkage starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO event descriptor linkage ===\n");
    printf("[MAPDESCPROBE] CONTRACT decode event bits + validate linked compact bytecodes; 0 persistent bytes; no script execution, mutation, entities, rendering or gameplay\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPDESCPROBE] FAILED precondition heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(), (unsigned int)largest8Block());
        return;
    }

    runtime = EspMapRuntime_view();
    state = EspMapState_view();
    if (runtime == NULL || state == NULL ||
        runtime->arenaBytes != EXPECTED_ARENA_BYTES ||
        runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        runtime->eventCount != EXPECTED_EVENT_COUNT ||
        runtime->byteCodeCount != EXPECTED_BYTECODE_COUNT ||
        state->tileCount != EXPECTED_STATE_BYTES ||
        state->stateFNV1a != EXPECTED_STATE_FNV) {
        printf("[MAPDESCPROBE] FAILED inherited regression arena=%u/%08x events=%u byteCodes=%u state=%u/%08x\n",
               runtime != NULL ? (unsigned int)runtime->arenaBytes : 0U,
               runtime != NULL ? (unsigned int)runtime->arenaFNV1a : 0U,
               runtime != NULL ? (unsigned int)runtime->eventCount : 0U,
               runtime != NULL ? (unsigned int)runtime->byteCodeCount : 0U,
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

    if (!validateDescriptors(runtime, &stats)) {
        printf("[MAPDESCPROBE] FAILED descriptor validation\n");
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
        printf("[MAPDESCPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x state=%08x->%08x\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)stateBefore, (unsigned int)stateAfter);
        return;
    }

    probeState.done = 1;
    printf("[MAPDESC] READY events=%u byteCodes=%u descriptorFNV=%08x linkageFNV=%08x persistentBytes=0\n",
           (unsigned int)runtime->eventCount,
           (unsigned int)runtime->byteCodeCount,
           (unsigned int)stats.descriptorFNV,
           (unsigned int)stats.linkageFNV);
    printf("[MAPDESCPROBE] READY elapsed=%ums commandRefs=%u unique=%u overlaps=%u gaps=%u countRange=%u..%u maxEnd=%u stateMask=%04x flagsMask=%02x\n",
           (unsigned int)elapsedMs,
           (unsigned int)stats.totalCommandRefs,
           (unsigned int)stats.uniqueCommandRefs,
           (unsigned int)stats.overlapCommandRefs,
           (unsigned int)stats.gapCommandCount,
           (unsigned int)stats.minCommandCount,
           (unsigned int)stats.maxCommandCount,
           (unsigned int)stats.maxCommandEnd,
           (unsigned int)stats.stateMask,
           (unsigned int)stats.flagsMask);
    printf("[MAPDESCPROBE] SAMPLE first=%u/%u/%08x cmd=%u+%u state=%u flags=%u last=%u/%u/%08x cmd=%u+%u state=%u flags=%u\n",
           (unsigned int)stats.first.tileIndex,
           (unsigned int)stats.first.eventIndex,
           (unsigned int)stats.first.value,
           (unsigned int)stats.first.firstCommandIndex,
           (unsigned int)stats.first.commandCount,
           (unsigned int)stats.first.initialState,
           (unsigned int)stats.first.flags,
           (unsigned int)stats.last.tileIndex,
           (unsigned int)stats.last.eventIndex,
           (unsigned int)stats.last.value,
           (unsigned int)stats.last.firstCommandIndex,
           (unsigned int)stats.last.commandCount,
           (unsigned int)stats.last.initialState,
           (unsigned int)stats.last.flags);
    printf("[MAPDESCPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x stateFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)stateBefore, (unsigned int)stateAfter);
    printf("[MAPDESCPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes persistentBytes=0 scriptExecution=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1EventDescriptorProbe_isDone(void) {
    return probeState.done;
}
