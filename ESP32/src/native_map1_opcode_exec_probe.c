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
#include "esp_map_opcode_executor.h"
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
#include "native_map1_opcode_exec_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_BYTES 1024U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_BYTES 81U
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_BYTECODE_COUNT 265U
#define LEGACY_OPCODE_MIN 1U
#define LEGACY_OPCODE_MAX 42U
#define OPCODE_SEEN_BYTES 32U

typedef struct Esp32Map1OpcodeExecProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1OpcodeExecProbeState;

typedef struct OpcodeAudit_s {
    uint32_t auditFNV;
    uint32_t opcodeMaskLo;
    uint32_t opcodeMaskHi;
    uint32_t uniqueIds;
    uint32_t outOfRangeRefs;
    uint32_t stateRefs;
    uint32_t changeRefs;
    uint32_t nextRefs;
    uint32_t prevRefs;
    uint32_t candidateIndex;
    uint32_t candidateRank;
    uint32_t unsupportedIndex;
    EspMapByteCode candidate;
    EspMapByteCode unsupported;
    EspMapEventRef candidateTarget;
    uint8_t candidateFound;
    uint8_t unsupportedFound;
} OpcodeAudit;

static Esp32Map1OpcodeExecProbeState probeState;

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
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32IntroDispose_isDone() &&
           Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() &&
           Esp32Map1AccessProbe_isDone() &&
           Esp32Map1StateProbe_isDone() &&
           Esp32Map1EventsProbe_isDone() &&
           Esp32Map1EventDescriptorProbe_isDone() &&
           Esp32Map1EventFilterProbe_isDone() &&
           runtime != NULL && mapState != NULL && scriptState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           !Esp32IntroClock_isActive() && !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static uint32_t stateOpcodeRank(uint8_t codeId) {
    if (codeId == ESP_MAP_OPCODE_NEXT_STATE) {
        return 0U;
    }
    if (codeId == ESP_MAP_OPCODE_CHANGE_STATE) {
        return 1U;
    }
    if (codeId == ESP_MAP_OPCODE_PREV_STATE) {
        return 2U;
    }
    return UINT32_MAX;
}

static int commandTarget(const EspMapByteCode* command,
                         EspMapEventRef* outTarget) {
    uint32_t x;
    uint32_t y;
    uint32_t tile;

    if (command == NULL || outTarget == NULL) {
        return 0;
    }
    x = command->arg1 & 0xffU;
    y = (command->arg1 >> 8) & 0xffU;
    tile = x + y * 32U;
    if (tile >= ESP_MAP_EVENT_TILE_COUNT) {
        return 0;
    }
    return EspMapEvents_findByTile(tile, outTarget);
}

static int auditOpcodes(OpcodeAudit* outAudit) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint8_t seen[OPCODE_SEEN_BYTES];
    EspMapByteCode command;
    EspMapEventRef target;
    uint32_t hash = 2166136261U;
    uint32_t rank;
    uint32_t requestedState;
    uint32_t i;

    if (runtime == NULL || outAudit == NULL ||
        runtime->byteCodeCount != EXPECTED_BYTECODE_COUNT) {
        return 0;
    }

    memset(outAudit, 0, sizeof(*outAudit));
    memset(seen, 0, sizeof(seen));
    outAudit->candidateRank = UINT32_MAX;

    for (i = 0U; i < runtime->byteCodeCount; ++i) {
        if (!EspMapRuntime_getByteCode(i, &command)) {
            return 0;
        }

        hash = hashU16(hash, (uint16_t)i);
        hash = hashByte(hash, command.id);
        hash = hashU32(hash, command.arg1);
        hash = hashU32(hash, command.arg2);

        if ((seen[command.id >> 3] &
             (uint8_t)(1U << (command.id & 7U))) == 0U) {
            seen[command.id >> 3] |=
                (uint8_t)(1U << (command.id & 7U));
            ++outAudit->uniqueIds;
        }

        if (command.id < 32U) {
            outAudit->opcodeMaskLo |= 1UL << command.id;
        }
        else if (command.id < 64U) {
            outAudit->opcodeMaskHi |= 1UL << (command.id - 32U);
        }

        if (command.id < LEGACY_OPCODE_MIN || command.id > LEGACY_OPCODE_MAX) {
            ++outAudit->outOfRangeRefs;
        }

        if (!EspMapOpcodeExecutor_supports(command.id)) {
            if (!outAudit->unsupportedFound) {
                outAudit->unsupportedFound = 1U;
                outAudit->unsupportedIndex = i;
                outAudit->unsupported = command;
            }
            continue;
        }

        ++outAudit->stateRefs;
        if (command.id == ESP_MAP_OPCODE_CHANGE_STATE) {
            ++outAudit->changeRefs;
        }
        else if (command.id == ESP_MAP_OPCODE_NEXT_STATE) {
            ++outAudit->nextRefs;
        }
        else if (command.id == ESP_MAP_OPCODE_PREV_STATE) {
            ++outAudit->prevRefs;
        }

        if (!commandTarget(&command, &target)) {
            continue;
        }
        if (command.id == ESP_MAP_OPCODE_CHANGE_STATE) {
            requestedState = (command.arg1 >> 16) & 0xffU;
            if (requestedState > 15U) {
                continue;
            }
        }

        rank = stateOpcodeRank(command.id);
        if (!outAudit->candidateFound || rank < outAudit->candidateRank) {
            outAudit->candidateFound = 1U;
            outAudit->candidateRank = rank;
            outAudit->candidateIndex = i;
            outAudit->candidate = command;
            outAudit->candidateTarget = target;
        }
    }

    outAudit->auditFNV = hash;
    return outAudit->outOfRangeRefs == 0U &&
           outAudit->candidateFound && outAudit->unsupportedFound;
}

static int validateFirstExecution(const OpcodeAudit* audit,
                                  uint32_t* outExecFNV,
                                  uint32_t* outPreparedFNV,
                                  uint32_t* outExecutedFNV,
                                  uint8_t* outPreparedState,
                                  EspMapOpcodeExecResult* outResult) {
    const EspMapScriptStateView* scriptState = EspMapScriptState_view();
    EspMapOpcodeExecResult result;
    EspMapOpcodeExecResult unsupportedResult;
    EspMapOpcodeExecResult invalidResult;
    EspMapByteCode invalidStateCommand;
    uint32_t initialFNV;
    uint32_t preparedFNV;
    uint32_t executedFNV;
    uint32_t restoredFNV;
    uint32_t stableFNV;
    uint32_t execHash = 2166136261U;
    uint32_t requestedState;
    uint32_t x;
    uint32_t y;
    uint8_t originalState;
    uint8_t preparedState;
    uint8_t expectedAfter;

    if (audit == NULL || outExecFNV == NULL || outPreparedFNV == NULL ||
        outExecutedFNV == NULL || outPreparedState == NULL || outResult == NULL ||
        scriptState == NULL || scriptState->storage == NULL ||
        scriptState->storageBytes != EXPECTED_SCRIPT_BYTES) {
        return 0;
    }

    initialFNV = fnv1a32(scriptState->storage, scriptState->storageBytes);
    if (initialFNV != EXPECTED_SCRIPT_FNV ||
        !EspMapScriptState_getEventState(audit->candidateTarget.index,
                                         &originalState)) {
        return 0;
    }

    if (audit->candidate.id == ESP_MAP_OPCODE_NEXT_STATE) {
        preparedState = 0U;
        expectedAfter = 1U;
    }
    else if (audit->candidate.id == ESP_MAP_OPCODE_PREV_STATE) {
        preparedState = 1U;
        expectedAfter = 0U;
    }
    else if (audit->candidate.id == ESP_MAP_OPCODE_CHANGE_STATE) {
        requestedState = (audit->candidate.arg1 >> 16) & 0xffU;
        if (requestedState > 15U) {
            return 0;
        }
        preparedState = (uint8_t)(requestedState == 0U ? 1U : 0U);
        expectedAfter = (uint8_t)requestedState;
    }
    else {
        return 0;
    }

    if (!EspMapScriptState_setEventState(audit->candidateTarget.index,
                                         preparedState)) {
        return 0;
    }
    preparedFNV = fnv1a32(scriptState->storage, scriptState->storageBytes);

    if (EspMapOpcodeExecutor_execute(&audit->candidate, &result) !=
            ESP_MAP_OPCODE_EXEC_OK ||
        result.status != ESP_MAP_OPCODE_EXEC_OK ||
        result.codeId != audit->candidate.id ||
        result.arg1 != audit->candidate.arg1 ||
        result.arg2 != audit->candidate.arg2 ||
        result.targetTile != audit->candidateTarget.tileIndex ||
        result.targetEventIndex != audit->candidateTarget.index ||
        result.stateBefore != preparedState ||
        result.stateAfter != expectedAfter || result.mutated != 1U) {
        EspMapScriptState_setEventState(audit->candidateTarget.index,
                                        originalState);
        return 0;
    }

    executedFNV = fnv1a32(scriptState->storage, scriptState->storageBytes);
    if (executedFNV == preparedFNV ||
        !EspMapScriptState_setEventState(audit->candidateTarget.index,
                                         originalState)) {
        return 0;
    }
    restoredFNV = fnv1a32(scriptState->storage, scriptState->storageBytes);
    if (restoredFNV != initialFNV || restoredFNV != EXPECTED_SCRIPT_FNV) {
        return 0;
    }

    stableFNV = restoredFNV;
    if (EspMapOpcodeExecutor_execute(&audit->unsupported,
                                     &unsupportedResult) !=
            ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
        unsupportedResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
        fnv1a32(scriptState->storage, scriptState->storageBytes) != stableFNV) {
        return 0;
    }

    x = audit->candidateTarget.tileIndex & 31U;
    y = audit->candidateTarget.tileIndex >> 5;
    invalidStateCommand.id = ESP_MAP_OPCODE_CHANGE_STATE;
    invalidStateCommand.arg1 = x | (y << 8) | (16UL << 16);
    invalidStateCommand.arg2 = 0U;
    if (EspMapOpcodeExecutor_execute(&invalidStateCommand, &invalidResult) !=
            ESP_MAP_OPCODE_EXEC_STATE_OUT_OF_RANGE ||
        invalidResult.status != ESP_MAP_OPCODE_EXEC_STATE_OUT_OF_RANGE ||
        fnv1a32(scriptState->storage, scriptState->storageBytes) != stableFNV ||
        EspMapOpcodeExecutor_execute(NULL, &invalidResult) !=
            ESP_MAP_OPCODE_EXEC_INVALID ||
        EspMapOpcodeExecutor_execute(&audit->candidate, NULL) !=
            ESP_MAP_OPCODE_EXEC_INVALID) {
        return 0;
    }

    execHash = hashU16(execHash, (uint16_t)audit->candidateIndex);
    execHash = hashByte(execHash, result.codeId);
    execHash = hashU32(execHash, result.arg1);
    execHash = hashU32(execHash, result.arg2);
    execHash = hashU16(execHash, result.targetTile);
    execHash = hashU16(execHash, result.targetEventIndex);
    execHash = hashByte(execHash, result.stateBefore);
    execHash = hashByte(execHash, result.stateAfter);
    execHash = hashByte(execHash, result.mutated);
    execHash = hashByte(execHash, result.status);

    *outExecFNV = execHash;
    *outPreparedFNV = preparedFNV;
    *outExecutedFNV = executedFNV;
    *outPreparedState = preparedState;
    *outResult = result;
    return 1;
}

void Esp32Map1OpcodeExecProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1OpcodeExecProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    OpcodeAudit audit;
    EspMapOpcodeExecResult execResult;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t arenaBefore;
    uint32_t arenaAfter;
    uint32_t mapStateBefore;
    uint32_t mapStateAfter;
    uint32_t scriptBefore;
    uint32_t scriptAfter;
    uint32_t execFNV;
    uint32_t preparedFNV;
    uint32_t executedFNV;
    uint32_t startedMs;
    uint32_t elapsedMs;
    uint8_t preparedState;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }
    if (!Esp32Map1EventFilterProbe_isDone()) {
        return;
    }
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPOPCODEPROBE] ARMED event filtering proven; opcode inventory + first reversible native execution starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO opcode audit + first execution ===\n");
    printf("[MAPOPCODEPROBE] CONTRACT audit all 265 real opcodes; execute only EV_CHANGESTATE/NEXTSTATE/PREVSTATE against 81B script overlay; unsupported fail closed; rollback before PARK\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPOPCODEPROBE] FAILED precondition heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block());
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL) {
        printf("[MAPOPCODEPROBE] FAILED inherited views\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = fnv1a32(mapState->tileFlags, mapState->tileCount);
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    startedMs = DoomRPG_GetUpTimeMS();

    if (arenaBefore != EXPECTED_ARENA_FNV ||
        mapStateBefore != EXPECTED_MAP_STATE_FNV ||
        scriptBefore != EXPECTED_SCRIPT_FNV ||
        !auditOpcodes(&audit)) {
        printf("[MAPOPCODEPROBE] FAILED opcode audit arena=%08x mapState=%08x script=%08x\n",
               (unsigned int)arenaBefore,
               (unsigned int)mapStateBefore,
               (unsigned int)scriptBefore);
        return;
    }

    if (!validateFirstExecution(&audit, &execFNV, &preparedFNV,
                                &executedFNV, &preparedState,
                                &execResult)) {
        printf("[MAPOPCODEPROBE] FAILED first execution validation candidate=%u id=%u\n",
               (unsigned int)audit.candidateIndex,
               (unsigned int)audit.candidate.id);
        return;
    }

    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ?
                 fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState != NULL ?
                    fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfter = scriptState != NULL ?
                  fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;

    if (!boundaryIsSafe(doomRpg) || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV) {
        printf("[MAPOPCODEPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
               (unsigned int)scriptBefore, (unsigned int)scriptAfter);
        return;
    }

    probeState.done = 1;
    printf("[MAPOPCODE] AUDIT refs=%u uniqueIds=%u idMaskLo=%08x idMaskHi=%08x outOfRange=%u stateRefs=%u change=%u next=%u prev=%u auditFNV=%08x\n",
           (unsigned int)EXPECTED_BYTECODE_COUNT,
           (unsigned int)audit.uniqueIds,
           (unsigned int)audit.opcodeMaskLo,
           (unsigned int)audit.opcodeMaskHi,
           (unsigned int)audit.outOfRangeRefs,
           (unsigned int)audit.stateRefs,
           (unsigned int)audit.changeRefs,
           (unsigned int)audit.nextRefs,
           (unsigned int)audit.prevRefs,
           (unsigned int)audit.auditFNV);
    printf("[MAPOPCODE] EXEC command=%u id=%u arg1=%08x arg2=%08x target=%u/%u prepared=%u state=%u->%u mutated=yes execFNV=%08x\n",
           (unsigned int)audit.candidateIndex,
           (unsigned int)audit.candidate.id,
           (unsigned int)audit.candidate.arg1,
           (unsigned int)audit.candidate.arg2,
           (unsigned int)execResult.targetTile,
           (unsigned int)execResult.targetEventIndex,
           (unsigned int)preparedState,
           (unsigned int)execResult.stateBefore,
           (unsigned int)execResult.stateAfter,
           (unsigned int)execFNV);
    printf("[MAPOPCODEPROBE] READY elapsed=%ums supportedRefs=%u unsupportedSample=%u/%u invalidState=refused rollback=yes scriptFNV=%08x->%08x->%08x->%08x\n",
           (unsigned int)elapsedMs,
           (unsigned int)audit.stateRefs,
           (unsigned int)audit.unsupportedIndex,
           (unsigned int)audit.unsupported.id,
           (unsigned int)scriptBefore,
           (unsigned int)preparedFNV,
           (unsigned int)executedFNV,
           (unsigned int)scriptAfter);
    printf("[MAPOPCODEPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter);
    printf("[MAPOPCODEPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes supportedOpcodes=3 worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1OpcodeExecProbe_isDone(void) {
    return probeState.done;
}
