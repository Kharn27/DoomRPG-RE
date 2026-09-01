#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_interaction_inventory.h"

#define INVENTORY_OPCODE_LIMIT 64U
#define INVENTORY_IDS_CAPACITY 48U
#define DEATH_ROUTE_RUN_FLAGS 0x00000100UL

static uint32_t loggedArenaFNV;

static const char* opcodeName(uint8_t codeId) {
    switch (codeId) {
    case 1U: return "GOTO";
    case 2U: return "CHANGEMAP";
    case 3U: return "TRIGGER";
    case 4U: return "MESSAGE";
    case 5U: return "PAIN";
    case 6U: return "MOVELINE";
    case 7U: return "SHOW";
    case 8U: return "DIALOG";
    case 9U: return "GIVEMAP";
    case 10U: return "PASSWORD";
    case 11U: return "CHANGESTATE";
    case 12U: return "LOCK";
    case 13U: return "UNLOCK";
    case 14U: return "TOGGLELOCK";
    case 15U: return "OPENLINE";
    case 16U: return "CLOSELINE";
    case 17U: return "MOVELINE2";
    case 18U: return "HIDE";
    case 19U: return "NEXTSTATE";
    case 20U: return "PREVSTATE";
    case 21U: return "INCSTAT";
    case 22U: return "DECSTAT";
    case 23U: return "REQSTAT";
    case 24U: return "FORCEMESSAGE";
    case 25U: return "ANIM";
    case 26U: return "DIALOGNOBACK";
    case 27U: return "SAVEGAME";
    case 28U: return "ABORTMOVE";
    case 29U: return "SCREENSHAKE";
    case 30U: return "CHANGEFLOORCOLOR";
    case 31U: return "CHANGECEILCOLOR";
    case 32U: return "ENABLEWEAPONS";
    case 33U: return "OPENSTORE";
    case 34U: return "CHANGESPRITE";
    case 35U: return "SPAWNPARTICLES";
    case 36U: return "REFRESHVIEW";
    case 37U: return "WAIT";
    case 38U: return "ACTIVE_PORTAL";
    case 39U: return "CHECK_COMPLETED_LEVEL";
    case 40U: return "NOTE";
    case 41U: return "CHECK_KEY";
    case 42U: return "PLAYSOUND";
    default: return "UNKNOWN";
    }
}

/* A bounded family has a production executor somewhere in the current resident
 * gameplay path.  This intentionally does not claim that every triggering
 * context is already wired; it only distinguishes known native semantics from
 * opcodes that still require a dedicated production milestone. */
static int boundedFamily(uint8_t codeId) {
    switch (codeId) {
    case 7U:  /* SHOW, post-dialog chain */
    case 8U:  /* DIALOG */
    case 11U: /* CHANGESTATE */
    case 13U: /* UNLOCK */
    case 15U: /* OPENLINE */
    case 16U: /* CLOSELINE */
    case 18U: /* HIDE */
    case 19U: /* NEXTSTATE */
    case 20U: /* PREVSTATE */
    case 24U: /* FORCEMESSAGE */
    case 26U: /* DIALOGNOBACK */
    case 40U: /* NOTE prefix */
        return 1;
    default:
        return 0;
    }
}

static const char* deferredReason(uint8_t codeId) {
    switch (codeId) {
    case 2U: return "transition-consumer";
    case 9U: return "automap-production-route";
    case 10U: return "password-input-ui";
    case 27U: return "save-route-consumer";
    case 41U: return "native-player-keys";
    default: return "dedicated-milestone";
    }
}

static int appendId(char* buffer,
                    size_t capacity,
                    size_t* used,
                    uint8_t codeId) {
    int written;
    if (buffer == NULL || used == NULL || *used >= capacity) return 0;
    written = snprintf(buffer + *used, capacity - *used,
                       "%s%u/%s",
                       *used == 0U ? "" : ",",
                       (unsigned int)codeId,
                       opcodeName(codeId));
    if (written < 0 || (size_t)written >= capacity - *used) return 0;
    *used += (size_t)written;
    return 1;
}

/*
 * Temporary, side-effect-free recovery witness for legacy Game_remove().
 * A line-linked entity death calls Game_executeTile(line midpoint, 0x100), so
 * inspect exactly that filtering context before destructible combat owns any
 * mutation.  Only eligible commands are logged; no command is executed and no
 * mutable map state is changed.  This lets hardware reveal the real scripted
 * consequence of MAP_INTRO's jammed-door event without replaying the level.
 */
static void logDeathRouteProbe(const EspMapRuntimeView* runtime) {
    uint32_t eventIndex;
    uint32_t eligibleEvents = 0U;
    uint32_t eligibleCommands = 0U;

    if (runtime == NULL || !EspMapScriptState_isReady()) return;

    printf("[DEATHROUTEPROBE] BEGIN arena=%08x runFlags=%08x events=%u mutation=forbidden\n",
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)DEATH_ROUTE_RUN_FLAGS,
           (unsigned int)runtime->eventCount);

    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t raw;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        EspMapEventFilterPlan plan;
        uint8_t currentState;
        uint32_t offset;
        uint32_t eventEligible = 0U;

        if (!EspMapRuntime_getEvent(eventIndex, &raw)) return;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(raw & ESP_MAP_EVENT_TILE_MASK);
        ref.value = raw;
        if (!EspMapEvents_describe(&ref, &descriptor) ||
            !EspMapScriptState_getEventState(descriptor.eventIndex,
                                             &currentState) ||
            !EspMapEventFilter_prepare(&descriptor,
                                       currentState,
                                       0U,
                                       DEATH_ROUTE_RUN_FLAGS,
                                       0U,
                                       &plan)) {
            return;
        }

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapEventCommandFilterResult filtered;
            EspMapByteCode command;
            uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
            uint8_t removed;

            if (global > UINT16_MAX ||
                !EspMapScriptState_isCommandRemoved(global, &removed) ||
                !EspMapEventFilter_evaluate(&descriptor,
                                            &plan,
                                            offset,
                                            removed,
                                            &filtered)) {
                return;
            }
            if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return;

            if (eventEligible == 0U) {
                printf("[DEATHROUTEPROBE] EVENT event=%u tile=%u state=%u initialState=%u commands=%u runFlags=%08x\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)descriptor.tileIndex,
                       (unsigned int)currentState,
                       (unsigned int)descriptor.initialState,
                       (unsigned int)descriptor.commandCount,
                       (unsigned int)DEATH_ROUTE_RUN_FLAGS);
                ++eligibleEvents;
            }
            ++eventEligible;
            ++eligibleCommands;
            printf("[DEATHROUTEPROBE] ELIGIBLE event=%u cmd=%u global=%u opcode=%u/%s arg1=%08x arg2=%08x removed=%u decision=%u\n",
                   (unsigned int)descriptor.eventIndex,
                   (unsigned int)offset,
                   (unsigned int)filtered.globalCommandIndex,
                   (unsigned int)command.id,
                   opcodeName(command.id),
                   (unsigned int)command.arg1,
                   (unsigned int)command.arg2,
                   (unsigned int)removed,
                   (unsigned int)filtered.decision);
        }
    }

    printf("[DEATHROUTEPROBE] SUMMARY eligibleEvents=%u eligibleCommands=%u runFlags=%08x mutation=no\n",
           (unsigned int)eligibleEvents,
           (unsigned int)eligibleCommands,
           (unsigned int)DEATH_ROUTE_RUN_FLAGS);
}

void EspNativeGameplayInteractionInventory_log(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint16_t counts[INVENTORY_OPCODE_LIMIT];
    uint32_t eventIndex;
    uint32_t deferredEvents = 0U;
    uint32_t boundedCommands = 0U;
    uint32_t deferredCommands = 0U;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->arenaFNV1a == loggedArenaFNV) return;

    memset(counts, 0, sizeof(counts));
    printf("[INTERACTMAP] BEGIN arena=%08x events=%u commands=%u policy=report-all-unbounded-before-player-contact\n",
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)runtime->eventCount,
           (unsigned int)runtime->byteCodeCount);

    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t raw;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        char ids[INVENTORY_IDS_CAPACITY];
        size_t used = 0U;
        uint32_t offset;
        uint8_t eventHasDeferred = 0U;
        uint8_t firstDeferred = 0U;

        memset(ids, 0, sizeof(ids));
        if (!EspMapRuntime_getEvent(eventIndex, &raw)) return;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(raw & ESP_MAP_EVENT_TILE_MASK);
        ref.value = raw;
        if (!EspMapEvents_describe(&ref, &descriptor)) return;

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return;
            if (command.id < INVENTORY_OPCODE_LIMIT &&
                counts[command.id] != UINT16_MAX) {
                ++counts[command.id];
            }
            if (boundedFamily(command.id)) {
                ++boundedCommands;
                continue;
            }
            ++deferredCommands;
            if (!eventHasDeferred) firstDeferred = command.id;
            eventHasDeferred = 1U;
            if (!appendId(ids, sizeof(ids), &used, command.id)) {
                strncpy(ids, "list-truncated", sizeof(ids));
                ids[sizeof(ids) - 1U] = '\0';
            }
        }

        if (eventHasDeferred) {
            ++deferredEvents;
            printf("[INTERACTMAP] DEFER event=%u tile=%u initialState=%u commands=%u range=%u..%u ids=%s reason=%s\n",
                   (unsigned int)descriptor.eventIndex,
                   (unsigned int)descriptor.tileIndex,
                   (unsigned int)descriptor.initialState,
                   (unsigned int)descriptor.commandCount,
                   (unsigned int)descriptor.firstCommandIndex,
                   (unsigned int)descriptor.commandEndIndex,
                   ids[0] != '\0' ? ids : "unknown",
                   deferredReason(firstDeferred));
        }
    }

    printf("[INTERACTMAP] SUMMARY arena=%08x deferredEvents=%u boundedCommands=%u deferredCommands=%u\n",
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)deferredEvents,
           (unsigned int)boundedCommands,
           (unsigned int)deferredCommands);
    for (eventIndex = 0U; eventIndex < INVENTORY_OPCODE_LIMIT; ++eventIndex) {
        if (counts[eventIndex] == 0U) continue;
        printf("[INTERACTMAP] OPCODE id=%u name=%s count=%u family=%s%s%s\n",
               (unsigned int)eventIndex,
               opcodeName((uint8_t)eventIndex),
               (unsigned int)counts[eventIndex],
               boundedFamily((uint8_t)eventIndex) ? "bounded" : "DEFERRED",
               boundedFamily((uint8_t)eventIndex) ? "" : " reason=",
               boundedFamily((uint8_t)eventIndex)
                   ? "" : deferredReason((uint8_t)eventIndex));
    }

    logDeathRouteProbe(runtime);
    loggedArenaFNV = runtime->arenaFNV1a;
}
