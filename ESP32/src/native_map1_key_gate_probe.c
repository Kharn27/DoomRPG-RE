#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_map_events.h"
#include "esp_map_key_gate.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_dialog_owner_probe.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
#include "native_map1_events_probe.h"
#include "native_map1_key_gate_probe.h"
#include "native_map1_notebook_probe.h"
#include "native_map1_opcode_exec_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_map1_status_message_probe.h"
#include "native_map1_string_reader_probe.h"
#include "native_map1_ui_intent_probe.h"
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
#define EXPECTED_STRING_COUNT 94U
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_RESULT_BYTES 12U
#define KEY_CONTEXT_COUNT 16U

typedef struct Esp32Map1KeyGateProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1KeyGateProbeState;

typedef struct KeyGateAudit_s {
    uint32_t gateFNV;
    uint32_t refs;
    uint32_t greenRefs;
    uint32_t yellowRefs;
    uint32_t blueRefs;
    uint32_t redRefs;
    uint32_t scenarios;
    uint32_t passScenarios;
    uint32_t blockedScenarios;
    uint32_t stateExecutorRefused;
    uint32_t messagesProof;
    uint32_t extraBitsIgnored;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullResultRefused;
    EspMapEventDescriptor sampleDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode sampleCommand;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
} KeyGateAudit;

static Esp32Map1KeyGateProbeState probeState;

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

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, data[i]);
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

static uint32_t hudWitnessHash(const Hud_t* hud) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (hud == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)hud->msgCount);
    hash = hashU32(hash, (uint32_t)hud->msgTime);
    hash = hashU32(hash, (uint32_t)hud->msgDuration);
    hash = hashU32(hash, (uint32_t)hud->isUpdate);
    hash = hashU32(hash, (uint32_t)(uintptr_t)hud->statBarMessage);
    for (i = 0U; i < (uint32_t)sizeof(hud->messages); ++i) {
        hash = hashByte(hash, ((const uint8_t*)hud->messages)[i]);
    }
    for (i = 0U; i < (uint32_t)sizeof(hud->logMessage); ++i) {
        hash = hashByte(hash, ((const uint8_t*)hud->logMessage)[i]);
    }
    return hash;
}

static uint32_t resultHash(const EspMapKeyGateResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->soundId);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->keyIndex);
    hash = hashByte(hash, result->requiredMask);
    hash = hashByte(hash, result->legacyReturnValue);
    hash = hashByte(hash, result->stopEvent);
    return hashByte(hash, result->saveCurrentCommand);
}

static int resultIsZero(const EspMapKeyGateResult* result) {
    EspMapKeyGateResult zero;

    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}

static const char* expectedMessage(uint8_t keyIndex) {
    switch (keyIndex) {
        case ESP_MAP_KEY_GREEN:  return "Need Green Key";
        case ESP_MAP_KEY_YELLOW: return "Need Yellow Key";
        case ESP_MAP_KEY_BLUE:   return "Need Blue Key";
        case ESP_MAP_KEY_RED:    return "Need Red Key";
        default:                 return NULL;
    }
}

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL && canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() && !EspNativeSpriteCache_isActive();
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32IntroDispose_isDone() && Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() && Esp32Map1AccessProbe_isDone() &&
           Esp32Map1StateProbe_isDone() && Esp32Map1EventsProbe_isDone() &&
           Esp32Map1EventDescriptorProbe_isDone() &&
           Esp32Map1EventFilterProbe_isDone() &&
           Esp32Map1OpcodeExecProbe_isDone() &&
           Esp32Map1UiIntentProbe_isDone() &&
           Esp32Map1StringReaderProbe_isDone() &&
           Esp32Map1StatusMessageProbe_isDone() &&
           Esp32Map1DialogOwnerProbe_isDone() &&
           Esp32Map1NotebookProbe_isDone() &&
           runtime != NULL && mapState != NULL && scriptState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           runtime->sourceCrc32 == EXPECTED_INTRO_BSP_CRC32 &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           runtime->stringCount == EXPECTED_STRING_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           fnv1a32(scriptState->storage, scriptState->storageBytes) ==
               EXPECTED_SCRIPT_FNV &&
           fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                   (uint32_t)sizeof(doomRpg->player->NotebookString)) ==
               EXPECTED_LEGACY_NOTEBOOK_FNV &&
           !EspAssetPack_isOpen() && !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0;
}

static int descriptorByIndex(uint32_t index,
                             EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t value;

    if (outDescriptor == NULL || index > 0xffffU ||
        !EspMapRuntime_getEvent(index, &value)) return 0;
    ref.index = (uint16_t)index;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int validateResult(const EspMapEventDescriptor* descriptor,
                          uint32_t commandOffset,
                          const EspMapByteCode* command,
                          uint32_t keyBits,
                          EspMapKeyGateStatus status,
                          const EspMapKeyGateResult* result) {
    uint32_t requiredMask;
    int blocked;
    const char* message;
    const char* expected;

    if (descriptor == NULL || command == NULL || result == NULL ||
        command->arg1 > ESP_MAP_KEY_RED) return 0;

    requiredMask = 1UL << command->arg1;
    blocked = (keyBits & requiredMask) == 0U;
    if (status != (blocked ? ESP_MAP_KEY_GATE_BLOCKED : ESP_MAP_KEY_GATE_PASS) ||
        result->sourceEventIndex != descriptor->eventIndex ||
        result->globalCommandIndex !=
            (uint16_t)((uint32_t)descriptor->firstCommandIndex + commandOffset) ||
        result->sourceCommandOffset != (uint8_t)commandOffset ||
        result->keyIndex != (uint8_t)command->arg1 ||
        result->requiredMask != (uint8_t)requiredMask) return 0;

    message = EspMapKeyGate_message(result);
    expected = expectedMessage((uint8_t)command->arg1);
    if (blocked) {
        return result->soundId == ESP_MAP_KEY_GATE_SOUND_ID &&
               result->legacyReturnValue == 1U && result->stopEvent == 1U &&
               result->saveCurrentCommand == 1U && message != NULL &&
               expected != NULL && strcmp(message, expected) == 0;
    }

    return result->soundId == 0U && result->legacyReturnValue == 0U &&
           result->stopEvent == 0U && result->saveCurrentCommand == 0U &&
           message == NULL;
}

static int auditKeyGates(KeyGateAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapKeyGateResult result;
    EspMapKeyGateResult synthetic;
    EspMapKeyGateStatus status;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t keyBits;
    uint32_t globalCommandIndex;
    uint32_t requiredMask;
    uint32_t expectedBlocked;
    uint32_t hash = 2166136261U;
    uint32_t color;
    const char* message;

    if (audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }

            if (!audit->haveUnsupported &&
                command.id != ESP_MAP_OPCODE_CHECK_KEY) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_CHECK_KEY) continue;
            if (command.arg1 > ESP_MAP_KEY_RED) return 0;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;

            switch ((uint8_t)command.arg1) {
                case ESP_MAP_KEY_GREEN:  ++audit->greenRefs; break;
                case ESP_MAP_KEY_YELLOW: ++audit->yellowRefs; break;
                case ESP_MAP_KEY_BLUE:   ++audit->blueRefs; break;
                case ESP_MAP_KEY_RED:    ++audit->redRefs; break;
                default: return 0;
            }

            if (!audit->haveSample) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleOffset = (uint8_t)commandOffset;
                audit->haveSample = 1U;
            }

            globalCommandIndex =
                (uint32_t)descriptor.firstCommandIndex + commandOffset;
            requiredMask = 1UL << command.arg1;
            for (keyBits = 0U; keyBits < KEY_CONTEXT_COUNT; ++keyBits) {
                memset(&result, 0xa5, sizeof(result));
                status = EspMapKeyGate_evaluate(&descriptor, commandOffset,
                                                keyBits, &result);
                if (!validateResult(&descriptor, commandOffset, &command,
                                    keyBits, status, &result)) return 0;

                expectedBlocked = (keyBits & requiredMask) == 0U;
                if (expectedBlocked) ++audit->blockedScenarios;
                else ++audit->passScenarios;
                ++audit->scenarios;

                hash = hashU16(hash, (uint16_t)globalCommandIndex);
                hash = hashU16(hash, descriptor.eventIndex);
                hash = hashByte(hash, (uint8_t)commandOffset);
                hash = hashU32(hash, command.arg1);
                hash = hashU32(hash, command.arg2);
                hash = hashByte(hash, (uint8_t)keyBits);
                hash = hashByte(hash, (uint8_t)status);
                hash = hashU32(hash, resultHash(&result));
            }
            ++audit->refs;
        }
    }

    if (audit->refs == 0U || !audit->haveSample || !audit->haveUnsupported ||
        audit->greenRefs + audit->yellowRefs + audit->blueRefs + audit->redRefs !=
            audit->refs ||
        audit->scenarios != audit->refs * KEY_CONTEXT_COUNT ||
        audit->passScenarios != audit->refs * (KEY_CONTEXT_COUNT / 2U) ||
        audit->blockedScenarios != audit->refs * (KEY_CONTEXT_COUNT / 2U) ||
        audit->stateExecutorRefused != audit->refs) return 0;

    audit->gateFNV = hash;

    for (color = 0U; color <= ESP_MAP_KEY_RED; ++color) {
        memset(&synthetic, 0, sizeof(synthetic));
        synthetic.keyIndex = (uint8_t)color;
        synthetic.soundId = ESP_MAP_KEY_GATE_SOUND_ID;
        synthetic.legacyReturnValue = 1U;
        synthetic.stopEvent = 1U;
        synthetic.saveCurrentCommand = 1U;
        message = EspMapKeyGate_message(&synthetic);
        if (message == NULL || expectedMessage((uint8_t)color) == NULL ||
            strcmp(message, expectedMessage((uint8_t)color)) != 0) return 0;
        ++audit->messagesProof;
    }

    requiredMask = 1UL << audit->sampleCommand.arg1;
    status = EspMapKeyGate_evaluate(&audit->sampleDescriptor,
                                    audit->sampleOffset,
                                    0xfffffff0UL,
                                    &result);
    if (!validateResult(&audit->sampleDescriptor, audit->sampleOffset,
                        &audit->sampleCommand, 0xfffffff0UL,
                        status, &result) || status != ESP_MAP_KEY_GATE_BLOCKED) {
        return 0;
    }
    status = EspMapKeyGate_evaluate(&audit->sampleDescriptor,
                                    audit->sampleOffset,
                                    0xfffffff0UL | requiredMask,
                                    &result);
    if (!validateResult(&audit->sampleDescriptor, audit->sampleOffset,
                        &audit->sampleCommand,
                        0xfffffff0UL | requiredMask,
                        status, &result) || status != ESP_MAP_KEY_GATE_PASS) {
        return 0;
    }
    audit->extraBitsIgnored = 1U;

    memset(&result, 0xa5, sizeof(result));
    status = EspMapKeyGate_evaluate(&audit->unsupportedDescriptor,
                                    audit->unsupportedOffset, 0U, &result);
    if (status != ESP_MAP_KEY_GATE_UNSUPPORTED || !resultIsZero(&result)) {
        return 0;
    }
    audit->unsupportedRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    status = EspMapKeyGate_evaluate(&audit->sampleDescriptor,
                                    audit->sampleDescriptor.commandCount,
                                    0U, &result);
    if (status != ESP_MAP_KEY_GATE_INVALID || !resultIsZero(&result)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.eventIndex = 0xffffU;
    memset(&result, 0xa5, sizeof(result));
    status = EspMapKeyGate_evaluate(&badDescriptor, audit->sampleOffset,
                                    0U, &result);
    if (status != ESP_MAP_KEY_GATE_INVALID || !resultIsZero(&result)) return 0;
    audit->badDescriptorRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    status = EspMapKeyGate_evaluate(NULL, audit->sampleOffset, 0U, &result);
    if (status != ESP_MAP_KEY_GATE_INVALID || !resultIsZero(&result)) return 0;
    audit->nullDescriptorRefused = 1U;

    status = EspMapKeyGate_evaluate(&audit->sampleDescriptor,
                                    audit->sampleOffset, 0U, NULL);
    if (status != ESP_MAP_KEY_GATE_INVALID) return 0;
    audit->nullResultRefused = 1U;

    return audit->messagesProof == 4U;
}

void Esp32Map1KeyGateProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32Map1KeyGateProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1KeyGateProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    KeyGateAudit audit;
    uint32_t heapBefore, heapAfter;
    uint32_t largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter;
    uint32_t scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter;
    uint32_t hudBefore, hudAfter;
    uint32_t started, elapsed;
    uint32_t sampleGlobal;
    uint32_t sampleMask;
    int keysBefore, keysAfter;
    int skipAdvanceTurnBefore;
    int saveTileEventBefore;
    int tileEventBefore;
    int tileEventIndexBefore;
    int tileEventFlagsBefore;

    if (probeState.done || probeState.attempted) return;
    if (!probeState.armed) {
        if (Esp32Map1NotebookProbe_isDone()) {
            probeState.armed = 1;
            printf("[MAPKEYPROBE] ARMED native NOTE owner proven; EV_CHECK_KEY pure gate evaluation starts on next loop service\n");
        }
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO CHECK_KEY gate ===\n");
    printf("[MAPKEYPROBE] CONTRACT evaluate only EV_CHECK_KEY over caller-supplied key bits -> PASS or BLOCKED metadata; no Player/Hud/Sound/Game/world/render mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPKEYPROBE] FAILED unsafe precondition\n");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = runtime->arenaFNV1a;
    mapStateBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    hudBefore = hudWitnessHash(doomRpg->hud);
    keysBefore = doomRpg->player->keys;
    skipAdvanceTurnBefore = doomRpg->game->skipAdvanceTurn;
    saveTileEventBefore = doomRpg->game->saveTileEvent;
    tileEventBefore = doomRpg->game->tileEvent;
    tileEventIndexBefore = doomRpg->game->tileEventIndex;
    tileEventFlagsBefore = doomRpg->game->tileEventFlags;
    started = DoomRPG_GetUpTimeMS();

    if (!auditKeyGates(&audit)) {
        printf("[MAPKEYPROBE] FAILED native CHECK_KEY audit\n");
        return;
    }

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ? runtime->arenaFNV1a : 0U;
    mapStateAfter = mapState != NULL ? mapState->stateFNV1a : 0U;
    scriptAfter = scriptState != NULL
                      ? fnv1a32(scriptState->storage, scriptState->storageBytes)
                      : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                            (uint32_t)sizeof(doomRpg->player->NotebookString));
    hudAfter = hudWitnessHash(doomRpg->hud);
    keysAfter = doomRpg->player->keys;

    if (audit.refs == 0U ||
        audit.greenRefs + audit.yellowRefs + audit.blueRefs + audit.redRefs !=
            audit.refs ||
        audit.scenarios != audit.refs * KEY_CONTEXT_COUNT ||
        audit.passScenarios != audit.refs * (KEY_CONTEXT_COUNT / 2U) ||
        audit.blockedScenarios != audit.refs * (KEY_CONTEXT_COUNT / 2U) ||
        audit.stateExecutorRefused != audit.refs ||
        audit.messagesProof != 4U || audit.extraBitsIgnored != 1U ||
        audit.unsupportedRefused != 1U || audit.badOffsetRefused != 1U ||
        audit.badDescriptorRefused != 1U || audit.nullDescriptorRefused != 1U ||
        audit.nullResultRefused != 1U ||
        sizeof(EspMapKeyGateResult) != EXPECTED_RESULT_BYTES ||
        EspAssetPack_isOpen() || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || mapStateAfter != mapStateBefore ||
        scriptAfter != scriptBefore || notebookBefore != EXPECTED_LEGACY_NOTEBOOK_FNV ||
        notebookAfter != notebookBefore || hudAfter != hudBefore ||
        keysAfter != keysBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceTurnBefore ||
        doomRpg->game->saveTileEvent != saveTileEventBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->doomCanvas->state != ST_INTRO) {
        printf("[MAPKEYPROBE] FAILED acceptance/integrity\n");
        return;
    }

    sampleGlobal = (uint32_t)audit.sampleDescriptor.firstCommandIndex +
                   (uint32_t)audit.sampleOffset;
    sampleMask = 1UL << audit.sampleCommand.arg1;

    printf("[MAPKEY] READY refs=%u green=%u yellow=%u blue=%u red=%u scenarios=%u pass=%u blocked=%u resultBytes=%u stateExecRefused=%u keyGateFNV=%08x elapsed=%ums\n",
           (unsigned)audit.refs, (unsigned)audit.greenRefs,
           (unsigned)audit.yellowRefs, (unsigned)audit.blueRefs,
           (unsigned)audit.redRefs, (unsigned)audit.scenarios,
           (unsigned)audit.passScenarios, (unsigned)audit.blockedScenarios,
           (unsigned)sizeof(EspMapKeyGateResult),
           (unsigned)audit.stateExecutorRefused,
           (unsigned)audit.gateFNV, (unsigned)elapsed);
    printf("[MAPKEY] SAMPLE cmd=%u event=%u off=%u key=%u mask=%02x arg2=%08x missingMessage=\"%s\" sound=%u saveOffset=current\n",
           (unsigned)sampleGlobal,
           (unsigned)audit.sampleDescriptor.eventIndex,
           (unsigned)audit.sampleOffset,
           (unsigned)audit.sampleCommand.arg1,
           (unsigned)sampleMask,
           (unsigned)audit.sampleCommand.arg2,
           expectedMessage((uint8_t)audit.sampleCommand.arg1),
           (unsigned)ESP_MAP_KEY_GATE_SOUND_ID);
    printf("[MAPKEY] TABLE perRef=16 passEach=8 blockedEach=8 messages=%u/4 extraBitsIgnored=%u passEffect=none blockedEffect=message+sound+stop+saveCurrent\n",
           (unsigned)audit.messagesProof, (unsigned)audit.extraBitsIgnored);
    printf("[MAPKEY] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullResult=%u\n",
           (unsigned)audit.unsupportedRefused,
           (unsigned)audit.badOffsetRefused,
           (unsigned)audit.badDescriptorRefused,
           (unsigned)audit.nullDescriptorRefused,
           (unsigned)audit.nullResultRefused);
    printf("[MAPKEYPROBE] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x legacyNotebookFNV=%08x->%08x legacyKeys=%08x->%08x hudFNV=%08x->%08x\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (int)heapAfter - (int)heapBefore,
           (unsigned)largestBefore, (unsigned)largestAfter,
           (int)largestAfter - (int)largestBefore,
           (unsigned)frameBefore, (unsigned)frameAfter,
           (unsigned)arenaBefore, (unsigned)arenaAfter,
           (unsigned)mapStateBefore, (unsigned)mapStateAfter,
           (unsigned)scriptBefore, (unsigned)scriptAfter,
           (unsigned)notebookBefore, (unsigned)notebookAfter,
           (unsigned)keysBefore, (unsigned)keysAfter,
           (unsigned)hudBefore, (unsigned)hudAfter);
    printf("[MAPKEYPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes resultBytes=%u persistentBytes=0 legacyKeyMutation=no legacyHudMutation=no legacyGameContinuationMutation=no worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)sizeof(EspMapKeyGateResult));

    probeState.done = 1;
}
