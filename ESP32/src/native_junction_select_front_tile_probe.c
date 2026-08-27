#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_key_gate.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_select.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

#define SELECT_KEY_BITS 0U

/* Temporary gameplay milestone observer. The engine root is used only for
 * before/after integrity hashing; no legacy field is read as gameplay truth. */
extern DoomRPG_t* doomRpg;

typedef struct LegacySnapshot_s {
    uint32_t hud;
    uint32_t player;
    uint32_t game;
    uint32_t canvas;
    uint32_t render;
} LegacySnapshot;

typedef struct SelectProbeState_s {
    uint32_t selects;
    uint8_t readyLogged;
    uint8_t failed;
    uint8_t reserved[2];
} SelectProbeState;

static SelectProbeState probeState;

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int legacySnapshot(const DoomRPG_t* root, LegacySnapshot* out) {
    if (root == NULL || out == NULL || root->hud == NULL ||
        root->player == NULL || root->game == NULL ||
        root->doomCanvas == NULL || root->render == NULL) return 0;
    out->hud = fnv1a(root->hud, sizeof(*root->hud));
    out->player = fnv1a(root->player, sizeof(*root->player));
    out->game = fnv1a(root->game, sizeof(*root->game));
    out->canvas = fnv1a(root->doomCanvas, sizeof(*root->doomCanvas));
    out->render = fnv1a(root->render, sizeof(*root->render));
    return 1;
}

static int legacyEqual(const LegacySnapshot* a, const LegacySnapshot* b) {
    return a != NULL && b != NULL && memcmp(a, b, sizeof(*a)) == 0;
}

static int runtimeBoundary(const DoomRPG_t* root) {
    return root != NULL && root->doomCanvas != NULL && root->game != NULL &&
           root->render != NULL && root->doomCanvas->state == ST_INTRO &&
           root->doomCanvas->storyPage == 3 && root->game->numEntities == 0 &&
           root->game->numMonsters == 0 && root->render->shapeData == NULL &&
           root->render->mediaTexels == NULL && !EspAssetPack_isOpen();
}

static const char* decisionName(uint8_t decision) {
    switch ((EspMapEventCommandDecision)decision) {
    case ESP_MAP_EVENT_COMMAND_ELIGIBLE: return "ELIGIBLE";
    case ESP_MAP_EVENT_COMMAND_EVENT_BLOCKED: return "EVENT_BLOCKED";
    case ESP_MAP_EVENT_COMMAND_BEFORE_START: return "BEFORE_START";
    case ESP_MAP_EVENT_COMMAND_REMOVED: return "REMOVED";
    case ESP_MAP_EVENT_COMMAND_STATE_MISMATCH: return "STATE_MISMATCH";
    case ESP_MAP_EVENT_COMMAND_KEY_MISMATCH: return "KEY_MISMATCH";
    case ESP_MAP_EVENT_COMMAND_FLAGS_MISMATCH: return "FLAGS_MISMATCH";
    default: return "UNKNOWN";
    }
}

static const char* opcodeName(uint8_t id) {
    switch (id) {
    case 2U: return "EV_CHANGEMAP";
    case 7U: return "EV_SHOW";
    case 8U: return "EV_DIALOG";
    case 9U: return "EV_GIVEMAP";
    case 10U: return "EV_PASSWORD";
    case 11U: return "EV_CHANGESTATE";
    case 13U: return "EV_UNLOCK";
    case 15U: return "EV_OPENLINE";
    case 16U: return "EV_CLOSELINE";
    case 18U: return "EV_HIDE";
    case 19U: return "EV_NEXTSTATE";
    case 24U: return "EV_FORCEMESSAGE";
    case 26U: return "EV_DIALOGNOBACK";
    case 27U: return "EV_SAVEGAME";
    case 40U: return "EV_NOTE";
    case 41U: return "EV_CHECK_KEY";
    default: return "UNOWNED";
    }
}

static const char* keyName(uint8_t keyIndex) {
    switch (keyIndex) {
    case ESP_MAP_KEY_GREEN: return "GREEN";
    case ESP_MAP_KEY_YELLOW: return "YELLOW";
    case ESP_MAP_KEY_BLUE: return "BLUE";
    case ESP_MAP_KEY_RED: return "RED";
    default: return "INVALID";
    }
}

void Esp32NativeGameplaySelectProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) {
    EspNativeGameplaySelectResult selectResult;
    EspNativeGameplaySelectStatus selectStatus;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    LegacySnapshot legacyBefore;
    LegacySnapshot legacyAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    unsigned int eligibleCount = 0U;
    unsigned int keyGateCount = 0U;
    unsigned int blockedKeyCount = 0U;
    unsigned int i;
    int inspectionOk = 1;

    if (intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        probeState.failed) return;

    if (!probeState.readyLogged) {
        printf("\n=== Doom RPG ESP32-native SELECT front-tile observer ===\n");
        printf("[SELECTPROBE] CONTRACT legacy SELECT first step only: dest+viewStep -> Game_executeTile flags=0x500 -> native tile/event/filter provenance. Read-only observer; current native key context is 0 because pickup/inventory gameplay is not owned yet. No bytecode execution, no door mutation, no entity trace/combat fallback, no HUD message, no sound, no animation, no render, no turn advance.\n");
        printf("[SELECTPROBE] READY resultBytes=%u runFlags=%08x keyBits=%02x keySource=native-unowned-zero callbackPhase=pre-feedback-draw allocation=none\n",
               (unsigned int)sizeof(EspNativeGameplaySelectResult),
               (unsigned int)ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS,
               (unsigned int)SELECT_KEY_BITS);
        probeState.readyLogged = 1U;
    }

    ++probeState.selects;
    frameBefore = frameFNV();
    heapBefore = heap8();
    largestBefore = largest8();

    if (!runtimeBoundary(doomRpg) || frameBefore == 0U ||
        !legacySnapshot(doomRpg, &legacyBefore) ||
        !EspMapResidentLifecycle_capture(&residentBefore)) {
        printf("[SELECTPROBE] FAILED precondition n=%u frame=%08x boundary=%d pack=%d\n",
               (unsigned int)probeState.selects,
               (unsigned int)frameBefore,
               runtimeBoundary(doomRpg), EspAssetPack_isOpen());
        probeState.failed = 1U;
        return;
    }

    selectStatus = EspNativeGameplaySelect_resolve(intent, &selectResult);
    printf("[SELECT] RESOLVE n=%u seq=%u status=%s front=%d,%d tile=%u flags=%08x event=%s%u state=%u eventFlags=%u commands=%u range=%u..%u\n",
           (unsigned int)probeState.selects,
           (unsigned int)intent->sequence,
           EspNativeGameplaySelect_statusName(selectStatus),
           (int)selectResult.frontX, (int)selectResult.frontY,
           (unsigned int)selectResult.frontTile,
           (unsigned int)selectResult.inputFlags,
           selectResult.eventFound ? "" : "none/",
           selectResult.eventFound ? (unsigned int)selectResult.eventIndex : 0U,
           (unsigned int)selectResult.currentState,
           (unsigned int)selectResult.eventFlags,
           (unsigned int)selectResult.commandCount,
           (unsigned int)selectResult.firstCommandIndex,
           (unsigned int)selectResult.commandEndIndex);

    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT) {
        EspMapEventRef eventRef;
        EspMapEventDescriptor descriptor;
        EspMapEventFilterPlan plan;
        uint32_t rawEvent;

        memset(&eventRef, 0, sizeof(eventRef));
        memset(&descriptor, 0, sizeof(descriptor));
        memset(&plan, 0, sizeof(plan));

        if (!EspMapRuntime_getEvent(selectResult.eventIndex, &rawEvent)) {
            inspectionOk = 0;
        }
        else {
            eventRef.index = selectResult.eventIndex;
            eventRef.tileIndex = selectResult.frontTile;
            eventRef.value = rawEvent;
            if (!EspMapEvents_describe(&eventRef, &descriptor) ||
                descriptor.eventIndex != selectResult.eventIndex ||
                descriptor.tileIndex != selectResult.frontTile ||
                descriptor.commandCount != selectResult.commandCount ||
                !EspMapEventFilter_prepare(&descriptor,
                                           selectResult.currentState,
                                           0U,
                                           selectResult.inputFlags,
                                           SELECT_KEY_BITS,
                                           &plan)) {
                inspectionOk = 0;
            }
        }

        for (i = 0U; inspectionOk && i < descriptor.commandCount; ++i) {
            EspMapByteCode command;
            EspMapEventCommandFilterResult filtered;
            uint8_t removed;

            memset(&command, 0, sizeof(command));
            memset(&filtered, 0, sizeof(filtered));
            if (!EspMapEvents_getCommand(&descriptor, i, &command) ||
                !EspMapScriptState_isCommandRemoved(
                    (uint32_t)descriptor.firstCommandIndex + i, &removed) ||
                !EspMapEventFilter_evaluate(&descriptor, &plan, i, removed,
                                            &filtered)) {
                inspectionOk = 0;
                break;
            }

            printf("[SELECT] CMD event=%u off=%u global=%u opcode=%u/%s decision=%s removed=%u arg1=%08x arg2=%08x\n",
                   (unsigned int)descriptor.eventIndex,
                   i,
                   (unsigned int)filtered.globalCommandIndex,
                   (unsigned int)command.id,
                   opcodeName(command.id),
                   decisionName(filtered.decision),
                   (unsigned int)removed,
                   (unsigned int)command.arg1,
                   (unsigned int)command.arg2);

            if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
            ++eligibleCount;

            if (command.id == ESP_MAP_OPCODE_CHECK_KEY) {
                EspMapKeyGateResult gate;
                EspMapKeyGateStatus gateStatus;
                const char* message;
                memset(&gate, 0, sizeof(gate));
                gateStatus = EspMapKeyGate_evaluate(&descriptor, i,
                                                    SELECT_KEY_BITS, &gate);
                if (gateStatus != ESP_MAP_KEY_GATE_PASS &&
                    gateStatus != ESP_MAP_KEY_GATE_BLOCKED) {
                    inspectionOk = 0;
                    break;
                }
                ++keyGateCount;
                if (gateStatus == ESP_MAP_KEY_GATE_BLOCKED) ++blockedKeyCount;
                message = EspMapKeyGate_message(&gate);
                printf("[SELECT] KEY event=%u off=%u key=%u/%s required=%02x keys=%02x status=%s message=%s sound=%u stop=%u saveCurrent=%u effects=observer-only\n",
                       (unsigned int)descriptor.eventIndex,
                       i,
                       (unsigned int)gate.keyIndex,
                       keyName(gate.keyIndex),
                       (unsigned int)gate.requiredMask,
                       (unsigned int)SELECT_KEY_BITS,
                       gateStatus == ESP_MAP_KEY_GATE_PASS ? "PASS" : "BLOCKED",
                       message != NULL ? message : "none",
                       (unsigned int)gate.soundId,
                       (unsigned int)gate.stopEvent,
                       (unsigned int)gate.saveCurrentCommand);
            }
        }
    }
    else if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT) {
        printf("[SELECT] FALLBACK tileEvent=none entityTrace=UNSUPPORTED failClosed=yes\n");
    }
    else if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS) {
        printf("[SELECT] FALLBACK front=out-of-bounds entityTrace=UNSUPPORTED failClosed=yes\n");
    }
    else {
        inspectionOk = 0;
    }

    frameAfter = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (!legacySnapshot(doomRpg, &legacyAfter) ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        !runtimeBoundary(doomRpg) || !inspectionOk ||
        frameAfter != frameBefore || heapAfter != heapBefore ||
        largestAfter != largestBefore ||
        !legacyEqual(&legacyBefore, &legacyAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        EspAssetPack_isOpen()) {
        printf("[SELECTPROBE] FAILED invariant n=%u status=%s inspect=%d frame=%08x->%08x heap=%u->%u largest=%u->%u legacyExact=%d residentExact=%d pack=%d\n",
               (unsigned int)probeState.selects,
               EspNativeGameplaySelect_statusName(selectStatus),
               inspectionOk,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               legacyEqual(&legacyBefore, &legacyAfter),
               memcmp(&residentBefore, &residentAfter,
                      sizeof(residentBefore)) == 0,
               EspAssetPack_isOpen());
        probeState.failed = 1U;
        return;
    }

    printf("[SELECT] RESULT n=%u seq=%u status=%s eventFound=%u eligible=%u keyGates=%u blockedKeys=%u frame=%08x exact=yes heap=%u->%u largest=%u->%u legacyExact=yes residentExact=yes packClosed=yes bytecodeExec=no doorMutation=no entityTrace=no turnAdvance=no render=no\n",
           (unsigned int)probeState.selects,
           (unsigned int)intent->sequence,
           EspNativeGameplaySelect_statusName(selectStatus),
           (unsigned int)selectResult.eventFound,
           eligibleCount, keyGateCount, blockedKeyCount,
           (unsigned int)frameAfter,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
}
