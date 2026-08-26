#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"

#include "esp_asset_pack.h"
#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "native_map1_change_map_probe.h"
#include "native_map1_show_hide_final_probe.h"
#include "native_map1_show_hide_probe_internal.h"

#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_STATE_FNV 0x669b1aa7U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_SPRITE_COUNT 344U
#define EXPECTED_TOPOLOGY_BYTES (EXPECTED_SPRITE_COUNT * 7U)
#define EXPECTED_SHOW_RESULT_BYTES 26U
#define EXPECTED_HIDE_RESULT_BYTES 18U
#define MAX_TOPOLOGY_HEAP_OVERHEAD 128U

typedef struct Esp32ShowHideFinalProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32ShowHideFinalProbeState;

typedef struct Esp32ShowHideFinalAudit_s {
    uint32_t refs;
    uint32_t showRefs;
    uint32_t hideRefs;
    uint32_t removableRefs;
    uint32_t stateExecRefused;
    uint32_t rollbackProofs;
    uint32_t showMutated;
    uint32_t hideMutated;
    uint32_t hideNoMutation;
    uint32_t showTargetEntities;
    uint32_t showTargetNoEntity;
    uint32_t blockersFound;
    uint32_t blockersRemoved;
    uint32_t blockerNoops;
    uint32_t deferredDeaths;
    uint32_t hideEntitiesIsolated;
    uint32_t showResultFNV;
    uint32_t hideResultFNV;
    uint32_t showStateFNV;
    uint32_t hideStateFNV;
    uint32_t initialFNV;
    uint32_t contextAfterShowFNV;
    uint32_t contextAfterHideFNV;
    uint32_t contextHidden;
    uint32_t contextSecondHidden;
    uint32_t showRepeatGuard;
    uint32_t hideContextProven;
    uint32_t hideIdempotent;
    uint32_t resetProof;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullResultRefused;
    EspMapEventDescriptor firstShowDescriptor;
    EspMapEventDescriptor hideDescriptor;
    EspMapEventDescriptor contextShowDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode firstShowCommand;
    EspMapByteCode hideCommand;
    EspMapByteCode contextShowCommand;
    EspMapShowResult firstShowResult;
    EspMapHideResult hideIsolatedResult;
    EspMapShowResult contextShowResult;
    EspMapHideResult contextHideResult;
    uint8_t firstShowOffset;
    uint8_t hideOffset;
    uint8_t contextShowOffset;
    uint8_t unsupportedOffset;
    uint8_t haveShow;
    uint8_t haveHide;
    uint8_t haveContextShow;
    uint8_t haveUnsupported;
} Esp32ShowHideFinalAudit;

static Esp32ShowHideFinalProbeState probeState;

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

static uint32_t hashU32(uint32_t hash, uint32_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, data[i]);
    return hash;
}

static int rawSpriteTile(uint32_t spriteIndex, uint16_t* outTile) {
    EspMapSprite sprite;
    uint32_t x;
    uint32_t y;
    if (outTile == NULL ||
        !EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;
    x = (uint32_t)sprite.x >> 6;
    y = (uint32_t)sprite.y >> 6;
    if (x >= 32U || y >= 32U) return 0;
    *outTile = (uint16_t)((y * 32U) + x);
    return 1;
}

static int findContextShow(const EspMapEventDescriptor* descriptor,
                           uint32_t hideOffset,
                           uint16_t hideTile,
                           Esp32ShowHideFinalAudit* audit) {
    EspMapByteCode command;
    uint16_t tile;
    uint32_t offset;
    if (descriptor == NULL || audit == NULL) return 0;
    for (offset = 0U; offset < hideOffset; ++offset) {
        if (!EspMapEvents_getCommand(descriptor, offset, &command) ||
            command.id != ESP_MAP_OPCODE_SHOW ||
            !rawSpriteTile(command.arg1 & 0xffffU, &tile) || tile != hideTile) {
            continue;
        }
        audit->contextShowDescriptor = *descriptor;
        audit->contextShowCommand = command;
        audit->contextShowOffset = (uint8_t)offset;
        audit->haveContextShow = 1U;
        return 1;
    }
    return 0;
}

static int resultShowSane(const EspMapEventDescriptor* descriptor,
                          uint32_t offset,
                          const EspMapByteCode* command,
                          const EspMapShowResult* result) {
    uint32_t global;
    if (descriptor == NULL || command == NULL || result == NULL) return 0;
    global = (uint32_t)descriptor->firstCommandIndex + offset;
    return global <= 0xffffU &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)global &&
           result->spriteIndex == (uint16_t)(command->arg1 & 0xffffU) &&
           result->sourceCommandOffset == (uint8_t)offset &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) &&
           result->targetLinkedBefore == 0U &&
           (result->effectFlags & ESP_MAP_SHOW_EFFECT_VISUAL_STATE) != 0U &&
           result->blockersFound <= 2U &&
           result->blockersRemoved <= result->blockersFound &&
           result->blockerNoops <= result->blockersFound &&
           (result->targetHasEntity == 0U ||
            (result->targetLinkedAfter == 1U &&
             (result->effectFlags & ESP_MAP_SHOW_EFFECT_LINK_TARGET) != 0U));
}

static int resultHideSane(const EspMapEventDescriptor* descriptor,
                          uint32_t offset,
                          const EspMapByteCode* command,
                          const EspMapHideResult* result) {
    uint32_t global;
    if (descriptor == NULL || command == NULL || result == NULL) return 0;
    global = (uint32_t)descriptor->firstCommandIndex + offset;
    return global <= 0xffffU &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)global &&
           result->sourceCommandOffset == (uint8_t)offset &&
           result->tileX == (uint8_t)(command->arg1 & 0xffU) &&
           result->tileY == (uint8_t)((command->arg1 >> 8) & 0xffU) &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) &&
           ((result->hiddenEntityCount == 0U && result->effectFlags == 0U) ||
            (result->hiddenEntityCount != 0U &&
             result->effectFlags ==
                 (ESP_MAP_HIDE_EFFECT_VISUAL_STATE | ESP_MAP_HIDE_EFFECT_UNLINK)));
}

static int auditCorpus(Esp32ShowHideFinalAudit* audit) {
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
    audit->initialFNV = view->stateFNV1a;

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!Esp32ShowHideProbe_descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) return 0;
            if (!audit->haveUnsupported && command.id != ESP_MAP_OPCODE_SHOW &&
                command.id != ESP_MAP_OPCODE_HIDE) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_SHOW && command.id != ESP_MAP_OPCODE_HIDE) {
                continue;
            }

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecRefused;
            ++audit->refs;
            if ((command.arg2 & ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U) {
                ++audit->removableRefs;
            }

            if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
            view = EspMapSpriteTopology_view();
            if (view == NULL || view->stateFNV1a != audit->initialFNV) return 0;
            beforeFNV = view->stateFNV1a;

            if (command.id == ESP_MAP_OPCODE_SHOW) {
                memset(&showResult, 0, sizeof(showResult));
                status = EspMapSpriteTopology_applyShow(&descriptor, commandOffset,
                                                        &showResult);
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    !resultShowSane(&descriptor, commandOffset, &command,
                                    &showResult)) return 0;
                view = EspMapSpriteTopology_view();
                if (view == NULL) return 0;
                afterFNV = view->stateFNV1a;
                ++audit->showRefs;
                if (afterFNV != beforeFNV) ++audit->showMutated;
                if (showResult.targetHasEntity != 0U) ++audit->showTargetEntities;
                else ++audit->showTargetNoEntity;
                audit->blockersFound += showResult.blockersFound;
                audit->blockersRemoved += showResult.blockersRemoved;
                audit->blockerNoops += showResult.blockerNoops;
                if ((showResult.effectFlags &
                     ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY) != 0U) {
                    ++audit->deferredDeaths;
                }
                showResultAggregate = hashU32(
                    showResultAggregate,
                    Esp32ShowHideProbe_showResultHash(&showResult));
                showStateAggregate = hashU32(showStateAggregate, afterFNV);
                if (!audit->haveShow) {
                    audit->firstShowDescriptor = descriptor;
                    audit->firstShowCommand = command;
                    audit->firstShowResult = showResult;
                    audit->firstShowOffset = (uint8_t)commandOffset;
                    audit->haveShow = 1U;
                }
            }
            else {
                memset(&hideResult, 0, sizeof(hideResult));
                status = EspMapSpriteTopology_applyHide(&descriptor, commandOffset,
                                                        &hideResult);
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    !resultHideSane(&descriptor, commandOffset, &command,
                                    &hideResult)) return 0;
                view = EspMapSpriteTopology_view();
                if (view == NULL) return 0;
                afterFNV = view->stateFNV1a;
                ++audit->hideRefs;
                if (afterFNV != beforeFNV) ++audit->hideMutated;
                else ++audit->hideNoMutation;
                audit->hideEntitiesIsolated += hideResult.hiddenEntityCount;
                hideResultAggregate = hashU32(
                    hideResultAggregate,
                    Esp32ShowHideProbe_hideResultHash(&hideResult));
                hideStateAggregate = hashU32(hideStateAggregate, afterFNV);
                if (!audit->haveHide) {
                    audit->hideDescriptor = descriptor;
                    audit->hideCommand = command;
                    audit->hideIsolatedResult = hideResult;
                    audit->hideOffset = (uint8_t)commandOffset;
                    audit->haveHide = 1U;
                    (void)findContextShow(&descriptor, commandOffset,
                                          hideResult.tileIndex, audit);
                }
            }

            if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
            view = EspMapSpriteTopology_view();
            if (view == NULL || view->stateFNV1a != audit->initialFNV) return 0;
            ++audit->rollbackProofs;
        }
    }

    if (audit->refs == 0U || audit->showRefs == 0U || audit->hideRefs == 0U ||
        audit->showMutated == 0U ||
        audit->refs != audit->showRefs + audit->hideRefs ||
        audit->stateExecRefused != audit->refs ||
        audit->rollbackProofs != audit->refs || !audit->haveShow ||
        !audit->haveHide || !audit->haveContextShow || !audit->haveUnsupported ||
        sizeof(EspMapShowResult) != EXPECTED_SHOW_RESULT_BYTES ||
        sizeof(EspMapHideResult) != EXPECTED_HIDE_RESULT_BYTES) return 0;

    audit->showResultFNV = showResultAggregate;
    audit->hideResultFNV = hideResultAggregate;
    audit->showStateFNV = showStateAggregate;
    audit->hideStateFNV = hideStateAggregate;

    /* SHOW repeat is deliberately guarded instead of corrupting the tile chain. */
    if (!EspMapSpriteTopology_resetMutableFromRuntime() ||
        EspMapSpriteTopology_applyShow(&audit->firstShowDescriptor,
                                       audit->firstShowOffset,
                                       &showResult) != ESP_MAP_SPRITE_TOPOLOGY_OK) {
        return 0;
    }
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    afterFNV = view->stateFNV1a;
    memset(&repeatShow, 0xa5, sizeof(repeatShow));
    status = EspMapSpriteTopology_applyShow(&audit->firstShowDescriptor,
                                            audit->firstShowOffset,
                                            &repeatShow);
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != afterFNV) return 0;
    if (showResult.targetHasEntity != 0U) {
        if (status != ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED ||
            !Esp32ShowHideProbe_showResultIsZero(&repeatShow)) return 0;
    }
    else if (status != ESP_MAP_SPRITE_TOPOLOGY_OK) return 0;
    audit->showRepeatGuard = 1U;

    /* The sole real HIDE is a valid no-op from source state. Prove its real
     * mutation semantics by composing it with the earlier real SHOW on the
     * same event/tile, then prove the second HIDE is handled/idempotent. */
    if (!EspMapSpriteTopology_resetMutableFromRuntime() ||
        EspMapSpriteTopology_applyShow(&audit->contextShowDescriptor,
                                       audit->contextShowOffset,
                                       &audit->contextShowResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_OK) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    audit->contextAfterShowFNV = view->stateFNV1a;

    memset(&audit->contextHideResult, 0, sizeof(audit->contextHideResult));
    if (EspMapSpriteTopology_applyHide(&audit->hideDescriptor,
                                       audit->hideOffset,
                                       &audit->contextHideResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_OK ||
        audit->contextHideResult.hiddenEntityCount == 0U ||
        !resultHideSane(&audit->hideDescriptor, audit->hideOffset,
                        &audit->hideCommand, &audit->contextHideResult)) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL) return 0;
    audit->contextAfterHideFNV = view->stateFNV1a;
    audit->contextHidden = audit->contextHideResult.hiddenEntityCount;
    if (audit->contextAfterHideFNV == audit->contextAfterShowFNV) return 0;
    audit->hideContextProven = 1U;

    memset(&repeatHide, 0, sizeof(repeatHide));
    if (EspMapSpriteTopology_applyHide(&audit->hideDescriptor,
                                       audit->hideOffset,
                                       &repeatHide) != ESP_MAP_SPRITE_TOPOLOGY_OK ||
        repeatHide.hiddenEntityCount != 0U || repeatHide.legacyReturnValue != 1U) {
        return 0;
    }
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != audit->contextAfterHideFNV) return 0;
    audit->contextSecondHidden = repeatHide.hiddenEntityCount;
    audit->hideIdempotent = 1U;

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
    view = EspMapSpriteTopology_view();
    if (view == NULL || view->stateFNV1a != audit->initialFNV) return 0;
    audit->resetProof = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&audit->unsupportedDescriptor,
                                       audit->unsupportedOffset,
                                       &showResult) !=
            ESP_MAP_SPRITE_TOPOLOGY_UNSUPPORTED ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->unsupportedRefused = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&audit->firstShowDescriptor,
                                       audit->firstShowDescriptor.commandCount,
                                       &showResult) != ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->firstShowDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(&badDescriptor,
                                       audit->firstShowOffset,
                                       &showResult) != ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->badDescriptorRefused = 1U;

    memset(&showResult, 0xa5, sizeof(showResult));
    if (EspMapSpriteTopology_applyShow(NULL, audit->firstShowOffset,
                                       &showResult) != ESP_MAP_SPRITE_TOPOLOGY_INVALID ||
        !Esp32ShowHideProbe_showResultIsZero(&showResult)) return 0;
    audit->nullDescriptorRefused = 1U;

    if (EspMapSpriteTopology_applyShow(&audit->firstShowDescriptor,
                                       audit->firstShowOffset, NULL) !=
            ESP_MAP_SPRITE_TOPOLOGY_INVALID) return 0;
    audit->nullResultRefused = 1U;

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
    view = EspMapSpriteTopology_view();
    return view != NULL && view->stateFNV1a == audit->initialFNV;
}

void Esp32Map1ShowHideFinalProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapSpriteTopology_reset();
}

int Esp32Map1ShowHideFinalProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1ShowHideFinalProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapAutomapStateView* automapState;
    const EspMapSpriteTopologyView* topology;
    EspAssetPackEntry entityDefsEntry;
    Esp32ShowHideTopologyAudit topologyAudit;
    Esp32ShowHideFinalAudit audit;
    uint32_t heapBefore, heapOpen, heapAfterBuild, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfterBuild, largestAfter;
    uint32_t frameBefore, frameAfter, arenaBefore, arenaAfter;
    uint32_t mapBefore, mapAfter, scriptBefore, scriptAfter;
    uint32_t automapBefore, automapAfter, notebookBefore, notebookAfter;
    uint32_t keysBefore, keysAfter, hudBefore, hudAfter;
    uint32_t passwordBefore, passwordAfter, continuationBefore, continuationAfter;
    uint32_t legacyTopologyBefore, legacyTopologyAfter;
    uint32_t persistentHeapCost, startedMs, elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL ||
        !Esp32Map1ChangeMapProbe_isDone()) return;
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPSHOWHIDEFINAL] ARMED corrected final SHOW/HIDE proof starts on next loop service\n");
        return;
    }
    probeState.attempted = 1;

    printf("\n=== Doom RPG ESP32-native MAP_INTRO SHOW/HIDE final topology ===\n");
    printf("[MAPSHOWHIDEFINAL] CONTRACT isolate all real SHOW/HIDE commands from source state, accept handled source-state HIDE no-op, then compose the earlier same-event/same-tile SHOW before HIDE to prove real hide/unlink mutation; rollback exact; no legacy Entity/Render/gameplay mutation\n");

    if (!Esp32ShowHideProbe_boundaryIsSafe(doomRpg)) {
        printf("[MAPSHOWHIDEFINAL] FAILED unsafe precondition\n");
        return;
    }
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        automapState == NULL) {
        printf("[MAPSHOWHIDEFINAL] FAILED prerequisites\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = Esp32ShowHideProbe_framebufferHash();
    arenaBefore = runtime->arenaFNV1a;
    mapBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    automapBefore = automapState->stateFNV1a;
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                              (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = Esp32ShowHideProbe_hudHash(doomRpg->hud);
    passwordBefore = Esp32ShowHideProbe_passwordHash(doomRpg->doomCanvas);
    continuationBefore = Esp32ShowHideProbe_continuationHash(doomRpg->game);
    legacyTopologyBefore = Esp32ShowHideProbe_legacyTopologyHash(doomRpg->game);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPSHOWHIDEFINAL] FAILED pack open\n");
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();
    if (!EspAssetPack_findEntry("/entities.db", &entityDefsEntry) ||
        (entityDefsEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspMapSpriteTopology_buildFromRuntime(&entityDefsEntry)) {
        EspAssetPack_close();
        printf("[MAPSHOWHIDEFINAL] FAILED topology build\n");
        return;
    }
    EspAssetPack_close();
    heapAfterBuild = heap8Free();
    largestAfterBuild = largest8Block();

    topology = EspMapSpriteTopology_view();
    memset(&topologyAudit, 0, sizeof(topologyAudit));
    if (topology == NULL || topology->storageBytes != EXPECTED_TOPOLOGY_BYTES ||
        sizeof(EspMapShowResult) != EXPECTED_SHOW_RESULT_BYTES ||
        sizeof(EspMapHideResult) != EXPECTED_HIDE_RESULT_BYTES ||
        !Esp32ShowHideProbe_auditInitial(doomRpg, &topologyAudit) ||
        !auditCorpus(&audit)) {
        printf("[MAPSHOWHIDEFINAL] FAILED corrected topology/corpus proof\n");
        return;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    persistentHeapCost = heapBefore >= heapAfterBuild ? heapBefore - heapAfterBuild : 0U;

    frameAfter = Esp32ShowHideProbe_framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    topology = EspMapSpriteTopology_view();
    arenaAfter = runtime != NULL ? runtime->arenaFNV1a : 0U;
    mapAfter = mapState != NULL ? mapState->stateFNV1a : 0U;
    scriptAfter = scriptState != NULL
                      ? fnv1a32(scriptState->storage, scriptState->storageBytes)
                      : 0U;
    automapAfter = automapState != NULL ? automapState->stateFNV1a : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = Esp32ShowHideProbe_hudHash(doomRpg->hud);
    passwordAfter = Esp32ShowHideProbe_passwordHash(doomRpg->doomCanvas);
    continuationAfter = Esp32ShowHideProbe_continuationHash(doomRpg->game);
    legacyTopologyAfter = Esp32ShowHideProbe_legacyTopologyHash(doomRpg->game);

    /* The topology owner is 2408 bytes plus bounded allocator overhead. Its
     * fragmentation proof must be relative to this build, not to an absolute
     * free-block floor captured by an older firmware. */
    if (topology == NULL || topology->stateFNV1a != audit.initialFNV ||
        topology->stateFNV1a != topologyAudit.topologyFNV ||
        persistentHeapCost < EXPECTED_TOPOLOGY_BYTES ||
        persistentHeapCost > EXPECTED_TOPOLOGY_BYTES + MAX_TOPOLOGY_HEAP_OVERHEAD ||
        heapAfter != heapAfterBuild || largestAfter != largestAfterBuild ||
        (largestBefore > largestAfterBuild &&
         (largestBefore - largestAfterBuild) > persistentHeapCost) ||
        frameAfter != frameBefore ||
        arenaAfter != arenaBefore || arenaAfter != EXPECTED_ARENA_FNV ||
        mapAfter != mapBefore || mapAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV ||
        automapAfter != automapBefore || automapAfter != EXPECTED_AUTOMAP_STATE_FNV ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        EspMapLineTextureState_view() == NULL ||
        EspMapLineTextureState_view()->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        notebookAfter != notebookBefore || notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV ||
        keysAfter != keysBefore || hudAfter != hudBefore ||
        passwordAfter != passwordBefore || continuationAfter != continuationBefore ||
        legacyTopologyAfter != legacyTopologyBefore || EspAssetPack_isOpen() ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPSHOWHIDEFINAL] FAILED integrity regression\n");
        return;
    }

    printf("[MAPTOPOLOGY] READY sprites=%u storageBytes=%u defCount=%u entities=%u hasDef=%u fallback=%u linked=%u hiddenSprites=%u hiddenEntities=%u enemies=%u destructibles=%u nextOrder=%u stateFNV=%08x\n",
           (unsigned int)topology->spriteCount,
           (unsigned int)topology->storageBytes,
           (unsigned int)topology->entityDefCount,
           (unsigned int)topologyAudit.entityCount,
           (unsigned int)topologyAudit.hasDefCount,
           (unsigned int)topologyAudit.fallbackCount,
           (unsigned int)topologyAudit.linkedCount,
           (unsigned int)topology->hiddenCount,
           (unsigned int)topologyAudit.hiddenEntityCount,
           (unsigned int)topologyAudit.enemyCount,
           (unsigned int)topologyAudit.destructibleCount,
           (unsigned int)topology->nextLinkOrder,
           (unsigned int)topology->stateFNV1a);
    printf("[MAPSHOWHIDE] READY refs=%u show=%u hide=%u removable=%u stateExecRefused=%u showMutated=%u hideMutatedIsolated=%u hideNoMutation=%u showTargetEnt=%u showTargetNoEnt=%u blockersFound=%u blockersRemoved=%u blockerNoops=%u deferredDeaths=%u hideEntitiesIsolated=%u showResultBytes=%u hideResultBytes=%u showResultFNV=%08x hideResultFNV=%08x showStateFNV=%08x hideStateFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs, (unsigned int)audit.showRefs,
           (unsigned int)audit.hideRefs, (unsigned int)audit.removableRefs,
           (unsigned int)audit.stateExecRefused, (unsigned int)audit.showMutated,
           (unsigned int)audit.hideMutated, (unsigned int)audit.hideNoMutation,
           (unsigned int)audit.showTargetEntities,
           (unsigned int)audit.showTargetNoEntity,
           (unsigned int)audit.blockersFound, (unsigned int)audit.blockersRemoved,
           (unsigned int)audit.blockerNoops, (unsigned int)audit.deferredDeaths,
           (unsigned int)audit.hideEntitiesIsolated,
           (unsigned int)sizeof(EspMapShowResult),
           (unsigned int)sizeof(EspMapHideResult),
           (unsigned int)audit.showResultFNV,
           (unsigned int)audit.hideResultFNV,
           (unsigned int)audit.showStateFNV,
           (unsigned int)audit.hideStateFNV,
           (unsigned int)elapsedMs);
    printf("[MAPSHOW] SAMPLE cmd=%u event=%u off=%u sprite=%u tile=%u blockers=%u removed=%u effects=%04x handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.firstShowResult.globalCommandIndex,
           (unsigned int)audit.firstShowResult.sourceEventIndex,
           (unsigned int)audit.firstShowResult.sourceCommandOffset,
           (unsigned int)audit.firstShowResult.spriteIndex,
           (unsigned int)audit.firstShowResult.tileIndex,
           (unsigned int)audit.firstShowResult.blockersFound,
           (unsigned int)audit.firstShowResult.blockersRemoved,
           (unsigned int)audit.firstShowResult.effectFlags,
           (unsigned int)audit.firstShowResult.legacyReturnValue,
           (unsigned int)audit.firstShowResult.removeCommandIfHandled);
    printf("[MAPHIDE] ISOLATED cmd=%u event=%u off=%u tile=%u,%u index=%u hidden=%u effects=%02x handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.hideIsolatedResult.globalCommandIndex,
           (unsigned int)audit.hideIsolatedResult.sourceEventIndex,
           (unsigned int)audit.hideIsolatedResult.sourceCommandOffset,
           (unsigned int)audit.hideIsolatedResult.tileX,
           (unsigned int)audit.hideIsolatedResult.tileY,
           (unsigned int)audit.hideIsolatedResult.tileIndex,
           (unsigned int)audit.hideIsolatedResult.hiddenEntityCount,
           (unsigned int)audit.hideIsolatedResult.effectFlags,
           (unsigned int)audit.hideIsolatedResult.legacyReturnValue,
           (unsigned int)audit.hideIsolatedResult.removeCommandIfHandled);
    printf("[MAPHIDE] CONTEXT showCmd=%u hideCmd=%u event=%u tile=%u afterShowFNV=%08x afterHideFNV=%08x hidden=%u first=%u last=%u effects=%02x secondHidden=%u contextProven=%u idempotent=%u\n",
           (unsigned int)((uint32_t)audit.contextShowDescriptor.firstCommandIndex +
                          audit.contextShowOffset),
           (unsigned int)audit.contextHideResult.globalCommandIndex,
           (unsigned int)audit.contextHideResult.sourceEventIndex,
           (unsigned int)audit.contextHideResult.tileIndex,
           (unsigned int)audit.contextAfterShowFNV,
           (unsigned int)audit.contextAfterHideFNV,
           (unsigned int)audit.contextHidden,
           (unsigned int)audit.contextHideResult.firstHiddenSpriteIndex,
           (unsigned int)audit.contextHideResult.lastHiddenSpriteIndex,
           (unsigned int)audit.contextHideResult.effectFlags,
           (unsigned int)audit.contextSecondHidden,
           (unsigned int)audit.hideContextProven,
           (unsigned int)audit.hideIdempotent);
    printf("[MAPSHOWHIDE] STATE initialFNV=%08x rollback=%u/%u showRepeatGuard=%u hideContext=%u hideIdempotent=%u reset=%u worldRestored=yes\n",
           (unsigned int)audit.initialFNV,
           (unsigned int)audit.rollbackProofs, (unsigned int)audit.refs,
           (unsigned int)audit.showRepeatGuard,
           (unsigned int)audit.hideContextProven,
           (unsigned int)audit.hideIdempotent,
           (unsigned int)audit.resetProof);
    printf("[MAPSHOWHIDE] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullResult=%u randomCrateReal=0 targetRelink=guarded stateAtomic=yes\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullResultRefused);
    printf("[MAPSHOWHIDEFINAL] IO entityDefs=/entities.db size=%u crc32=%08x heapOpen=%u transientPackCost=%u largestOpen=%u packIO=yes buildOnly=yes executorPackIO=no\n",
           (unsigned int)entityDefsEntry.size,
           (unsigned int)entityDefsEntry.crc32,
           (unsigned int)heapOpen,
           (unsigned int)(heapBefore >= heapOpen ? heapBefore - heapOpen : 0U),
           (unsigned int)largestOpen);
    printf("[MAPSHOWHIDEFINAL] RAM heap8=%u->%u persistentHeapCost=%u payload=%u allocatorOverhead=%u largest8=%u->%u frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x automapFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)persistentHeapCost,
           (unsigned int)EXPECTED_TOPOLOGY_BYTES,
           (unsigned int)(persistentHeapCost - EXPECTED_TOPOLOGY_BYTES),
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapBefore, (unsigned int)mapAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter,
           (unsigned int)automapBefore, (unsigned int)automapAfter);
    printf("[MAPSHOWHIDEFINAL] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x entityTopologyFNV=%08x->%08x legacyRuntimeClear=yes\n",
           (unsigned int)notebookBefore, (unsigned int)notebookAfter,
           (unsigned int)keysBefore, (unsigned int)keysAfter,
           (unsigned int)hudBefore, (unsigned int)hudAfter,
           (unsigned int)passwordBefore, (unsigned int)passwordAfter,
           (unsigned int)continuationBefore, (unsigned int)continuationAfter,
           (unsigned int)legacyTopologyBefore,
           (unsigned int)legacyTopologyAfter);
    printf("[MAPSHOWHIDEFINAL] PARK state=%d page=%d nativeSpriteTopology=yes nativeShowHideExec=yes topologyBytes=%u showResultBytes=%u hideResultBytes=%u allMapIntroOpcodeFamiliesOwned=yes worldMutationProven=yes worldRestored=yes legacyEntityMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)topology->storageBytes,
           (unsigned int)sizeof(EspMapShowResult),
           (unsigned int)sizeof(EspMapHideResult),
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);

    probeState.done = 1;
}
