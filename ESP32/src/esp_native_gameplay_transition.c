#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_change_map_state.h"
#include "esp_map_event_filter.h"
#include "esp_map_line_state.h"
#include "esp_map_events.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_strings.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_select.h"
#include "esp_native_gameplay_transition.h"
#include "esp_player_view_state.h"
#include "platform_touch_events.h"

/* action_engine.h deliberately renames the historical SELECT wrapper into the
 * private fallback leaf. This translation unit owns the one public linker
 * wrapper and therefore restores the real wrapper token here. */
#undef __wrap_EspNativeGameplayAction_executeSelect

#define TRANSITION_NAME_CAPACITY ESP_MAP_SAVE_ROUTE_NAME_CAPACITY
#define TRANSITION_MIN_ELIGIBLE 2U
#define TRANSITION_MAX_ELIGIBLE \
    (2U + ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS)

typedef struct EspNativeGameplayTransitionDoorScratch_s {
    EspMapLineDoorResult preview[ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS];
    EspMapLineDoorResult applied[ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS];
    uint8_t offsets[ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS];
    uint8_t removedBefore[ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS];
    uint8_t removedAfter[ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS];
} EspNativeGameplayTransitionDoorScratch;

static EspNativeGameplayTransitionState transitionState;
static EspNativeGameplayTransitionDoorScratch transitionDoorScratch;
static uint8_t junctionExitCensusDone;

void __real_EspNativeGameplayInput_reset(void);

static int descriptorForSelect(const EspNativeGameplaySelectResult* select,
                               EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t value;

    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (select == NULL || outDescriptor == NULL || select->eventFound == 0U ||
        select->eventIndex == ESP_NATIVE_GAMEPLAY_SELECT_NO_EVENT ||
        !EspMapRuntime_getEvent(select->eventIndex, &value)) {
        return 0;
    }

    ref.index = select->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    if (!EspMapEvents_describe(&ref, outDescriptor)) return 0;

    return outDescriptor->eventIndex == select->eventIndex &&
           outDescriptor->tileIndex == select->frontTile &&
           outDescriptor->firstCommandIndex == select->firstCommandIndex &&
           outDescriptor->commandEndIndex == select->commandEndIndex &&
           outDescriptor->commandCount == select->commandCount;
}

static int isTransitionDoorOpcode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_MOVELINE ||
           codeId == ESP_MAP_OPCODE_OPENLINE ||
           codeId == ESP_MAP_OPCODE_CLOSELINE ||
           codeId == ESP_MAP_OPCODE_MOVELINE2;
}

static int rollbackTransitionDoorPrefix(uint8_t count) {
    int i;
    int ok = 1;

    if (count > ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS) return 0;
    for (i = (int)count - 1; i >= 0; --i) {
        EspMapLineDoorResult* applied = &transitionDoorScratch.applied[i];
        const EspMapLineDoorResult* preview = &transitionDoorScratch.preview[i];
        if (transitionDoorScratch.removedAfter[i] !=
            transitionDoorScratch.removedBefore[i]) {
            if (!EspMapScriptState_setCommandRemoved(
                    preview->globalCommandIndex,
                    transitionDoorScratch.removedBefore[i])) {
                ok = 0;
            }
            transitionDoorScratch.removedAfter[i] =
                transitionDoorScratch.removedBefore[i];
        }
        if (applied->mutated != 0U &&
            !EspMapLineState_setOpen(preview->lineIndex,
                                     preview->openBefore)) {
            ok = 0;
        }
    }
    if (!EspNativeDoorAnimator_validateLineState()) ok = 0;
    return ok;
}

static EspNativeGameplayTransitionStatus findTransitionCommands(
    const EspNativeGameplaySelectResult* select,
    const EspMapEventDescriptor* descriptor,
    uint8_t* outSaveOffset,
    uint8_t* outChangeOffset,
    uint8_t* outDoorOffset,
    uint8_t* outDoorOffsets,
    uint8_t* outDoorCount,
    uint8_t* outEligibleCount) {
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    uint32_t offset;
    uint8_t eligible = 0U;
    uint8_t saveOffset = 0U;
    uint8_t changeOffset = 0U;
    uint8_t doorOffset = 0U;
    uint8_t doorCount = 0U;

    if (outSaveOffset != NULL) *outSaveOffset = 0U;
    if (outChangeOffset != NULL) *outChangeOffset = 0U;
    if (outDoorOffset != NULL) *outDoorOffset = 0U;
    if (outDoorOffsets != NULL) {
        memset(outDoorOffsets, 0,
               ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS);
    }
    if (outDoorCount != NULL) *outDoorCount = 0U;
    if (outEligibleCount != NULL) *outEligibleCount = 0U;
    if (select == NULL || descriptor == NULL || outSaveOffset == NULL ||
        outChangeOffset == NULL || outDoorOffset == NULL ||
        outDoorOffsets == NULL || outDoorCount == NULL ||
        outEligibleCount == NULL || !EspMapScriptState_isReady() ||
        !EspMapEventFilter_prepare(descriptor, select->currentState, 0U,
                                   ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS,
                                   0U, &plan)) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
    }

    for (offset = 0U; offset < descriptor->commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor->firstCommandIndex + offset;
        uint8_t removed;

        if (global > UINT16_MAX || offset > UINT8_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(descriptor, &plan, offset, removed,
                                        &filtered)) {
            return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;

        ++eligible;
        if (eligible == 1U) {
            if (filtered.codeId != ESP_MAP_OPCODE_SAVEGAME) {
                return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE;
            }
            saveOffset = (uint8_t)offset;
        }
        else if (eligible == 2U) {
            if (filtered.codeId != ESP_MAP_OPCODE_CHANGE_MAP) {
                *outEligibleCount = eligible;
                return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
            }
            changeOffset = (uint8_t)offset;
        }
        else {
            if (!isTransitionDoorOpcode(filtered.codeId) ||
                doorCount >= ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS) {
                *outEligibleCount = eligible;
                return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
            }
            if (doorCount == 0U) doorOffset = (uint8_t)offset;
            outDoorOffsets[doorCount++] = (uint8_t)offset;
        }

        if (eligible > TRANSITION_MAX_ELIGIBLE) {
            *outEligibleCount = eligible;
            return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
        }
    }

    *outEligibleCount = eligible;
    if (eligible == 0U) return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE;
    if (eligible < TRANSITION_MIN_ELIGIBLE ||
        eligible > TRANSITION_MAX_ELIGIBLE) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
    }

    *outSaveOffset = saveOffset;
    *outChangeOffset = changeOffset;
    *outDoorOffset = doorOffset;
    *outDoorCount = doorCount;
    return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY;
}

void EspNativeGameplayTransition_reset(void) {
    memset(&transitionState, 0, sizeof(transitionState));
    junctionExitCensusDone = 0U;
}

int EspNativeGameplayTransition_isWaitingDoor(void) {
    return transitionState.active == 1U &&
           transitionState.waitingDoor == 1U &&
           transitionState.waitingStats == 0U &&
           transitionState.committed.phase ==
               ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS;
}

int EspNativeGameplayTransition_isWaitingStats(void) {
    return transitionState.active == 1U &&
           transitionState.waitingDoor == 0U &&
           transitionState.waitingStats == 1U &&
           transitionState.committed.phase ==
               ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS;
}

const EspNativeGameplayTransitionState* EspNativeGameplayTransition_view(void) {
    return transitionState.active != 0U ? &transitionState : NULL;
}

int EspNativeGameplayTransition_probeJunctionExitCensus(void) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const char* sourceName;
    EspAssetPackEntry sourceEntry;
    uint32_t scriptBefore;
    uint32_t scriptAfter;
    uint32_t candidateEvents = 0U;
    uint32_t saveCommands = 0U;
    uint32_t changeCommands = 0U;
    uint32_t targetMask = 0U;
    uint32_t eventIndex;
    int ok = 1;

    if (junctionExitCensusDone != 0U) return 1;
    if (view == NULL || view->targetMapId != ESP_MAP_ID_JUNCTION) return 1;
    if (runtime == NULL || runtime->eventCount == 0U ||
        runtime->byteCodeCount == 0U || !EspMapScriptState_isReady() ||
        EspAssetPack_isOpen()) {
        printf("[JUNCTIONEXITCENSUS] DEFER map=%u runtime=%u script=%u packOpen=%u\n",
               view != NULL ? (unsigned int)view->targetMapId : 0U,
               runtime != NULL ? 1U : 0U,
               (unsigned int)EspMapScriptState_isReady(),
               (unsigned int)EspAssetPack_isOpen());
        return 0;
    }

    sourceName = EspMapCatalog_nameForId(view->targetMapId);
    if (sourceName == NULL || sourceName[0] == '\0') return 0;

    memset(&sourceEntry, 0, sizeof(sourceEntry));
    scriptBefore = EspMapScriptState_fingerprint();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[JUNCTIONEXITCENSUS] FAILED stage=PACK_OPEN mutation=no\n");
        return 0;
    }
    if (!EspAssetPack_findEntry(sourceName, &sourceEntry) ||
        (sourceEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
        EspAssetPack_close();
        printf("[JUNCTIONEXITCENSUS] FAILED stage=SOURCE_ENTRY source=%s mutation=no\n",
               sourceName);
        return 0;
    }

    printf("[JUNCTIONEXITCENSUS] BEGIN map=%u source=%s arena=%08x events=%u commands=%u mode=raw-save+changemap-candidates mutation=no allocation=no-explicit\n",
           (unsigned int)view->targetMapId,
           sourceName,
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)runtime->eventCount,
           (unsigned int)runtime->byteCodeCount);

    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        uint32_t rawEvent;
        uint32_t offset;
        uint8_t currentState = 0U;
        uint8_t hasSave = 0U;
        uint8_t hasChange = 0U;

        if (!EspMapRuntime_getEvent(eventIndex, &rawEvent)) {
            ok = 0;
            break;
        }
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(rawEvent & ESP_MAP_EVENT_TILE_MASK);
        ref.value = rawEvent;
        if (!EspMapEvents_describe(&ref, &descriptor) ||
            !EspMapScriptState_getEventState(eventIndex, &currentState)) {
            ok = 0;
            break;
        }

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) {
                ok = 0;
                break;
            }
            if (command.id == ESP_MAP_OPCODE_SAVEGAME) hasSave = 1U;
            if (command.id == ESP_MAP_OPCODE_CHANGE_MAP) hasChange = 1U;
        }
        if (!ok) break;
        if (hasSave == 0U && hasChange == 0U) continue;

        ++candidateEvents;
        printf("[JUNCTIONEXITCENSUS] EVENT index=%u tile=%u initialState=%u currentState=%u flags=%u first=%u count=%u hasSave=%u hasChange=%u\n",
               (unsigned int)descriptor.eventIndex,
               (unsigned int)descriptor.tileIndex,
               (unsigned int)descriptor.initialState,
               (unsigned int)currentState,
               (unsigned int)descriptor.flags,
               (unsigned int)descriptor.firstCommandIndex,
               (unsigned int)descriptor.commandCount,
               (unsigned int)hasSave,
               (unsigned int)hasChange);

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            uint32_t globalCommand;
            uint8_t removed = 0U;
            char mapName[TRANSITION_NAME_CAPACITY];
            uint8_t targetMapId = 0U;
            const char* mapLabel = "-";

            memset(mapName, 0, sizeof(mapName));
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) {
                ok = 0;
                break;
            }
            globalCommand = (uint32_t)descriptor.firstCommandIndex + offset;
            if (!EspMapScriptState_isCommandRemoved(globalCommand, &removed)) {
                ok = 0;
                break;
            }

            if (command.id == ESP_MAP_OPCODE_SAVEGAME ||
                command.id == ESP_MAP_OPCODE_CHANGE_MAP) {
                EspMapStringRef mapRef;
                size_t mapLength = 0U;
                uint32_t stringIndex = command.arg1 & 0xffU;

                if (!EspMapStrings_getRef(stringIndex, &mapRef) ||
                    mapRef.length >= sizeof(mapName) ||
                    EspMapStrings_read(&sourceEntry, &mapRef,
                                       mapName, sizeof(mapName),
                                       &mapLength) != ESP_MAP_STRING_READ_OK ||
                    mapLength != mapRef.length) {
                    ok = 0;
                    break;
                }
                mapLabel = mapName;
                if (EspMapCatalog_idForName(mapName, &targetMapId) &&
                    targetMapId < 32U) {
                    targetMask |= 1UL << targetMapId;
                }
            }

            if (command.id == ESP_MAP_OPCODE_SAVEGAME) {
                uint32_t packed = command.arg1 >> 8U;
                uint8_t rawX = (uint8_t)(packed & 0xffU);
                uint8_t rawY = (uint8_t)((packed >> 8U) & 0xffU);
                uint8_t angle = (uint8_t)((packed >> 16U) & 0xffU);
                ++saveCommands;
                printf("[JUNCTIONEXITCENSUS] CMD event=%u off=%u global=%u id=%u/SAVEGAME arg1=%08x arg2=%08x removed=%u map=%s targetMap=%u raw=%u,%u angle=%u dest=%u,%u\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)offset,
                       (unsigned int)globalCommand,
                       (unsigned int)command.id,
                       (unsigned int)command.arg1,
                       (unsigned int)command.arg2,
                       (unsigned int)removed,
                       mapLabel,
                       (unsigned int)targetMapId,
                       (unsigned int)rawX,
                       (unsigned int)rawY,
                       (unsigned int)angle,
                       (unsigned int)(32U + ((uint32_t)rawX << 6U)),
                       (unsigned int)(32U + ((uint32_t)rawY << 6U)));
            }
            else if (command.id == ESP_MAP_OPCODE_CHANGE_MAP) {
                uint8_t showStats =
                    (uint8_t)((command.arg1 &
                               ESP_MAP_CHANGE_MAP_SHOW_STATS_BIT) != 0U);
                uint32_t spawnParam = (command.arg1 << 1U) >> 9U;
                ++changeCommands;
                printf("[JUNCTIONEXITCENSUS] CMD event=%u off=%u global=%u id=%u/CHANGEMAP arg1=%08x arg2=%08x removed=%u map=%s targetMap=%u showStats=%u spawnParam=%u\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)offset,
                       (unsigned int)globalCommand,
                       (unsigned int)command.id,
                       (unsigned int)command.arg1,
                       (unsigned int)command.arg2,
                       (unsigned int)removed,
                       mapLabel,
                       (unsigned int)targetMapId,
                       (unsigned int)showStats,
                       (unsigned int)spawnParam);
            }
            else {
                printf("[JUNCTIONEXITCENSUS] CMD event=%u off=%u global=%u id=%u arg1=%08x arg2=%08x removed=%u\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)offset,
                       (unsigned int)globalCommand,
                       (unsigned int)command.id,
                       (unsigned int)command.arg1,
                       (unsigned int)command.arg2,
                       (unsigned int)removed);
            }
        }
        if (!ok) break;
    }

    EspAssetPack_close();
    scriptAfter = EspMapScriptState_fingerprint();

    if (!ok || scriptBefore == 0U || scriptAfter != scriptBefore ||
        EspAssetPack_isOpen()) {
        printf("[JUNCTIONEXITCENSUS] FAILED stage=SCAN candidates=%u script=%08x->%08x packOpen=%u failClosed=yes\n",
               (unsigned int)candidateEvents,
               (unsigned int)scriptBefore,
               (unsigned int)scriptAfter,
               (unsigned int)EspAssetPack_isOpen());
        return 0;
    }

    junctionExitCensusDone = 1U;
    printf("[JUNCTIONEXITCENSUS] SUMMARY map=%u candidates=%u save=%u changemap=%u targetMask=%08x scriptFNV=%08x exact=yes mutation=no allocation=no-explicit\n",
           (unsigned int)view->targetMapId,
           (unsigned int)candidateEvents,
           (unsigned int)saveCommands,
           (unsigned int)changeCommands,
           (unsigned int)targetMask,
           (unsigned int)scriptAfter);
    return 1;
}

EspNativeGameplayTransitionStatus EspNativeGameplayTransition_trySelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayTransitionSelectResult* outResult) {
    EspNativeGameplaySelectResult select;
    EspNativeGameplaySelectStatus selectStatus;
    EspMapEventDescriptor descriptor;
    EspMapSaveRouteState saveRoute;
    EspMapSaveRouteResult saveResult;
    EspMapChangeMapState pendingChange;
    EspMapChangeMapResult changeResult;
    EspMapTransitionPreflightResult preflight;
    EspMapLevelExitStats levelStats;
    EspStatsMenuIntent statsIntent;
    EspMapCommittedTransitionState committed;
    EspMapCommittedTransitionStatus committedStatus;
    EspNativeGameplayTransitionState next;
    EspAssetPackEntry sourceEntry;
    EspMapStringRef changeNameRef;
    EspMapStringReadStatus stringStatus;
    const EspPlayerViewState* view;
    const char* sourceName;
    char changeName[TRANSITION_NAME_CAPACITY];
    size_t changeNameLength = 0U;
    uint8_t saveOffset = 0U;
    uint8_t changeOffset = 0U;
    uint8_t doorOffset = 0U;
    uint8_t doorCount = 0U;
    uint8_t eligibleCount = 0U;
    uint8_t targetMapId = 0U;
    uint8_t i;
    EspNativeGameplayTransitionStatus matchStatus;
    EspMapSaveRouteStatus saveStatus;
    EspMapChangeMapStatus changeStatus;
    EspMapTransitionPreflightStatus preflightStatus;
    EspMapLevelExitStatsStatus statsStatus;
    EspStatsMenuIntentStatus menuStatus;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (intent == NULL || outResult == NULL ||
        intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->pending != 1U || intent->active == 0U) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
    }
    if (transitionState.active != 0U) {
        return (EspNativeGameplayTransition_isWaitingStats() ||
                transitionState.directReady != 0U)
                   ? ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY
                   : ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
    }

    memset(&select, 0, sizeof(select));
    selectStatus = EspNativeGameplaySelect_resolve(intent, &select);
    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT ||
        selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE;
    }
    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY;
    }
    if (selectStatus != ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT ||
        !descriptorForSelect(&select, &descriptor)) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
    }

    memset(&transitionDoorScratch, 0, sizeof(transitionDoorScratch));
    matchStatus = findTransitionCommands(
        &select, &descriptor, &saveOffset, &changeOffset, &doorOffset,
        transitionDoorScratch.offsets, &doorCount, &eligibleCount);
    outResult->sequence = intent->sequence;
    outResult->frontTile = select.frontTile;
    outResult->eventIndex = select.eventIndex;
    outResult->eligibleCount = eligibleCount;
    outResult->saveCommandOffset = saveOffset;
    outResult->changeCommandOffset = changeOffset;
    outResult->doorCommandOffset = doorOffset;
    outResult->doorCount = doorCount;
    outResult->doorReady = doorCount != 0U ? 1U : 0U;
    if (matchStatus != ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY) {
        return matchStatus;
    }

    view = EspPlayerView_view();
    if (view == NULL || view->active != 1U ||
        !EspMapCatalog_isValidId(view->targetMapId) ||
        !EspMapResidentLifecycle_isReady() || !EspMapRuntime_isLoaded() ||
        EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY;
    }
    sourceName = EspMapCatalog_nameForId(view->targetMapId);
    if (sourceName == NULL || sourceName[0] == '\0') {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
    }

    if (doorCount != 0U && EspNativeDoorAnimator_hasPendingFrames()) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
    }
    for (i = 0U; i < doorCount; ++i) {
        EspMapLineDoorStatus doorStatus;
        uint8_t j;
        doorStatus = EspMapLineState_previewDoorCommand(
            &descriptor, transitionDoorScratch.offsets[i],
            &transitionDoorScratch.preview[i]);
        if (doorStatus != ESP_MAP_LINE_DOOR_OK ||
            transitionDoorScratch.preview[i].mutated != 0U ||
            !isTransitionDoorOpcode(
                transitionDoorScratch.preview[i].codeId) ||
            !EspMapScriptState_isCommandRemoved(
                transitionDoorScratch.preview[i].globalCommandIndex,
                &transitionDoorScratch.removedBefore[i])) {
            return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
        }
        for (j = 0U; j < i; ++j) {
            if (transitionDoorScratch.preview[j].lineIndex ==
                transitionDoorScratch.preview[i].lineIndex) {
                return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
            }
        }
        transitionDoorScratch.removedAfter[i] =
            transitionDoorScratch.removedBefore[i];
    }

    memset(&saveRoute, 0, sizeof(saveRoute));
    memset(&saveResult, 0, sizeof(saveResult));
    memset(&pendingChange, 0, sizeof(pendingChange));
    memset(&changeResult, 0, sizeof(changeResult));
    memset(&preflight, 0, sizeof(preflight));
    memset(&levelStats, 0, sizeof(levelStats));
    memset(&statsIntent, 0, sizeof(statsIntent));
    memset(&committed, 0, sizeof(committed));
    memset(&sourceEntry, 0, sizeof(sourceEntry));
    memset(&changeNameRef, 0, sizeof(changeNameRef));
    memset(changeName, 0, sizeof(changeName));

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }
    if (!EspAssetPack_findEntry(sourceName, &sourceEntry) ||
        (sourceEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
        EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    saveStatus = EspMapSaveRoute_apply(&sourceEntry, &saveRoute, &descriptor,
                                       saveOffset, &saveResult);
    changeStatus = EspMapChangeMap_apply(&pendingChange, &descriptor,
                                         changeOffset, &changeResult);
    if (saveStatus != ESP_MAP_SAVE_ROUTE_OK ||
        changeStatus != ESP_MAP_CHANGE_MAP_OK ||
        !EspMapChangeMap_isActive(&pendingChange) ||
        changeResult.pending != 1U ||
        !EspMapStrings_getRef(changeResult.mapStringIndex, &changeNameRef)) {
        EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    stringStatus = EspMapStrings_read(&sourceEntry, &changeNameRef,
                                      changeName, sizeof(changeName),
                                      &changeNameLength);
    EspAssetPack_close();
    /*
     * Legacy EV_SAVEGAME and EV_CHANGEMAP carry independent map strings.
     * SAVEGAME prepares the future save/return route (newMapName/newDest*),
     * while CHANGEMAP selects the map loaded now. Entrance -> Junction happened
     * to use the same resource for both, but hub -> sector transitions do not.
     */
    if (stringStatus != ESP_MAP_STRING_READ_OK ||
        changeNameLength >= sizeof(changeName) ||
        !EspMapCatalog_idForName(changeName, &targetMapId) ||
        !EspMapCatalog_isValidId(targetMapId) ||
        targetMapId == view->targetMapId) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    preflightStatus = EspMapTransitionPreflight_run(targetMapId, &preflight);
    if (preflightStatus != ESP_MAP_TRANSITION_PREFLIGHT_OK ||
        preflight.ready != 1U || preflight.targetMapId != targetMapId ||
        EspAssetPack_isOpen()) {
        if (EspAssetPack_isOpen()) EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    statsStatus = EspMapLevelExitStats_collect(
        view->gameplayLoadMapId, changeResult.showStats, &levelStats);
    menuStatus = EspStatsMenuIntent_prepare(
        targetMapId, changeResult.showStats, &statsIntent);
    if (statsStatus != ESP_MAP_LEVEL_EXIT_STATS_OK ||
        (changeResult.showStats != 0U
             ? (menuStatus != ESP_STATS_MENU_INTENT_OK ||
                statsIntent.active != 1U ||
                statsIntent.consumePending != 1U)
             : (menuStatus != ESP_STATS_MENU_INTENT_NOT_APPLICABLE ||
                statsIntent.active != 0U ||
                statsIntent.consumePending != 0U ||
                statsIntent.menuKind != ESP_STATS_MENU_KIND_NONE))) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    committedStatus = EspMapCommittedTransition_begin(
        &committed, view->targetMapId, &pendingChange, &changeResult,
        &statsIntent, &preflight);
    if ((changeResult.showStats != 0U
             ? committedStatus != ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS ||
                   committed.phase !=
                       ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS
             : committedStatus != ESP_MAP_COMMITTED_TRANSITION_READY ||
                   committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_READY) ||
        committed.pendingConsumed != 1U || committed.committed != 0U ||
        EspMapChangeMap_isActive(&pendingChange) || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_isReady()) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    for (i = 0U; i < doorCount; ++i) {
        EspMapLineDoorStatus doorStatus =
            EspMapLineState_applyDoorCommand(
                &descriptor, transitionDoorScratch.offsets[i],
                &transitionDoorScratch.applied[i]);
        const EspMapLineDoorResult* preview =
            &transitionDoorScratch.preview[i];
        EspMapLineDoorResult* applied = &transitionDoorScratch.applied[i];

        if (doorStatus != ESP_MAP_LINE_DOOR_OK ||
            applied->mutated != 1U ||
            applied->codeId != preview->codeId ||
            applied->lineIndex != preview->lineIndex ||
            applied->globalCommandIndex != preview->globalCommandIndex ||
            applied->openBefore != preview->openBefore ||
            applied->openAfter != preview->openAfter) {
            if (!rollbackTransitionDoorPrefix((uint8_t)(i + 1U))) {
                return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
            }
            return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
        }

        if (applied->removeCommandIfHandled != 0U &&
            transitionDoorScratch.removedBefore[i] == 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    applied->globalCommandIndex, 1U)) {
                (void)rollbackTransitionDoorPrefix((uint8_t)(i + 1U));
                return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
            }
            transitionDoorScratch.removedAfter[i] = 1U;
        }
    }

    memset(&next, 0, sizeof(next));
    next.saveRoute = saveRoute;
    next.changeResult = changeResult;
    next.targetPreflight = preflight;
    next.levelStats = levelStats;
    next.statsIntent = statsIntent;
    next.committed = committed;
    next.sequence = intent->sequence;
    next.frontTile = select.frontTile;
    next.eventIndex = select.eventIndex;
    next.saveCommandOffset = saveOffset;
    next.changeCommandOffset = changeOffset;
    next.doorCommandOffset = doorOffset;
    next.doorCount = doorCount;
    for (i = 0U; i < doorCount; ++i) {
        next.doorResults[i] = transitionDoorScratch.applied[i];
        next.doorRemovedBeforeAll[i] =
            transitionDoorScratch.removedBefore[i];
        next.doorRemovedAfterAll[i] =
            transitionDoorScratch.removedAfter[i];
    }
    if (doorCount != 0U) {
        next.doorResult = next.doorResults[0];
        next.doorRemovedBefore = next.doorRemovedBeforeAll[0];
        next.doorRemovedAfter = next.doorRemovedAfterAll[0];
    }
    next.waitingDoor = doorCount != 0U ? 1U : 0U;
    next.active = 1U;
    next.waitingStats =
        (doorCount == 0U && changeResult.showStats != 0U) ? 1U : 0U;
    next.directReady =
        (doorCount == 0U && changeResult.showStats == 0U) ? 1U : 0U;
    transitionState = next;

    outResult->targetMapId = targetMapId;
    outResult->targetGameplayLoadMapId = preflight.gameplayLoadMapId;
    outResult->showStats = changeResult.showStats;
    outResult->committedPhase = committed.phase;

    printf("[NATIVECHANGEMAP] SAVE seq=%u event=%u tile=%u cmd=%u map=%s pos=%u,%u angle=%u remove=%u\n",
           (unsigned int)intent->sequence,
           (unsigned int)select.eventIndex,
           (unsigned int)select.frontTile,
           (unsigned int)saveOffset,
           saveRoute.mapName,
           (unsigned int)saveRoute.destinationX,
           (unsigned int)saveRoute.destinationY,
           (unsigned int)saveRoute.angle,
           (unsigned int)saveResult.removeCommandIfHandled);
    printf("[NATIVECHANGEMAP] CHANGE cmd=%u targetMap=%u gameplayLoadMapId=%u showStats=%u spawnParam=%u sourceBytes=%u crc=%08x fnv=%08x remove=%u\n",
           (unsigned int)changeOffset,
           (unsigned int)targetMapId,
           (unsigned int)preflight.gameplayLoadMapId,
           (unsigned int)changeResult.showStats,
           (unsigned int)changeResult.spawnParam,
           (unsigned int)preflight.sourceBytes,
           (unsigned int)preflight.sourceCrc32,
           (unsigned int)preflight.sourceFNV1a,
           (unsigned int)changeResult.removeCommandIfHandled);
    printf("[NATIVECHANGEMAP] STATS sourceMap=%u sourceGameplayLoad=%u showStats=%u secrets=%u/%u monsters=%u/%u completionBit=%08x effects=%02x playerExitApply=deferred reason=authoritative-turn-counter-not-owned\n",
           (unsigned int)view->targetMapId,
           (unsigned int)view->gameplayLoadMapId,
           (unsigned int)changeResult.showStats,
           (unsigned int)levelStats.secretsFound,
           (unsigned int)levelStats.secretsTotal,
           (unsigned int)levelStats.monstersDead,
           (unsigned int)levelStats.monstersTotal,
           (unsigned int)levelStats.completionLevelBit,
           (unsigned int)levelStats.effectFlags);

    if (doorCount != 0U) {
        const EspMapLineDoorResult* first = &next.doorResults[0];
        printf("[NATIVECHANGEMAP] WAIT_DOOR phase=%u menuKind=%u doors=%u firstCmd=%u firstLine=%u open=%u->%u removed=%u->%u animation=bounded-4frame sourceResident=%u packOpen=%u statsAck=no direct=%u\n",
               (unsigned int)committed.phase,
               (unsigned int)statsIntent.menuKind,
               (unsigned int)doorCount,
               (unsigned int)first->sourceCommandOffset,
               (unsigned int)first->lineIndex,
               (unsigned int)first->openBefore,
               (unsigned int)first->openAfter,
               (unsigned int)next.doorRemovedBeforeAll[0],
               (unsigned int)next.doorRemovedAfterAll[0],
               (unsigned int)EspMapResidentLifecycle_isReady(),
               (unsigned int)EspAssetPack_isOpen(),
               (unsigned int)(changeResult.showStats == 0U));
        return ESP_NATIVE_GAMEPLAY_TRANSITION_DOOR_READY;
    }

    if (changeResult.showStats != 0U) {
        printf("[NATIVECHANGEMAP] WAIT_STATS phase=%u menuKind=%u pendingConsumed=%u sourceResident=%u packOpen=%u destructiveHandoff=no statsAck=no\n",
               (unsigned int)committed.phase,
               (unsigned int)statsIntent.menuKind,
               (unsigned int)committed.pendingConsumed,
               (unsigned int)EspMapResidentLifecycle_isReady(),
               (unsigned int)EspAssetPack_isOpen());
        return ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS;
    }

    printf("[NATIVECHANGEMAP] HANDOFF-READY phase=%u targetMap=%u pendingConsumed=%u sourceResident=%u packOpen=%u showStats=0 service=next-tick\n",
           (unsigned int)committed.phase,
           (unsigned int)targetMapId,
           (unsigned int)committed.pendingConsumed,
           (unsigned int)EspMapResidentLifecycle_isReady(),
           (unsigned int)EspAssetPack_isOpen());
    return ESP_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_READY;
}

EspNativeGameplayTransitionStatus EspNativeGameplayTransition_finishDoor(
    uint32_t sequence,
    uint16_t eventIndex,
    uint16_t lineIndex) {
    uint8_t i;

    if (!EspNativeGameplayTransition_isWaitingDoor() ||
        transitionState.sequence != sequence ||
        transitionState.eventIndex != eventIndex ||
        transitionState.doorCount == 0U ||
        transitionState.doorCount > ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS ||
        transitionState.doorResults[0].lineIndex != lineIndex ||
        EspNativeDoorAnimator_hasPendingFrames()) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    for (i = 0U; i < transitionState.doorCount; ++i) {
        uint8_t openNow;
        uint8_t removedNow;
        const EspMapLineDoorResult* door = &transitionState.doorResults[i];
        if (door->mutated != 1U ||
            !isTransitionDoorOpcode(door->codeId) ||
            !EspMapLineState_getOpen(door->lineIndex, &openNow) ||
            !EspMapScriptState_isCommandRemoved(
                door->globalCommandIndex, &removedNow) ||
            openNow != door->openAfter ||
            removedNow != transitionState.doorRemovedAfterAll[i]) {
            return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
        }
    }

    transitionState.waitingDoor = 0U;
    if (transitionState.changeResult.showStats != 0U) {
        transitionState.waitingStats = 1U;
        transitionState.directReady = 0U;
        printf("[NATIVECHANGEMAP] DOOR-COMPLETE seq=%u event=%u doors=%u firstLine=%u phase=WAIT_STATS sourceResident=%u statsPresentation=native-pending-arm\n",
               (unsigned int)sequence,
               (unsigned int)eventIndex,
               (unsigned int)transitionState.doorCount,
               (unsigned int)lineIndex,
               (unsigned int)EspMapResidentLifecycle_isReady());
        PlatformInput_setTapCallback(NULL);
        return ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS;
    }

    transitionState.waitingStats = 0U;
    transitionState.directReady = 1U;
    printf("[NATIVECHANGEMAP] DOOR-COMPLETE seq=%u event=%u doors=%u firstLine=%u phase=HANDOFF_READY sourceResident=%u showStats=0 service=next-tick\n",
           (unsigned int)sequence,
           (unsigned int)eventIndex,
           (unsigned int)transitionState.doorCount,
           (unsigned int)lineIndex,
           (unsigned int)EspMapResidentLifecycle_isReady());
    return ESP_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_READY;
}

int EspNativeGameplayTransition_abortDoor(
    uint32_t sequence,
    uint16_t eventIndex,
    uint16_t lineIndex) {
    if (!EspNativeGameplayTransition_isWaitingDoor() ||
        transitionState.sequence != sequence ||
        transitionState.eventIndex != eventIndex ||
        transitionState.doorCount == 0U ||
        transitionState.doorResults[0].lineIndex != lineIndex) {
        return 0;
    }
    memset(&transitionState, 0, sizeof(transitionState));
    return 1;
}

const char* EspNativeGameplayTransition_statusName(
    EspNativeGameplayTransitionStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE: return "NOT_APPLICABLE";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX: return "COMPLEX";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_UNSUPPORTED: return "UNSUPPORTED";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED: return "FAILED";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS: return "WAIT_STATS";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_DOOR_READY: return "DOOR_READY";
    case ESP_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_READY: return "HANDOFF_READY";
    default: return "UNKNOWN";
    }
}

/* Reset chaining keeps the transition owner scoped to the current native
 * gameplay input/session lifetime without modifying the large resident service. */
void __wrap_EspNativeGameplayInput_reset(void) {
    EspNativeGameplayTransition_reset();
    __real_EspNativeGameplayInput_reset();
}

/* Public SELECT wrapper: transition event first, then the already-proven
 * tile-event/action-engine chain. This preserves legacy event-before-entity
 * ordering while keeping non-transition SELECT behavior byte-for-byte in the
 * existing private leaf. */
EspNativeGameplayActionStatus __wrap_EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayTransitionSelectResult transition;
    EspNativeGameplayTransitionStatus status;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;

    memset(&transition, 0, sizeof(transition));
    status = EspNativeGameplayTransition_trySelect(intent, &transition);
    if (status == ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_APPLICABLE) {
        return EspNativeGameplayActionEngine_executeSelect(intent, outResult);
    }

    if (status == ESP_NATIVE_GAMEPLAY_TRANSITION_DOOR_READY) {
        const EspNativeGameplayTransitionState* staged =
            EspNativeGameplayTransition_view();
        const EspMapLineDoorResult* first;
        uint8_t i;

        if (staged == NULL || !EspNativeGameplayTransition_isWaitingDoor() ||
            staged->doorCount == 0U ||
            staged->doorCount > ESP_NATIVE_GAMEPLAY_TRANSITION_MAX_DOORS) {
            return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
        }
        first = &staged->doorResults[0];
        memset(outResult, 0, sizeof(*outResult));
        outResult->sequence = transition.sequence;
        outResult->frontTile = transition.frontTile;
        outResult->eventIndex = transition.eventIndex;
        outResult->globalCommandIndex = first->globalCommandIndex;
        outResult->lineIndex = first->lineIndex;
        outResult->soundId = first->soundId;
        outResult->commandOffset = first->sourceCommandOffset;
        outResult->codeId = first->codeId;
        outResult->eligibleCount = transition.eligibleCount;
        outResult->openBefore = first->openBefore;
        outResult->openAfter = first->openAfter;
        outResult->locked = first->locked;
        outResult->handled = 1U;
        outResult->mutated = 1U;
        outResult->effectFlags = first->effectFlags;
        outResult->removedBefore = staged->doorRemovedBeforeAll[0];
        outResult->removedAfter = staged->doorRemovedAfterAll[0];
        outResult->removeIfHandled = first->removeCommandIfHandled;
        outResult->rollbackAvailable = 1U;
        outResult->doorCount = staged->doorCount;
        for (i = 0U; i < staged->doorCount; ++i) {
            const EspMapLineDoorResult* door = &staged->doorResults[i];
            outResult->doors[i].globalCommandIndex =
                door->globalCommandIndex;
            outResult->doors[i].lineIndex = door->lineIndex;
            outResult->doors[i].soundId = door->soundId;
            outResult->doors[i].commandOffset = door->sourceCommandOffset;
            outResult->doors[i].codeId = door->codeId;
            outResult->doors[i].openBefore = door->openBefore;
            outResult->doors[i].openAfter = door->openAfter;
            outResult->doors[i].locked = door->locked;
            outResult->doors[i].effectFlags = door->effectFlags;
            outResult->doors[i].removedBefore =
                staged->doorRemovedBeforeAll[i];
            outResult->doors[i].removedAfter =
                staged->doorRemovedAfterAll[i];
            outResult->doors[i].removeIfHandled =
                door->removeCommandIfHandled;
        }
        printf("[NATIVECHANGEMAP] DOOR-READY seq=%u event=%u doors=%u firstCmd=%u firstLine=%u open=%u->%u transitionHandled=yes render=shared-door-batch\n",
               (unsigned int)transition.sequence,
               (unsigned int)transition.eventIndex,
               (unsigned int)staged->doorCount,
               (unsigned int)first->sourceCommandOffset,
               (unsigned int)first->lineIndex,
               (unsigned int)first->openBefore,
               (unsigned int)first->openAfter);
        return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK;
    }

    if (status == ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS) {
        memset(outResult, 0, sizeof(*outResult));
        outResult->sequence = transition.sequence;
        outResult->frontTile = transition.frontTile;
        outResult->eventIndex = transition.eventIndex;
        outResult->commandOffset = transition.changeCommandOffset;
        outResult->codeId = ESP_MAP_OPCODE_CHANGE_MAP;
        outResult->eligibleCount = transition.eligibleCount;
        PlatformInput_setTapCallback(NULL);
        printf("[NATIVECHANGEMAP] INPUT-PAUSE seq=%u state=WAIT_STATS callback=NULL worldMutation=no sourceResident=yes\n",
               (unsigned int)transition.sequence);
        printf("[NATIVECHANGEMAP] COMPAT residentSelectStatus=UNSUPPORTED_EVENT reason=transition-ui-owns-wait-stats transitionHandled=yes\n");
        return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
    }

    if (status == ESP_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_READY) {
        memset(outResult, 0, sizeof(*outResult));
        outResult->sequence = transition.sequence;
        outResult->frontTile = transition.frontTile;
        outResult->eventIndex = transition.eventIndex;
        outResult->commandOffset = transition.changeCommandOffset;
        outResult->codeId = ESP_MAP_OPCODE_CHANGE_MAP;
        outResult->eligibleCount = transition.eligibleCount;
        printf("[NATIVECHANGEMAP] DIRECT-HANDOFF seq=%u targetMap=%u showStats=0 owner=staged service=next-tick\n",
               (unsigned int)transition.sequence,
               (unsigned int)transition.targetMapId);
        return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
    }

    if (status != ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY) {
        printf("[NATIVECHANGEMAP] DEFER seq=%u event=%u tile=%u status=%s eligible=%u mutation=no sourceResident=yes\n",
               intent != NULL ? (unsigned int)intent->sequence : 0U,
               (unsigned int)transition.eventIndex,
               (unsigned int)transition.frontTile,
               EspNativeGameplayTransition_statusName(status),
               (unsigned int)transition.eligibleCount);
    }
    return EspNativeGameplayActionEngine_executeSelect(intent, outResult);
}
