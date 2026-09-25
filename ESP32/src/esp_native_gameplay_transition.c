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
#define TRANSITION_MAX_ELIGIBLE 3U

static EspNativeGameplayTransitionState transitionState;

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

static EspNativeGameplayTransitionStatus findTransitionCommands(
    const EspNativeGameplaySelectResult* select,
    const EspMapEventDescriptor* descriptor,
    uint8_t* outSaveOffset,
    uint8_t* outChangeOffset,
    uint8_t* outDoorOffset,
    uint8_t* outDoorPresent,
    uint8_t* outEligibleCount) {
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    uint32_t offset;
    uint8_t eligible = 0U;
    uint8_t saveOffset = 0U;
    uint8_t changeOffset = 0U;
    uint8_t doorOffset = 0U;
    uint8_t doorPresent = 0U;

    if (outSaveOffset != NULL) *outSaveOffset = 0U;
    if (outChangeOffset != NULL) *outChangeOffset = 0U;
    if (outDoorOffset != NULL) *outDoorOffset = 0U;
    if (outDoorPresent != NULL) *outDoorPresent = 0U;
    if (outEligibleCount != NULL) *outEligibleCount = 0U;
    if (select == NULL || descriptor == NULL || outSaveOffset == NULL ||
        outChangeOffset == NULL || outDoorOffset == NULL ||
        outDoorPresent == NULL || outEligibleCount == NULL ||
        !EspMapScriptState_isReady() ||
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
        else if (eligible == 3U) {
            /*
             * The real Entrance exit event is SAVEGAME -> CHANGEMAP ->
             * OPENLINE. Legacy executes all three commands in event order;
             * Game_changeMap() is consumed only after the transition door has
             * opened. Own only that exact one-door suffix here.
             */
            if (filtered.codeId != ESP_MAP_OPCODE_OPENLINE) {
                *outEligibleCount = eligible;
                return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
            }
            doorOffset = (uint8_t)offset;
            doorPresent = 1U;
        }
        else {
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
    *outDoorPresent = doorPresent;
    return ESP_NATIVE_GAMEPLAY_TRANSITION_NOT_READY;
}

void EspNativeGameplayTransition_reset(void) {
    memset(&transitionState, 0, sizeof(transitionState));
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
    uint8_t doorPresent = 0U;
    uint8_t doorRemovedBefore = 0U;
    uint8_t doorRemovedAfter = 0U;
    uint8_t eligibleCount = 0U;
    EspMapLineDoorResult doorPreview;
    EspMapLineDoorResult doorApplied;
    EspMapLineDoorStatus doorStatus;
    uint8_t targetMapId = 0U;
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
        return EspNativeGameplayTransition_isWaitingStats()
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

    memset(&doorPreview, 0, sizeof(doorPreview));
    memset(&doorApplied, 0, sizeof(doorApplied));
    matchStatus = findTransitionCommands(&select, &descriptor,
                                         &saveOffset, &changeOffset,
                                         &doorOffset, &doorPresent,
                                         &eligibleCount);
    outResult->sequence = intent->sequence;
    outResult->frontTile = select.frontTile;
    outResult->eventIndex = select.eventIndex;
    outResult->eligibleCount = eligibleCount;
    outResult->saveCommandOffset = saveOffset;
    outResult->changeCommandOffset = changeOffset;
    outResult->doorCommandOffset = doorOffset;
    outResult->doorReady = doorPresent;
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

    if (doorPresent != 0U) {
        doorStatus = EspMapLineState_previewDoorCommand(
            &descriptor, doorOffset, &doorPreview);
        if (doorStatus != ESP_MAP_LINE_DOOR_OK ||
            doorPreview.codeId != ESP_MAP_OPCODE_OPENLINE ||
            doorPreview.mutated != 0U ||
            !EspMapScriptState_isCommandRemoved(
                doorPreview.globalCommandIndex, &doorRemovedBefore) ||
            EspNativeDoorAnimator_hasPendingFrames()) {
            return ESP_NATIVE_GAMEPLAY_TRANSITION_COMPLEX;
        }
        doorRemovedAfter = doorRemovedBefore;
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
    if (stringStatus != ESP_MAP_STRING_READ_OK ||
        changeNameLength >= sizeof(changeName) ||
        saveRoute.mapNameLength != changeNameLength ||
        strcmp(saveRoute.mapName, changeName) != 0 ||
        !EspMapCatalog_idForName(changeName, &targetMapId) ||
        !EspMapCatalog_isValidId(targetMapId) ||
        targetMapId == view->targetMapId) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    /* Direct-load transitions are intentionally left for the destructive
     * handoff milestone. The first real Entrance exit is a show-stats route. */
    if (changeResult.showStats != 1U) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_UNSUPPORTED;
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
        menuStatus != ESP_STATS_MENU_INTENT_OK ||
        statsIntent.active != 1U || statsIntent.consumePending != 1U) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    committedStatus = EspMapCommittedTransition_begin(
        &committed, view->targetMapId, &pendingChange, &changeResult,
        &statsIntent, &preflight);
    if (committedStatus != ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS ||
        committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS ||
        committed.pendingConsumed != 1U || committed.committed != 0U ||
        EspMapChangeMap_isActive(&pendingChange) || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_isReady()) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    if (doorPresent != 0U) {
        doorStatus = EspMapLineState_applyDoorCommand(
            &descriptor, doorOffset, &doorApplied);
        if (doorStatus != ESP_MAP_LINE_DOOR_OK ||
            doorApplied.mutated != 1U ||
            doorApplied.codeId != ESP_MAP_OPCODE_OPENLINE ||
            doorApplied.lineIndex != doorPreview.lineIndex ||
            doorApplied.globalCommandIndex != doorPreview.globalCommandIndex ||
            doorApplied.openBefore != doorPreview.openBefore ||
            doorApplied.openAfter != doorPreview.openAfter) {
            if (doorApplied.mutated != 0U) {
                (void)EspMapLineState_setOpen(
                    doorApplied.lineIndex, doorApplied.openBefore);
                (void)EspNativeDoorAnimator_validateLineState();
            }
            return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
        }

        if (doorApplied.removeCommandIfHandled != 0U &&
            doorRemovedBefore == 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    doorApplied.globalCommandIndex, 1U)) {
                (void)EspMapLineState_setOpen(
                    doorApplied.lineIndex, doorApplied.openBefore);
                (void)EspNativeDoorAnimator_validateLineState();
                return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
            }
            doorRemovedAfter = 1U;
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
    next.doorRemovedBefore = doorRemovedBefore;
    next.doorRemovedAfter = doorRemovedAfter;
    next.waitingDoor = doorPresent;
    if (doorPresent != 0U) next.doorResult = doorApplied;
    next.active = 1U;
    next.waitingStats = doorPresent == 0U ? 1U : 0U;
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
    printf("[NATIVECHANGEMAP] STATS sourceMap=%u sourceGameplayLoad=%u secrets=%u/%u monsters=%u/%u completionBit=%08x effects=%02x playerExitApply=deferred reason=authoritative-turn-counter-not-owned\n",
           (unsigned int)view->targetMapId,
           (unsigned int)view->gameplayLoadMapId,
           (unsigned int)levelStats.secretsFound,
           (unsigned int)levelStats.secretsTotal,
           (unsigned int)levelStats.monstersDead,
           (unsigned int)levelStats.monstersTotal,
           (unsigned int)levelStats.completionLevelBit,
           (unsigned int)levelStats.effectFlags);
    if (doorPresent != 0U) {
        printf("[NATIVECHANGEMAP] WAIT_DOOR phase=%u menuKind=%u cmd=%u line=%u open=%u->%u removed=%u->%u animation=bounded-4frame sourceResident=%u packOpen=%u statsAck=no\n",
               (unsigned int)committed.phase,
               (unsigned int)statsIntent.menuKind,
               (unsigned int)doorOffset,
               (unsigned int)doorApplied.lineIndex,
               (unsigned int)doorApplied.openBefore,
               (unsigned int)doorApplied.openAfter,
               (unsigned int)doorRemovedBefore,
               (unsigned int)doorRemovedAfter,
               (unsigned int)EspMapResidentLifecycle_isReady(),
               (unsigned int)EspAssetPack_isOpen());
        return ESP_NATIVE_GAMEPLAY_TRANSITION_DOOR_READY;
    }

    printf("[NATIVECHANGEMAP] WAIT_STATS phase=%u menuKind=%u pendingConsumed=%u sourceResident=%u packOpen=%u destructiveHandoff=no statsAck=no\n",
           (unsigned int)committed.phase,
           (unsigned int)statsIntent.menuKind,
           (unsigned int)committed.pendingConsumed,
           (unsigned int)EspMapResidentLifecycle_isReady(),
           (unsigned int)EspAssetPack_isOpen());
    return ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS;
}

EspNativeGameplayTransitionStatus EspNativeGameplayTransition_finishDoor(
    uint32_t sequence,
    uint16_t eventIndex,
    uint16_t lineIndex) {
    uint8_t openNow;
    uint8_t removedNow;

    if (!EspNativeGameplayTransition_isWaitingDoor() ||
        transitionState.sequence != sequence ||
        transitionState.eventIndex != eventIndex ||
        transitionState.doorResult.lineIndex != lineIndex ||
        transitionState.doorResult.mutated != 1U ||
        transitionState.doorResult.codeId != ESP_MAP_OPCODE_OPENLINE ||
        EspNativeDoorAnimator_hasPendingFrames() ||
        !EspMapLineState_getOpen(lineIndex, &openNow) ||
        !EspMapScriptState_isCommandRemoved(
            transitionState.doorResult.globalCommandIndex, &removedNow) ||
        openNow != transitionState.doorResult.openAfter ||
        removedNow != transitionState.doorRemovedAfter) {
        return ESP_NATIVE_GAMEPLAY_TRANSITION_FAILED;
    }

    transitionState.waitingDoor = 0U;
    transitionState.waitingStats = 1U;
    printf("[NATIVECHANGEMAP] DOOR-COMPLETE seq=%u event=%u line=%u open=%u phase=WAIT_STATS sourceResident=%u statsPresentation=native-pending-arm\n",
           (unsigned int)sequence,
           (unsigned int)eventIndex,
           (unsigned int)lineIndex,
           (unsigned int)openNow,
           (unsigned int)EspMapResidentLifecycle_isReady());

    /*
     * The platform NULL-callback wrapper recognizes WAIT_STATS and hands
     * framebuffer/input ownership to the bounded native level-complete
     * presenter. Its one tap advances into the loading presentation without
     * changing this transition ownership boundary.
     */
    PlatformInput_setTapCallback(NULL);
    return ESP_NATIVE_GAMEPLAY_TRANSITION_WAIT_STATS;
}

int EspNativeGameplayTransition_abortDoor(
    uint32_t sequence,
    uint16_t eventIndex,
    uint16_t lineIndex) {
    if (!EspNativeGameplayTransition_isWaitingDoor() ||
        transitionState.sequence != sequence ||
        transitionState.eventIndex != eventIndex ||
        transitionState.doorResult.lineIndex != lineIndex) {
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
        const EspMapLineDoorResult* door;

        if (staged == NULL || !EspNativeGameplayTransition_isWaitingDoor()) {
            return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
        }
        door = &staged->doorResult;
        memset(outResult, 0, sizeof(*outResult));
        outResult->sequence = transition.sequence;
        outResult->frontTile = transition.frontTile;
        outResult->eventIndex = transition.eventIndex;
        outResult->globalCommandIndex = door->globalCommandIndex;
        outResult->lineIndex = door->lineIndex;
        outResult->soundId = door->soundId;
        outResult->commandOffset = door->sourceCommandOffset;
        outResult->codeId = door->codeId;
        outResult->eligibleCount = transition.eligibleCount;
        outResult->openBefore = door->openBefore;
        outResult->openAfter = door->openAfter;
        outResult->locked = door->locked;
        outResult->handled = 1U;
        outResult->mutated = 1U;
        outResult->effectFlags = door->effectFlags;
        outResult->removedBefore = staged->doorRemovedBefore;
        outResult->removedAfter = staged->doorRemovedAfter;
        outResult->removeIfHandled = door->removeCommandIfHandled;
        outResult->rollbackAvailable = 1U;
        outResult->doorCount = 1U;
        outResult->doors[0].globalCommandIndex = door->globalCommandIndex;
        outResult->doors[0].lineIndex = door->lineIndex;
        outResult->doors[0].soundId = door->soundId;
        outResult->doors[0].commandOffset = door->sourceCommandOffset;
        outResult->doors[0].codeId = door->codeId;
        outResult->doors[0].openBefore = door->openBefore;
        outResult->doors[0].openAfter = door->openAfter;
        outResult->doors[0].locked = door->locked;
        outResult->doors[0].effectFlags = door->effectFlags;
        outResult->doors[0].removedBefore = staged->doorRemovedBefore;
        outResult->doors[0].removedAfter = staged->doorRemovedAfter;
        outResult->doors[0].removeIfHandled = door->removeCommandIfHandled;
        printf("[NATIVECHANGEMAP] DOOR-READY seq=%u event=%u cmd=%u line=%u open=%u->%u transitionHandled=yes render=shared-door-batch\n",
               (unsigned int)transition.sequence,
               (unsigned int)transition.eventIndex,
               (unsigned int)door->sourceCommandOffset,
               (unsigned int)door->lineIndex,
               (unsigned int)door->openBefore,
               (unsigned int)door->openAfter);
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
