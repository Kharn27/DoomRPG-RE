#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "native_map1_change_map_probe.h"
#include "native_map1_show_hide_diag.h"
#include "native_map1_show_hide_probe.h"
#include "native_map1_show_hide_probe_internal.h"

#define EXPECTED_EVENT_COUNT 93U

typedef struct Esp32ShowHideDiagState_s {
    int done;
} Esp32ShowHideDiagState;

static Esp32ShowHideDiagState diagState;

void Esp32Map1ShowHideDiag_reset(void) {
    memset(&diagState, 0, sizeof(diagState));
}

static void printShow(uint32_t commandIndex,
                      uint32_t eventIndex,
                      uint32_t commandOffset,
                      EspMapSpriteTopologyStatus status,
                      uint32_t beforeFNV,
                      uint32_t afterFNV,
                      const EspMapShowResult* result) {
    printf("[MAPSHOWHIDEDIAG] SHOW cmd=%u event=%u off=%u status=%u before=%08x after=%08x sprite=%u tile=%u targetEnt=%u linked=%u->%u blockers=%u removed=%u noops=%u effects=%04x handled=%u remove=%u\n",
           (unsigned int)commandIndex,
           (unsigned int)eventIndex,
           (unsigned int)commandOffset,
           (unsigned int)status,
           (unsigned int)beforeFNV,
           (unsigned int)afterFNV,
           result != NULL ? (unsigned int)result->spriteIndex : 0U,
           result != NULL ? (unsigned int)result->tileIndex : 0U,
           result != NULL ? (unsigned int)result->targetHasEntity : 0U,
           result != NULL ? (unsigned int)result->targetLinkedBefore : 0U,
           result != NULL ? (unsigned int)result->targetLinkedAfter : 0U,
           result != NULL ? (unsigned int)result->blockersFound : 0U,
           result != NULL ? (unsigned int)result->blockersRemoved : 0U,
           result != NULL ? (unsigned int)result->blockerNoops : 0U,
           result != NULL ? (unsigned int)result->effectFlags : 0U,
           result != NULL ? (unsigned int)result->legacyReturnValue : 0U,
           result != NULL ? (unsigned int)result->removeCommandIfHandled : 0U);
}

static void printHide(uint32_t commandIndex,
                      uint32_t eventIndex,
                      uint32_t commandOffset,
                      EspMapSpriteTopologyStatus status,
                      uint32_t beforeFNV,
                      uint32_t afterFNV,
                      const EspMapHideResult* result) {
    printf("[MAPSHOWHIDEDIAG] HIDE cmd=%u event=%u off=%u status=%u before=%08x after=%08x tile=%u,%u index=%u hidden=%u first=%u last=%u effects=%02x handled=%u remove=%u\n",
           (unsigned int)commandIndex,
           (unsigned int)eventIndex,
           (unsigned int)commandOffset,
           (unsigned int)status,
           (unsigned int)beforeFNV,
           (unsigned int)afterFNV,
           result != NULL ? (unsigned int)result->tileX : 0U,
           result != NULL ? (unsigned int)result->tileY : 0U,
           result != NULL ? (unsigned int)result->tileIndex : 0U,
           result != NULL ? (unsigned int)result->hiddenEntityCount : 0U,
           result != NULL ? (unsigned int)result->firstHiddenSpriteIndex : 0U,
           result != NULL ? (unsigned int)result->lastHiddenSpriteIndex : 0U,
           result != NULL ? (unsigned int)result->effectFlags : 0U,
           result != NULL ? (unsigned int)result->legacyReturnValue : 0U,
           result != NULL ? (unsigned int)result->removeCommandIfHandled : 0U);
}

void Esp32Map1ShowHideDiag_service(struct DoomRPG_s* doomRpg) {
    Esp32ShowHideTopologyAudit topologyAudit;
    const EspMapSpriteTopologyView* view;
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapShowResult showResult;
    EspMapHideResult hideResult;
    EspMapSpriteTopologyStatus status;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t commandIndex;
    uint32_t beforeFNV;
    uint32_t afterFNV;
    uint32_t refs = 0U;
    uint32_t showRefs = 0U;
    uint32_t hideRefs = 0U;
    uint32_t showOk = 0U;
    uint32_t hideOk = 0U;
    uint32_t showAlreadyLinked = 0U;
    uint32_t showRandomBlocker = 0U;
    uint32_t showOtherFailure = 0U;
    uint32_t hideOtherFailure = 0U;
    int initialOk;

    if (diagState.done || doomRpg == NULL ||
        !Esp32Map1ChangeMapProbe_isDone() || Esp32Map1ShowHideProbe_isDone() ||
        !EspMapSpriteTopology_isReady()) {
        return;
    }

    diagState.done = 1;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return;

    printf("[MAPSHOWHIDEDIAG] BEGIN sprites=%u storageBytes=%u stateFNV=%08x entities=%u linked=%u hidden=%u enemies=%u destructibles=%u nextOrder=%u showResultBytes=%u hideResultBytes=%u\n",
           (unsigned int)view->spriteCount,
           (unsigned int)view->storageBytes,
           (unsigned int)view->stateFNV1a,
           (unsigned int)view->entityCount,
           (unsigned int)view->linkedCount,
           (unsigned int)view->hiddenCount,
           (unsigned int)view->enemyCount,
           (unsigned int)view->destructibleCount,
           (unsigned int)view->nextLinkOrder,
           (unsigned int)sizeof(EspMapShowResult),
           (unsigned int)sizeof(EspMapHideResult));

    memset(&topologyAudit, 0, sizeof(topologyAudit));
    initialOk = Esp32ShowHideProbe_auditInitial(doomRpg, &topologyAudit);
    printf("[MAPSHOWHIDEDIAG] INITIAL ok=%d hasDef=%u fallback=%u entities=%u linked=%u hiddenEntities=%u enemies=%u destructibles=%u topologyFNV=%08x\n",
           initialOk,
           (unsigned int)topologyAudit.hasDefCount,
           (unsigned int)topologyAudit.fallbackCount,
           (unsigned int)topologyAudit.entityCount,
           (unsigned int)topologyAudit.linkedCount,
           (unsigned int)topologyAudit.hiddenEntityCount,
           (unsigned int)topologyAudit.enemyCount,
           (unsigned int)topologyAudit.destructibleCount,
           (unsigned int)topologyAudit.topologyFNV);

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!Esp32ShowHideProbe_descriptorByIndex(eventIndex, &descriptor)) {
            printf("[MAPSHOWHIDEDIAG] DESCRIPTOR_FAIL event=%u\n",
                   (unsigned int)eventIndex);
            continue;
        }
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                printf("[MAPSHOWHIDEDIAG] COMMAND_FAIL event=%u off=%u\n",
                       (unsigned int)eventIndex,
                       (unsigned int)commandOffset);
                continue;
            }
            if (command.id != ESP_MAP_OPCODE_SHOW &&
                command.id != ESP_MAP_OPCODE_HIDE) {
                continue;
            }

            ++refs;
            commandIndex =
                (uint32_t)descriptor.firstCommandIndex + commandOffset;
            if (!EspMapSpriteTopology_resetMutableFromRuntime()) {
                printf("[MAPSHOWHIDEDIAG] RESET_FAIL cmd=%u\n",
                       (unsigned int)commandIndex);
                continue;
            }
            view = EspMapSpriteTopology_view();
            if (view == NULL) continue;
            beforeFNV = view->stateFNV1a;

            if (command.id == ESP_MAP_OPCODE_SHOW) {
                ++showRefs;
                memset(&showResult, 0, sizeof(showResult));
                status = EspMapSpriteTopology_applyShow(
                    &descriptor, commandOffset, &showResult);
                view = EspMapSpriteTopology_view();
                afterFNV = view != NULL ? view->stateFNV1a : 0U;
                if (status == ESP_MAP_SPRITE_TOPOLOGY_OK) ++showOk;
                else if (status == ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED) {
                    ++showAlreadyLinked;
                }
                else if (status == ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER) {
                    ++showRandomBlocker;
                }
                else {
                    ++showOtherFailure;
                }
                printShow(commandIndex, eventIndex, commandOffset, status,
                          beforeFNV, afterFNV, &showResult);
            }
            else {
                ++hideRefs;
                memset(&hideResult, 0, sizeof(hideResult));
                status = EspMapSpriteTopology_applyHide(
                    &descriptor, commandOffset, &hideResult);
                view = EspMapSpriteTopology_view();
                afterFNV = view != NULL ? view->stateFNV1a : 0U;
                if (status == ESP_MAP_SPRITE_TOPOLOGY_OK) ++hideOk;
                else ++hideOtherFailure;
                printHide(commandIndex, eventIndex, commandOffset, status,
                          beforeFNV, afterFNV, &hideResult);
            }
        }
    }

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) {
        printf("[MAPSHOWHIDEDIAG] FINAL_RESET_FAIL\n");
        return;
    }
    view = EspMapSpriteTopology_view();
    printf("[MAPSHOWHIDEDIAG] SUMMARY initialOk=%d refs=%u show=%u hide=%u showOk=%u hideOk=%u showAlreadyLinked=%u showRandomBlocker=%u showOtherFailure=%u hideOtherFailure=%u finalFNV=%08x\n",
           initialOk,
           (unsigned int)refs,
           (unsigned int)showRefs,
           (unsigned int)hideRefs,
           (unsigned int)showOk,
           (unsigned int)hideOk,
           (unsigned int)showAlreadyLinked,
           (unsigned int)showRandomBlocker,
           (unsigned int)showOtherFailure,
           (unsigned int)hideOtherFailure,
           view != NULL ? (unsigned int)view->stateFNV1a : 0U);
}
