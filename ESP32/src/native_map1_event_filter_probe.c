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

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
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
#define EXPECTED_SCRIPT_EVENT_BYTES 47U
#define EXPECTED_SCRIPT_REMOVED_BYTES 34U
#define EXPECTED_SCRIPT_BYTES 81U
#define MAX_SCRIPT_ALLOCATOR_OVERHEAD 64U
#define MIN_LARGEST8_AFTER_SCRIPT 32768U

#define FILTER_STATE_COUNT 16U
#define FILTER_RUN_FLAG_COUNT 12U
#define FILTER_KEY_SET_COUNT 8U
#define FILTER_CONTEXT_COUNT \
    (FILTER_STATE_COUNT * FILTER_RUN_FLAG_COUNT * FILTER_KEY_SET_COUNT)

static const uint32_t filterRunFlags[FILTER_RUN_FLAG_COUNT] = {
    0U,
    0x00000001U, 0x00000002U, 0x00000004U, 0x00000008U,
    0x00000010U, 0x00000020U, 0x00000040U, 0x00000080U,
    0x00000100U,
    0x00000400U,
    0x00000500U
};

static const uint32_t filterKeySets[FILTER_KEY_SET_COUNT] = {
    0U,
    ESP_MAP_PLAYER_KEY_GREEN,
    ESP_MAP_PLAYER_KEY_YELLOW,
    ESP_MAP_PLAYER_KEY_BLUE,
    ESP_MAP_PLAYER_KEY_RED,
    ESP_MAP_PLAYER_KEY_GREEN | ESP_MAP_PLAYER_KEY_YELLOW,
    ESP_MAP_PLAYER_KEY_GREEN | ESP_MAP_PLAYER_KEY_YELLOW |
        ESP_MAP_PLAYER_KEY_BLUE,
    ESP_MAP_PLAYER_KEY_GREEN | ESP_MAP_PLAYER_KEY_YELLOW |
        ESP_MAP_PLAYER_KEY_BLUE | ESP_MAP_PLAYER_KEY_RED
};

typedef struct Esp32Map1EventFilterProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1EventFilterProbeState;

typedef struct FilterStats_s {
    uint32_t filterFNV;
    uint32_t resumeFNV;
    uint32_t contexts;
    uint32_t evaluations;
    uint32_t eligible;
    uint32_t eventBlocked;
    uint32_t beforeStart;
    uint32_t removed;
    uint32_t stateMismatch;
    uint32_t keyMismatch;
    uint32_t flagsMismatch;
    uint32_t blockInputEvents;
} FilterStats;

static Esp32Map1EventFilterProbeState probeState;

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

static int boundaryCommon(const DoomRPG_t* doomRpg) {
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
           Esp32Map1EventDescriptorProbe_isDone() &&
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

static int descriptorByIndex(uint32_t eventIndex,
                             EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t raw;

    if (outDescriptor == NULL || eventIndex > 0xffffU ||
        !EspMapRuntime_getEvent(eventIndex, &raw)) {
        return 0;
    }

    ref.index = (uint16_t)eventIndex;
    ref.tileIndex = (uint16_t)(raw & ESP_MAP_EVENT_TILE_MASK);
    ref.value = raw;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static uint32_t referenceEffectiveFlags(uint32_t inputFlags,
                                        uint32_t playerKeys) {
    uint32_t flags = inputFlags;

    if ((playerKeys & ESP_MAP_PLAYER_KEY_RED) != 0U) {
        flags |= ESP_MAP_RUN_KEY_RED;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_BLUE) != 0U) {
        flags |= ESP_MAP_RUN_KEY_BLUE;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_GREEN) != 0U) {
        flags |= ESP_MAP_RUN_KEY_GREEN;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_YELLOW) != 0U) {
        flags |= ESP_MAP_RUN_KEY_YELLOW;
    }
    return flags;
}

static uint32_t referenceStateMask(uint8_t currentState) {
    if (currentState == 0U) {
        return 0U;
    }
    return 0x00010000UL << (currentState - 1U);
}

static uint8_t referenceDecision(const EspMapEventDescriptor* descriptor,
                                 uint8_t currentState,
                                 uint32_t startOffset,
                                 uint32_t inputFlags,
                                 uint32_t playerKeys,
                                 uint32_t commandOffset,
                                 uint8_t removed,
                                 uint32_t arg2) {
    uint32_t stateMask;
    uint32_t effectiveFlags;
    uint32_t keyBits;
    uint32_t effectiveKeyBits;

    if ((descriptor->flags & ESP_MAP_EVENT_FLAG_BLOCK_INPUT) != 0U &&
        (inputFlags & ESP_MAP_EVENT_BLOCK_INPUT_RUN_FLAG) != 0U) {
        return ESP_MAP_EVENT_COMMAND_EVENT_BLOCKED;
    }
    if (commandOffset < startOffset) {
        return ESP_MAP_EVENT_COMMAND_BEFORE_START;
    }
    if (removed != 0U) {
        return ESP_MAP_EVENT_COMMAND_REMOVED;
    }

    stateMask = referenceStateMask(currentState);
    if (stateMask != 0U) {
        if ((arg2 & stateMask) == 0U) {
            return ESP_MAP_EVENT_COMMAND_STATE_MISMATCH;
        }
    }
    else if ((arg2 & ESP_MAP_EVENT_STATE_ARG_MASK) != 0U) {
        return ESP_MAP_EVENT_COMMAND_STATE_MISMATCH;
    }

    effectiveFlags = referenceEffectiveFlags(inputFlags, playerKeys);
    keyBits = arg2 & ESP_MAP_EVENT_KEY_ARG_MASK;
    effectiveKeyBits = effectiveFlags & ESP_MAP_EVENT_KEY_ARG_MASK;
    if (keyBits != 0U && keyBits != effectiveKeyBits) {
        return ESP_MAP_EVENT_COMMAND_KEY_MISMATCH;
    }
    if ((effectiveFlags & arg2) == 0U) {
        return ESP_MAP_EVENT_COMMAND_FLAGS_MISMATCH;
    }
    return ESP_MAP_EVENT_COMMAND_ELIGIBLE;
}

static void countDecision(FilterStats* stats, uint8_t decision) {
    switch (decision) {
        case ESP_MAP_EVENT_COMMAND_ELIGIBLE:
            ++stats->eligible;
            break;
        case ESP_MAP_EVENT_COMMAND_EVENT_BLOCKED:
            ++stats->eventBlocked;
            break;
        case ESP_MAP_EVENT_COMMAND_BEFORE_START:
            ++stats->beforeStart;
            break;
        case ESP_MAP_EVENT_COMMAND_REMOVED:
            ++stats->removed;
            break;
        case ESP_MAP_EVENT_COMMAND_STATE_MISMATCH:
            ++stats->stateMismatch;
            break;
        case ESP_MAP_EVENT_COMMAND_KEY_MISMATCH:
            ++stats->keyMismatch;
            break;
        case ESP_MAP_EVENT_COMMAND_FLAGS_MISMATCH:
            ++stats->flagsMismatch;
            break;
        default:
            break;
    }
}

static int validateScriptState(uint32_t* outInitialFNV,
                               uint32_t* outMutatedFNV) {
    const EspMapScriptStateView* view = EspMapScriptState_view();
    EspMapEventDescriptor descriptor;
    uint8_t state;
    uint8_t removed;
    uint32_t initialFNV;
    uint32_t mutatedFNV;
    uint32_t restoredFNV;
    uint32_t i;

    if (view == NULL || outInitialFNV == NULL || outMutatedFNV == NULL ||
        view->storage == NULL || view->eventStatesPacked == NULL ||
        view->removedCommandBits == NULL ||
        view->eventCount != EXPECTED_EVENT_COUNT ||
        view->byteCodeCount != EXPECTED_BYTECODE_COUNT ||
        view->eventStateBytes != EXPECTED_SCRIPT_EVENT_BYTES ||
        view->removedCommandBytes != EXPECTED_SCRIPT_REMOVED_BYTES ||
        view->storageBytes != EXPECTED_SCRIPT_BYTES) {
        return 0;
    }

    for (i = 0U; i < view->eventCount; ++i) {
        if (!descriptorByIndex(i, &descriptor) ||
            !EspMapScriptState_getEventState(i, &state) ||
            state != descriptor.initialState || state != 0U) {
            return 0;
        }
    }
    for (i = 0U; i < view->byteCodeCount; ++i) {
        if (!EspMapScriptState_isCommandRemoved(i, &removed) || removed != 0U) {
            return 0;
        }
    }

    if (EspMapScriptState_getEventState(view->eventCount, &state) ||
        EspMapScriptState_setEventState(view->eventCount, 0U) ||
        EspMapScriptState_setEventState(0U, 16U) ||
        EspMapScriptState_isCommandRemoved(view->byteCodeCount, &removed) ||
        EspMapScriptState_setCommandRemoved(view->byteCodeCount, 0U) ||
        EspMapScriptState_setCommandRemoved(0U, 2U)) {
        return 0;
    }

    initialFNV = fnv1a32(view->storage, view->storageBytes);
    if (!EspMapScriptState_setEventState(0U, 15U) ||
        !EspMapScriptState_setCommandRemoved(0U, 1U) ||
        !EspMapScriptState_getEventState(0U, &state) || state != 15U ||
        !EspMapScriptState_isCommandRemoved(0U, &removed) || removed != 1U) {
        return 0;
    }

    mutatedFNV = fnv1a32(view->storage, view->storageBytes);
    if (mutatedFNV == initialFNV ||
        !EspMapScriptState_setEventState(0U, 0U) ||
        !EspMapScriptState_setCommandRemoved(0U, 0U)) {
        return 0;
    }

    restoredFNV = fnv1a32(view->storage, view->storageBytes);
    if (restoredFNV != initialFNV) {
        return 0;
    }

    *outInitialFNV = initialFNV;
    *outMutatedFNV = mutatedFNV;
    return 1;
}

static int validateFilterMatrix(FilterStats* stats) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult result;
    EspMapByteCode command;
    uint32_t filterHash = 2166136261U;
    uint32_t eventIndex;
    uint32_t state;
    uint32_t runIndex;
    uint32_t keyIndex;
    uint32_t commandOffset;
    uint32_t expectedFlags;
    uint32_t expectedStateMask;
    uint8_t expectedDecision;

    if (stats == NULL) {
        return 0;
    }
    memset(stats, 0, sizeof(*stats));

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) {
            return 0;
        }
        if ((descriptor.flags & ESP_MAP_EVENT_FLAG_BLOCK_INPUT) != 0U) {
            ++stats->blockInputEvents;
        }

        for (state = 0U; state < FILTER_STATE_COUNT; ++state) {
            for (runIndex = 0U; runIndex < FILTER_RUN_FLAG_COUNT; ++runIndex) {
                for (keyIndex = 0U; keyIndex < FILTER_KEY_SET_COUNT; ++keyIndex) {
                    if (!EspMapEventFilter_prepare(
                            &descriptor,
                            (uint8_t)state,
                            0U,
                            filterRunFlags[runIndex],
                            filterKeySets[keyIndex],
                            &plan)) {
                        return 0;
                    }

                    expectedFlags = referenceEffectiveFlags(
                        filterRunFlags[runIndex], filterKeySets[keyIndex]);
                    expectedStateMask = referenceStateMask((uint8_t)state);
                    if (plan.currentState != state || plan.startCommandOffset != 0U ||
                        plan.inputFlags != filterRunFlags[runIndex] ||
                        plan.effectiveFlags != expectedFlags ||
                        plan.stateArgMask != expectedStateMask ||
                        plan.eventIndex != descriptor.eventIndex ||
                        plan.eventBlocked !=
                            (uint8_t)(((descriptor.flags & ESP_MAP_EVENT_FLAG_BLOCK_INPUT) != 0U &&
                                       (filterRunFlags[runIndex] &
                                        ESP_MAP_EVENT_BLOCK_INPUT_RUN_FLAG) != 0U) ? 1U : 0U)) {
                        return 0;
                    }

                    ++stats->contexts;
                    filterHash = hashU16(filterHash, descriptor.eventIndex);
                    filterHash = hashByte(filterHash, (uint8_t)state);
                    filterHash = hashU32(filterHash, filterRunFlags[runIndex]);
                    filterHash = hashU32(filterHash, filterKeySets[keyIndex]);
                    filterHash = hashU32(filterHash, plan.effectiveFlags);
                    filterHash = hashU32(filterHash, plan.stateArgMask);
                    filterHash = hashByte(filterHash, plan.eventBlocked);

                    for (commandOffset = 0U;
                         commandOffset < descriptor.commandCount;
                         ++commandOffset) {
                        if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command) ||
                            !EspMapEventFilter_evaluate(&descriptor, &plan,
                                                       commandOffset, 0U, &result)) {
                            return 0;
                        }

                        expectedDecision = referenceDecision(
                            &descriptor,
                            (uint8_t)state,
                            0U,
                            filterRunFlags[runIndex],
                            filterKeySets[keyIndex],
                            commandOffset,
                            0U,
                            command.arg2);

                        if (result.decision != expectedDecision ||
                            result.globalCommandIndex !=
                                descriptor.firstCommandIndex + commandOffset ||
                            result.commandOffset != commandOffset ||
                            result.codeId != command.id || result.arg2 != command.arg2) {
                            return 0;
                        }

                        ++stats->evaluations;
                        countDecision(stats, result.decision);
                        filterHash = hashU16(filterHash, result.globalCommandIndex);
                        filterHash = hashByte(filterHash, result.commandOffset);
                        filterHash = hashByte(filterHash, result.codeId);
                        filterHash = hashU32(filterHash, result.arg2);
                        filterHash = hashByte(filterHash, result.decision);
                    }
                }
            }
        }
    }

    if (stats->contexts != EXPECTED_EVENT_COUNT * FILTER_CONTEXT_COUNT ||
        stats->evaluations != EXPECTED_BYTECODE_COUNT * FILTER_CONTEXT_COUNT ||
        stats->beforeStart != 0U || stats->removed != 0U ||
        stats->blockInputEvents == 0U) {
        return 0;
    }

    stats->filterFNV = filterHash;
    return 1;
}

static int validateResumeAndRemoved(FilterStats* stats) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult result;
    EspMapByteCode command;
    uint32_t starts[3];
    uint32_t resumeHash = 2166136261U;
    uint32_t eventIndex;
    uint32_t startIndex;
    uint32_t commandOffset;
    uint8_t expectedDecision;
    uint8_t removed;

    if (stats == NULL) {
        return 0;
    }

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) {
            return 0;
        }

        starts[0] = 0U;
        starts[1] = descriptor.commandCount >> 1;
        starts[2] = descriptor.commandCount;

        for (startIndex = 0U; startIndex < 3U; ++startIndex) {
            if (!EspMapEventFilter_prepare(&descriptor, 0U, starts[startIndex],
                                           0x000001ffU, 0U, &plan)) {
                return 0;
            }
            resumeHash = hashU16(resumeHash, descriptor.eventIndex);
            resumeHash = hashByte(resumeHash, (uint8_t)starts[startIndex]);

            for (commandOffset = 0U;
                 commandOffset < descriptor.commandCount;
                 ++commandOffset) {
                if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command) ||
                    !EspMapScriptState_isCommandRemoved(
                        descriptor.firstCommandIndex + commandOffset, &removed) ||
                    !EspMapEventFilter_evaluate(&descriptor, &plan, commandOffset,
                                               removed, &result)) {
                    return 0;
                }

                expectedDecision = referenceDecision(
                    &descriptor, 0U, starts[startIndex], 0x000001ffU, 0U,
                    commandOffset, removed, command.arg2);
                if (result.decision != expectedDecision) {
                    return 0;
                }
                resumeHash = hashByte(resumeHash, result.decision);
                resumeHash = hashU16(resumeHash, result.globalCommandIndex);
            }
        }
    }

    if (!descriptorByIndex(0U, &descriptor) ||
        !EspMapEventFilter_prepare(&descriptor, 0U, 0U, 0x000001ffU, 0U, &plan) ||
        !EspMapEventFilter_evaluate(&descriptor, &plan, 0U, 1U, &result) ||
        result.decision != ESP_MAP_EVENT_COMMAND_REMOVED ||
        EspMapEventFilter_prepare(&descriptor, 16U, 0U, 0U, 0U, &plan) ||
        EspMapEventFilter_prepare(&descriptor, 0U,
                                  (uint32_t)descriptor.commandCount + 1U,
                                  0U, 0U, &plan) ||
        EspMapEventFilter_evaluate(&descriptor, &plan,
                                   descriptor.commandCount, 0U, &result) ||
        EspMapEventFilter_evaluate(&descriptor, &plan, 0U, 2U, &result) ||
        EspMapEventFilter_evaluate(NULL, &plan, 0U, 0U, &result) ||
        EspMapEventFilter_evaluate(&descriptor, NULL, 0U, 0U, &result) ||
        EspMapEventFilter_evaluate(&descriptor, &plan, 0U, 0U, NULL)) {
        return 0;
    }

    stats->resumeFNV = resumeHash;
    return 1;
}

void Esp32Map1EventFilterProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapScriptState_reset();
}

void Esp32Map1EventFilterProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    FilterStats stats;
    uint32_t heapBefore;
    uint32_t heapAfterBuild;
    uint32_t heapAfterFilter;
    uint32_t largestBefore;
    uint32_t largestAfterBuild;
    uint32_t largestAfterFilter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t arenaBefore;
    uint32_t arenaAfter;
    uint32_t mapStateBefore;
    uint32_t mapStateAfter;
    uint32_t scriptInitialFNV;
    uint32_t scriptMutatedFNV;
    uint32_t scriptAfterFNV;
    uint32_t buildStarted;
    uint32_t buildElapsed;
    uint32_t filterStarted;
    uint32_t filterElapsed;
    uint32_t heapUsed;
    uint32_t allocatorOverhead;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }
    if (!Esp32Map1EventDescriptorProbe_isDone()) {
        return;
    }
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPFILTERPROBE] ARMED descriptor/linkage proven; native script state + side-effect-free Game_runEvent filtering starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO script state + event filtering ===\n");
    printf("[MAPFILTERPROBE] CONTRACT 4-bit current event states + removed-command bitset + exact Game_runEvent pre-execution filtering; no command execution, world mutation, entities, rendering or gameplay\n");

    if (!boundaryCommon(doomRpg) || EspMapScriptState_isReady()) {
        printf("[MAPFILTERPROBE] FAILED precondition heap8=%u largest8=%u scriptReady=%d\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               EspMapScriptState_isReady());
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    if (runtime == NULL || mapState == NULL ||
        runtime->arenaBytes != EXPECTED_ARENA_BYTES ||
        runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        runtime->eventCount != EXPECTED_EVENT_COUNT ||
        runtime->byteCodeCount != EXPECTED_BYTECODE_COUNT ||
        mapState->tileCount != EXPECTED_STATE_BYTES ||
        mapState->stateFNV1a != EXPECTED_STATE_FNV) {
        printf("[MAPFILTERPROBE] FAILED inherited regression\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = fnv1a32(mapState->tileFlags, mapState->tileCount);
    buildStarted = DoomRPG_GetUpTimeMS();

    if (!EspMapScriptState_buildFromRuntime()) {
        printf("[MAPFILTERPROBE] FAILED script-state build\n");
        return;
    }

    buildElapsed = DoomRPG_GetUpTimeMS() - buildStarted;
    heapAfterBuild = heap8Free();
    largestAfterBuild = largest8Block();
    heapUsed = heapBefore >= heapAfterBuild ? heapBefore - heapAfterBuild : 0U;
    allocatorOverhead = heapUsed >= EXPECTED_SCRIPT_BYTES ?
                        heapUsed - EXPECTED_SCRIPT_BYTES : UINT32_MAX;

    if (heapAfterBuild >= heapBefore || heapUsed < EXPECTED_SCRIPT_BYTES ||
        allocatorOverhead > MAX_SCRIPT_ALLOCATOR_OVERHEAD ||
        largestAfterBuild < MIN_LARGEST8_AFTER_SCRIPT ||
        !validateScriptState(&scriptInitialFNV, &scriptMutatedFNV)) {
        printf("[MAPFILTERPROBE] FAILED script-state validation heap8=%u->%u used=%u overhead=%u largest8=%u->%u\n",
               (unsigned int)heapBefore, (unsigned int)heapAfterBuild,
               (unsigned int)heapUsed, (unsigned int)allocatorOverhead,
               (unsigned int)largestBefore, (unsigned int)largestAfterBuild);
        EspMapScriptState_reset();
        return;
    }

    scriptState = EspMapScriptState_view();
    printf("[MAPSCRIPT] READY bytes=%u eventStateBytes=%u removedBytes=%u events=%u byteCodes=%u initialFNV=%08x buildElapsed=%ums\n",
           (unsigned int)scriptState->storageBytes,
           (unsigned int)scriptState->eventStateBytes,
           (unsigned int)scriptState->removedCommandBytes,
           (unsigned int)scriptState->eventCount,
           (unsigned int)scriptState->byteCodeCount,
           (unsigned int)scriptInitialFNV,
           (unsigned int)buildElapsed);

    filterStarted = DoomRPG_GetUpTimeMS();
    if (!validateFilterMatrix(&stats) || !validateResumeAndRemoved(&stats)) {
        printf("[MAPFILTERPROBE] FAILED filter validation\n");
        EspMapScriptState_reset();
        return;
    }
    filterElapsed = DoomRPG_GetUpTimeMS() - filterStarted;

    heapAfterFilter = heap8Free();
    largestAfterFilter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState != NULL ?
                    fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfterFNV = scriptState != NULL ?
                     fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;

    if (!boundaryCommon(doomRpg) || !EspMapScriptState_isReady() ||
        heapAfterFilter != heapAfterBuild ||
        largestAfterFilter != largestAfterBuild ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore || mapStateAfter != EXPECTED_STATE_FNV ||
        scriptAfterFNV != scriptInitialFNV) {
        printf("[MAPFILTERPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x\n",
               (unsigned int)heapAfterBuild, (unsigned int)heapAfterFilter,
               (unsigned int)largestAfterBuild, (unsigned int)largestAfterFilter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
               (unsigned int)scriptInitialFNV, (unsigned int)scriptAfterFNV);
        EspMapScriptState_reset();
        return;
    }

    probeState.done = 1;
    printf("[MAPFILTER] READY filterFNV=%08x resumeFNV=%08x elapsed=%ums contexts=%u evaluations=%u eligible=%u blocked=%u stateSkip=%u keySkip=%u flagsSkip=%u blockInputEvents=%u\n",
           (unsigned int)stats.filterFNV,
           (unsigned int)stats.resumeFNV,
           (unsigned int)filterElapsed,
           (unsigned int)stats.contexts,
           (unsigned int)stats.evaluations,
           (unsigned int)stats.eligible,
           (unsigned int)stats.eventBlocked,
           (unsigned int)stats.stateMismatch,
           (unsigned int)stats.keyMismatch,
           (unsigned int)stats.flagsMismatch,
           (unsigned int)stats.blockInputEvents);
    printf("[MAPFILTERPROBE] MUTATION reversible=yes scriptFNV=%08x->%08x->%08x removedFinal=0 statesRestored=yes\n",
           (unsigned int)scriptInitialFNV,
           (unsigned int)scriptMutatedFNV,
           (unsigned int)scriptAfterFNV);
    printf("[MAPFILTERPROBE] RAM heap8=%u->%u used=%u payload=%u allocatorOverhead=%u largest8=%u->%u filterDelta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfterBuild,
           (unsigned int)heapUsed,
           (unsigned int)EXPECTED_SCRIPT_BYTES,
           (unsigned int)allocatorOverhead,
           (unsigned int)largestBefore,
           (unsigned int)largestAfterBuild,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)arenaBefore,
           (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore,
           (unsigned int)mapStateAfter,
           (unsigned int)scriptAfterFNV);
    printf("[MAPFILTERPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes scriptBytes=%u filterReady=yes scriptExecution=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)EXPECTED_SCRIPT_BYTES,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1EventFilterProbe_isDone(void) {
    return probeState.done;
}
