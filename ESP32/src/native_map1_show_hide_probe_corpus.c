#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "native_map1_show_hide_probe_internal.h"

#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_MAP_SPRITE_COUNT 344U
#define EXPECTED_SHOW_RESULT_BYTES 26U
#define EXPECTED_HIDE_RESULT_BYTES 18U

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t hashU32(uint32_t hash, uint32_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static int validateShow(const EspMapEventDescriptor* descriptor,
                        uint32_t commandOffset,
                        const EspMapByteCode* command,
                        const EspMapShowResult* result) {
    EspMapSprite sprite;
    uint32_t globalCommandIndex;
    uint32_t spriteIndex;
    uint16_t tile;
    uint8_t oldVisual;
    uint8_t showFlags;
    uint8_t expectedVisual;

    if (descriptor == NULL || command == NULL || result == NULL ||
        command->id != ESP_MAP_OPCODE_SHOW) return 0;
    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    spriteIndex = command->arg1 & 0xffffU;
    if (globalCommandIndex > 0xffffU ||
        spriteIndex >= EXPECTED_MAP_SPRITE_COUNT ||
        !EspMapRuntime_getMapSprite(spriteIndex, &sprite) ||
        (sprite.x >> 6) >= 32U || (sprite.y >> 6) >= 32U) return 0;

    tile = (uint16_t)(((uint32_t)(sprite.y >> 6) * 32U) +
                      (uint32_t)(sprite.x >> 6));
    oldVisual = (uint8_t)((sprite.info >> 9) & 0xffU);
    showFlags = (uint8_t)((command->arg1 >> 16) & 0xffU);
    expectedVisual = (uint8_t)((oldVisual & 0x70U) | showFlags);

    return result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->spriteIndex == (uint16_t)spriteIndex &&
           result->tileIndex == tile &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->showFlags == showFlags &&
           result->visualBefore == oldVisual &&
           result->visualAfter == expectedVisual &&
           result->blockersFound <= 2U &&
           result->blockersRemoved <= result->blockersFound &&
           result->blockerNoops <= result->blockersFound &&
           result->targetLinkedBefore == 0U &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) &&
           (result->effectFlags & ESP_MAP_SHOW_EFFECT_VISUAL_STATE) != 0U &&
           (result->targetHasEntity == 0U ||
            ((result->effectFlags & ESP_MAP_SHOW_EFFECT_LINK_TARGET) != 0U &&
             result->targetLinkedAfter == 1U));
}

static int validateHide(const EspMapEventDescriptor* descriptor,
                        uint32_t commandOffset,
                        const EspMapByteCode* command,
                        const EspMapHideResult* result) {
    uint32_t globalCommandIndex;
    uint32_t x;
    uint32_t y;
    uint16_t tile;

    if (descriptor == NULL || command == NULL || result == NULL ||
        command->id != ESP_MAP_OPCODE_HIDE) return 0;
    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) return 0;
    x = command->arg1 & 0xffU;
    y = (command->arg1 >> 8) & 0xffU;
    tile = (x < 32U && y < 32U)
               ? (uint16_t)((y * 32U) + x)
               : ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;

    return result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->tileIndex == tile &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->tileX == (uint8_t)x && result->tileY == (uint8_t)y &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) &&
           ((result->hiddenEntityCount == 0U && result->effectFlags == 0U &&
             result->firstHiddenSpriteIndex ==
                 ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE &&
             result->lastHiddenSpriteIndex ==
                 ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) ||
            (result->hiddenEntityCount != 0U &&
             result->effectFlags ==
                 (ESP_MAP_HIDE_EFFECT_VISUAL_STATE |
                  ESP_MAP_HIDE_EFFECT_UNLINK)));
}

int Esp32ShowHideProbe_auditCorpus(Esp32ShowHideCorpusAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapShowResult showResult;
    EspMapShowResult repeatShow;
    EspMapHideResult hideResult;
    EspMapHideResult repeatHide;
    EspMapSpriteTopologyStatus status;
    const EspMapSpriteTopologyView* view;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t beforeFNV;
    uint32_t afterFNV;
    uint32_t showResultAggregate = 2166136261U;
    uint32_t hideResultAggregate = 2166136261U;
    uint32_t showStateAggregate = 2166136261U;
    uint32_t hideStateAggregate = 2166136261U;

    if (audit == NULL || !EspMapSpriteTopology_isReady()) return 0;
    memset(audit, 0, sizeof(*audit));
    if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    audit->initialStateFNV = view->stateFNV1a;

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!Esp32ShowHideProbe_descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }
            if (!audit->haveUnsupported && command.id != ESP_MAP_OPCODE_SHOW &&
                command.id != ESP_MAP_OPCODE_HIDE) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_SHOW &&
                command.id != ESP_MAP_OPCODE_HIDE) continue;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;
            ++audit->refs;
            if ((command.arg2 & ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) {
                ++audit->removableRefs;
            }

            if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
            view = EspMapSpriteTopology_view();
            if (view == NULL || view->stateFNV1a != audit->initialStateFNV) return 0;
            beforeFNV = view->stateFNV1a;

            if (command.id == ESP_MAP_OPCODE_SHOW) {
                memset(&showResult, 0, sizeof(showResult));
                status = EspMapSpriteTopology_applyShow(
                    &descriptor, commandOffset, &showResult);
                if (status == ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER) {
                    printf("[MAPSHOWHIDE] BLOCKED random crate cmd=%u event=%u off=%u sprite=%u\n",
                           (unsigned int)(descriptor.firstCommandIndex + commandOffset),
                           (unsigned int)descriptor.eventIndex,
                           (unsigned int)commandOffset,
                           (unsigned int)(command.arg1 & 0xffffU));
                    return 0;
                }
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    !validateShow(&descriptor, commandOffset, &command,
                                  &showResult)) return 0;
                view = EspMapSpriteTopology_view();
                if (view == NULL) return 0;
                afterFNV = view->stateFNV1a;
                ++audit->showRefs;
                if (afterFNV != beforeFNV) ++audit->showMutated;
                if (showResult.targetHasEntity) ++audit->showTargetEntities;
                else ++audit->showTargetNoEntity;
                audit->showBlockersFound += showResult.blockersFound;
                audit->showBlockersRemoved += showResult.blockersRemoved;
                audit->showBlockerNoops += showResult.blockerNoops;
                if ((showResult.effectFlags &
                     ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY) != 0U) {
                    ++audit->showDeferredDeaths;
                }
                showResultAggregate = hashU32(
                    showResultAggregate,
                    Esp32ShowHideProbe_showResultHash(&showResult));
                showStateAggregate = hashU32(showStateAggregate, afterFNV);
                if (!audit->haveShow) {
                    audit->showDescriptor = descriptor;
                    audit->showCommand = command;
                    audit->showResult = showResult;
                    audit->showOffset = (uint8_t)commandOffset;
                    audit->haveShow = 1U;
                }
            }
            else {
                memset(&hideResult, 0, sizeof(hideResult));
                status = EspMapSpriteTopology_applyHide(
                    &descriptor, commandOffset, &hideResult);
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    !validateHide(&descriptor, commandOffset, &command,
                                  &hideResult)) return 0;
                view = EspMapSpriteTopology_view();
                if (view == NULL) return 0;
                afterFNV = view->stateFNV1a;
                ++audit->hideRefs;
                if (afterFNV != beforeFNV) ++audit->hideMutated;
                else ++audit->hideNoMutation;
                audit->hideEntitiesTotal += hideResult.hiddenEntityCount;
                hideResultAggregate = hashU32(
                    hideResultAggregate,
                    Esp32ShowHideProbe_hideResultHash(&hideResult));
                hideStateAggregate = hashU32(hideStateAggregate, afterFNV);
                if (!audit->haveHide && hideResult.hiddenEntityCount != 0U) {
                    audit->hideDescriptor = descriptor;
                    audit->hideCommand = command;
                    audit->hideResult = hideResult;
                    audit->hideOffset = (uint8_t)commandOffset;
                    audit->haveHide = 1U;
                }
            }

            if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
            view = EspMapSpriteTopology_view();
            if (view == NULL || view->stateFNV1a != audit->initialStateFNV) return 0;
            ++audit->rollbackProofs;
        }
    }

    if (audit->refs == 0U || audit->showRefs == 0U || audit->hideRefs == 0U ||
        audit->showMutated == 0U || audit->hideMutated == 0U ||
        audit->hideEntitiesTotal == 0U ||
        audit->refs != audit->showRefs + audit->hideRefs ||
        audit->stateExecutorRefused != audit->refs ||
        audit->rollbackProofs != audit->refs || !audit->haveShow ||
        !audit->haveHide || !audit->haveUnsupported ||
        sizeof(EspMapShowResult) != EXPECTED_SHOW_RESULT_BYTES ||
        sizeof(EspMapHideResult) != EXPECTED_HIDE_RESULT_BYTES) return 0;

    audit->showResultFNV = showResultAggregate;
    audit->hideResultFNV = hideResultAggregate;
    audit->showStateFNV = showStateAggregate;
    audit->hideStateFNV = hideStateAggregate;

    /* SHOW is not safe to repeat on an already-linked target in legacy. The
     * native owner refuses that corrupting relink and otherwise proves no
     * second state mutation for a no-entity target. */
    if (!EspMapSpriteTopology_resetMutableFromRuntime() ||
        EspMapSpriteTopology_applyShow(&audit->showDescriptor,
                                       audit->showOffset,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_OK) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    afterFNV = view->stateFNV1a;
    memset(&repeatShow, 0xa5, sizeof(repeatShow));
    status = EspMapSpriteTopology_applyShow(&audit->showDescriptor,
                                            audit->showOffset,
                                            &repeatShow);
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != afterFNV) return 0;
    if (showResult.targetHasEntity) {
        if (status != ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED ||
            !Esp32ShowHideProbe_showResultIsZero(&repeatShow)) return 0;
    }
    else if (status != ESP_MAP_SPRITE_TOPOLOGY_OK) {
        return 0;
    }
    audit->showRepeatGuard = 1U;

    if (!EspMapSpriteTopology_resetMutableFromRuntime() ||
        EspMapSpriteTopology_applyHide(&audit->hideDescriptor,
                                       audit->hideOffset,
                                       &hideResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_OK) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    afterFNV = view->stateFNV1a;
    memset(&repeatHide, 0, sizeof(repeatHide));
    if (EspMapSpriteTopology_applyHide(&audit->hideDescriptor,
                                       audit->hideOffset,
                                       &repeatHide) !=
            ESP_MAP_SPRITE_TOPOLOGY_OK ||
        repeatHide.hiddenEntityCount != 0U || repeatHide.legacyReturnValue != 1U) {
        return 0;
    }
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != afterFNV) return 0;
    audit->hideIdempotent = 1U;

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != audit->initialStateFNV) return 0;
    audit->resetProof = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&audit->unsupportedDescriptor,
                                       audit->unsupportedOffset,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_UNSUPPORTED ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->unsupportedRefused = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&audit->showDescriptor,
                                       audit->showDescriptor.commandCount,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->showDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&badDescriptor, audit->showOffset,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->badDescriptorRefused = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(NULL, audit->showOffset,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->nullDescriptorRefused = 1U;

    if (EspMapSpriteTopology_applyShow(&audit->showDescriptor,
                                       audit->showOffset, NULL) !=
            ESP_MAP_SPRITE_TOPOLOGY_INVALID) return 0;
    audit->nullResultRefused = 1U;

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
    view = EspMapSpriteTopology_view();
    return view != NULL && view->stateFNV1a == audit->initialStateFNV;
}
