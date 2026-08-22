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
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
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
#include "native_map1_line_door_probe.h"
#include "native_map1_notebook_probe.h"
#include "native_map1_opcode_exec_probe.h"
#include "native_map1_password_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_map1_status_message_probe.h"
#include "native_map1_string_reader_probe.h"
#include "native_map1_ui_intent_probe.h"
#include "native_map1_unlock_probe.h"
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
#define EXPECTED_LINE_COUNT 480U
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_LINE_STATE_BYTES 120U
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_LINE_OPEN_COUNT 0U
#define EXPECTED_LINE_LOCKED_COUNT 7U
#define EXPECTED_TEXTURE_STATE_BYTES 60U
#define EXPECTED_RESULT_BYTES 20U
#define MAX_ALLOCATOR_OVERHEAD 64U
#define MIN_LARGEST8_AFTER_STATE 32768U

typedef struct Esp32Map1UnlockProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1UnlockProbeState;

typedef struct UnlockAudit_s {
    uint32_t unlockFNV;
    uint32_t refs;
    uint32_t mutatedRefs;
    uint32_t lockMutatedRefs;
    uint32_t textureMutatedRefs;
    uint32_t noMutationRefs;
    uint32_t removableRefs;
    uint32_t stateExecutorRefused;
    uint32_t rollbackProofs;
    uint32_t idempotentHandledProof;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullResultRefused;
    uint32_t badTextureIndexRefused;
    uint32_t badTextureValueRefused;
    uint32_t nonVariantRefused;
    EspMapEventDescriptor sampleDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode sampleCommand;
    EspMapLineUnlockResult sampleResult;
    uint32_t sampleMutatedLineFNV;
    uint32_t sampleMutatedTextureFNV;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
} UnlockAudit;

static Esp32Map1UnlockProbeState probeState;

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

static uint32_t canvasPasswordWitnessHash(const DoomCanvas_t* canvas) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (canvas == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)canvas->passwordTime);
    hash = hashByte(hash, (uint8_t)canvas->passInput);
    for (i = 0U; i < (uint32_t)sizeof(canvas->passCode); ++i) {
        hash = hashByte(hash, (uint8_t)canvas->passCode[i]);
    }
    for (i = 0U; i < (uint32_t)sizeof(canvas->strPassCode); ++i) {
        hash = hashByte(hash, (uint8_t)canvas->strPassCode[i]);
    }
    return hash;
}

static uint32_t gameContinuationHash(const Game_t* game) {
    uint32_t hash = 2166136261U;

    if (game == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)game->skipAdvanceTurn);
    hash = hashU32(hash, (uint32_t)game->saveTileEvent);
    hash = hashU32(hash, (uint32_t)game->tileEvent);
    hash = hashU32(hash, (uint32_t)game->tileEventIndex);
    hash = hashU32(hash, (uint32_t)game->tileEventFlags);
    return hashU32(hash, (uint32_t)(uintptr_t)game->passCode);
}

static uint32_t resultHash(const EspMapLineUnlockResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->lineIndex);
    hash = hashU16(hash, result->soundId);
    hash = hashU16(hash, result->textureBefore);
    hash = hashU16(hash, result->textureAfter);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->lockedBefore);
    hash = hashByte(hash, result->lockedAfter);
    hash = hashByte(hash, result->lockMutated);
    hash = hashByte(hash, result->textureMutated);
    hash = hashByte(hash, result->effectFlags);
    hash = hashByte(hash, result->legacyReturnValue);
    return hashByte(hash, result->removeCommandIfHandled);
}

static int resultIsZero(const EspMapLineUnlockResult* result) {
    EspMapLineUnlockResult zero;

    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
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

static int findFirstCommand(uint8_t codeId,
                            EspMapEventDescriptor* outDescriptor,
                            uint8_t* outOffset,
                            EspMapByteCode* outCommand) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    uint32_t eventIndex;
    uint32_t commandOffset;

    if (outDescriptor == NULL || outOffset == NULL || outCommand == NULL) return 0;
    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) return 0;
            if (command.id == codeId) {
                *outDescriptor = descriptor;
                *outOffset = (uint8_t)commandOffset;
                *outCommand = command;
                return 1;
            }
        }
    }
    return 0;
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32Map1LineDoorProbe_isDone() && runtime != NULL &&
           mapState != NULL && scriptState != NULL && lineState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           runtime->sourceCrc32 == EXPECTED_INTRO_BSP_CRC32 &&
           runtime->lineCount == EXPECTED_LINE_COUNT &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           fnv1a32(scriptState->storage, scriptState->storageBytes) ==
               EXPECTED_SCRIPT_FNV &&
           lineState->storageBytes == EXPECTED_LINE_STATE_BYTES &&
           lineState->stateFNV1a == EXPECTED_LINE_STATE_FNV &&
           lineState->openCount == EXPECTED_LINE_OPEN_COUNT &&
           lineState->lockedCount == EXPECTED_LINE_LOCKED_COUNT &&
           fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                   (uint32_t)sizeof(doomRpg->player->NotebookString)) ==
               EXPECTED_LEGACY_NOTEBOOK_FNV &&
           !EspMapLineTextureState_isReady() && !EspAssetPack_isOpen() &&
           !Esp32IntroClock_isActive() && !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) && legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0;
}

static int validateTextureState(const EspMapLineTextureStateView* state,
                                uint32_t* outFNV,
                                uint32_t* outVariants,
                                uint32_t* outTexture10) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t hash = 2166136261U;
    uint32_t variants = 0U;
    uint32_t texture10 = 0U;
    uint32_t i;
    uint16_t effective;

    if (state == NULL || runtime == NULL || outFNV == NULL ||
        outVariants == NULL || outTexture10 == NULL ||
        state->lineCount != EXPECTED_LINE_COUNT ||
        state->bitsetBytes != EXPECTED_TEXTURE_STATE_BYTES ||
        state->storageBytes != EXPECTED_TEXTURE_STATE_BYTES ||
        state->texture10Bits == NULL) return 0;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line) ||
            !EspMapLineTextureState_getEffectiveTexture(i, &effective) ||
            effective != line.texture) return 0;
        if (line.texture == ESP_MAP_LINE_TEXTURE_LOCKED ||
            line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) {
            ++variants;
            if (line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) ++texture10;
        }
    }
    if (EspMapLineTextureState_getEffectiveTexture(runtime->lineCount,
                                                   &effective)) return 0;

    for (i = 0U; i < state->storageBytes; ++i) {
        hash = hashByte(hash, state->texture10Bits[i]);
    }
    if (state->stateFNV1a != hash || state->variantCount != variants ||
        state->texture10Count != texture10) return 0;

    *outFNV = hash;
    *outVariants = variants;
    *outTexture10 = texture10;
    return 1;
}

static int validateUnlockResult(const EspMapEventDescriptor* descriptor,
                                uint32_t commandOffset,
                                const EspMapByteCode* command,
                                uint8_t lockedBefore,
                                uint16_t textureBefore,
                                EspMapLineUnlockStatus status,
                                const EspMapLineUnlockResult* result) {
    uint32_t globalCommandIndex;
    uint8_t expectedLockMutated;
    uint8_t expectedTextureMutated;
    uint16_t expectedTextureAfter;

    if (descriptor == NULL || command == NULL || result == NULL) return 0;
    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    expectedLockMutated = lockedBefore != 0U ? 1U : 0U;
    expectedTextureMutated =
        textureBefore == ESP_MAP_LINE_TEXTURE_LOCKED ? 1U : 0U;
    expectedTextureAfter = expectedTextureMutated != 0U
                               ? ESP_MAP_LINE_TEXTURE_UNLOCKED
                               : textureBefore;

    return status == ESP_MAP_LINE_UNLOCK_OK &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->lineIndex == (uint16_t)command->arg1 &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->lockedBefore == lockedBefore && result->lockedAfter == 0U &&
           result->lockMutated == expectedLockMutated &&
           result->textureBefore == textureBefore &&
           result->textureAfter == expectedTextureAfter &&
           result->textureMutated == expectedTextureMutated &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 & ESP_MAP_COMMAND_FLAG_REMOVE) != 0U) &&
           result->soundId == (expectedTextureMutated != 0U
                                  ? ESP_MAP_LINE_UNLOCK_SOUND
                                  : 0U) &&
           result->effectFlags == (expectedTextureMutated != 0U
                                      ? ESP_MAP_LINE_UNLOCK_EFFECT_ALL
                                      : 0U);
}

static int auditUnlockCommands(UnlockAudit* audit,
                               uint32_t initialLineFNV,
                               uint32_t initialTextureFNV) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapLineUnlockResult result;
    EspMapLineUnlockResult secondResult;
    EspMapLineUnlockStatus status;
    EspMapLine line;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t hash = 2166136261U;
    uint32_t lineAfterFNV;
    uint32_t textureAfterFNV;
    uint32_t nonVariantLine = EXPECTED_LINE_COUNT;
    uint8_t lockedBefore;
    uint8_t lockedAfter;
    uint16_t textureBefore;
    uint16_t textureAfter;

    if (audit == NULL || runtime == NULL) return 0;
    memset(audit, 0, sizeof(*audit));

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) return 0;
            if (!audit->haveUnsupported && command.id != ESP_MAP_OPCODE_UNLOCK) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_UNLOCK) continue;

            if (command.arg1 >= EXPECTED_LINE_COUNT ||
                !EspMapLineState_getLocked(command.arg1, &lockedBefore) ||
                !EspMapLineTextureState_getEffectiveTexture(command.arg1,
                                                            &textureBefore)) return 0;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;
            ++audit->refs;

            if (EspMapLineState_view() == NULL ||
                EspMapLineState_view()->stateFNV1a != initialLineFNV ||
                EspMapLineTextureState_view() == NULL ||
                EspMapLineTextureState_view()->stateFNV1a != initialTextureFNV) return 0;

            status = EspMapLineTextureState_applyUnlockCommand(
                &descriptor, commandOffset, &result);
            if (!validateUnlockResult(&descriptor, commandOffset, &command,
                                      lockedBefore, textureBefore, status,
                                      &result)) return 0;

            if (result.lockMutated != 0U) ++audit->lockMutatedRefs;
            if (result.textureMutated != 0U) ++audit->textureMutatedRefs;
            if (result.lockMutated != 0U || result.textureMutated != 0U) {
                ++audit->mutatedRefs;
            }
            else {
                ++audit->noMutationRefs;
            }
            if (result.removeCommandIfHandled != 0U) ++audit->removableRefs;

            if (!EspMapLineState_getLocked(command.arg1, &lockedAfter) ||
                !EspMapLineTextureState_getEffectiveTexture(command.arg1,
                                                            &textureAfter) ||
                lockedAfter != 0U || textureAfter != result.textureAfter) return 0;

            lineAfterFNV = EspMapLineState_view()->stateFNV1a;
            textureAfterFNV = EspMapLineTextureState_view()->stateFNV1a;
            if ((result.lockMutated != 0U) != (lineAfterFNV != initialLineFNV) ||
                (result.textureMutated != 0U) !=
                    (textureAfterFNV != initialTextureFNV)) return 0;

            if ((result.lockMutated != 0U || result.textureMutated != 0U) &&
                !audit->haveSample) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleResult = result;
                audit->sampleOffset = (uint8_t)commandOffset;
                audit->sampleMutatedLineFNV = lineAfterFNV;
                audit->sampleMutatedTextureFNV = textureAfterFNV;
                audit->haveSample = 1U;
            }

            if (result.textureMutated != 0U &&
                !EspMapLineTextureState_setDoorTexture(command.arg1,
                                                       textureBefore)) return 0;
            if (result.lockMutated != 0U &&
                !EspMapLineState_setLocked(command.arg1, lockedBefore)) return 0;
            if (EspMapLineState_view()->stateFNV1a != initialLineFNV ||
                EspMapLineTextureState_view()->stateFNV1a != initialTextureFNV) return 0;
            if (result.lockMutated != 0U || result.textureMutated != 0U) {
                ++audit->rollbackProofs;
            }

            hash = hashU32(hash, resultHash(&result));
        }
    }

    if (audit->refs == 0U || audit->mutatedRefs == 0U ||
        audit->mutatedRefs + audit->noMutationRefs != audit->refs ||
        audit->stateExecutorRefused != audit->refs ||
        audit->rollbackProofs != audit->mutatedRefs ||
        !audit->haveSample || !audit->haveUnsupported) return 0;
    audit->unlockFNV = hash;

    /* A repeated valid UNLOCK remains handled but becomes mutation-free. */
    if (!EspMapLineState_getLocked(audit->sampleResult.lineIndex, &lockedBefore) ||
        !EspMapLineTextureState_getEffectiveTexture(audit->sampleResult.lineIndex,
                                                    &textureBefore)) return 0;
    status = EspMapLineTextureState_applyUnlockCommand(
        &audit->sampleDescriptor, audit->sampleOffset, &result);
    if (status != ESP_MAP_LINE_UNLOCK_OK ||
        (result.lockMutated == 0U && result.textureMutated == 0U)) return 0;
    lineAfterFNV = EspMapLineState_view()->stateFNV1a;
    textureAfterFNV = EspMapLineTextureState_view()->stateFNV1a;

    status = EspMapLineTextureState_applyUnlockCommand(
        &audit->sampleDescriptor, audit->sampleOffset, &secondResult);
    if (status != ESP_MAP_LINE_UNLOCK_OK || secondResult.lockMutated != 0U ||
        secondResult.textureMutated != 0U || secondResult.soundId != 0U ||
        secondResult.effectFlags != 0U || secondResult.legacyReturnValue != 1U ||
        secondResult.removeCommandIfHandled != result.removeCommandIfHandled ||
        EspMapLineState_view()->stateFNV1a != lineAfterFNV ||
        EspMapLineTextureState_view()->stateFNV1a != textureAfterFNV) return 0;

    if (result.textureMutated != 0U &&
        !EspMapLineTextureState_setDoorTexture(audit->sampleResult.lineIndex,
                                               textureBefore)) return 0;
    if (result.lockMutated != 0U &&
        !EspMapLineState_setLocked(audit->sampleResult.lineIndex,
                                   lockedBefore)) return 0;
    if (EspMapLineState_view()->stateFNV1a != initialLineFNV ||
        EspMapLineTextureState_view()->stateFNV1a != initialTextureFNV) return 0;
    audit->idempotentHandledProof = 1U;

    /* Fail closed while preserving both native world owners. */
    memset(&result, 0xa5, sizeof(result));
    if (EspMapLineTextureState_applyUnlockCommand(
            &audit->unsupportedDescriptor, audit->unsupportedOffset, &result) !=
            ESP_MAP_LINE_UNLOCK_UNSUPPORTED || !resultIsZero(&result)) return 0;
    audit->unsupportedRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapLineTextureState_applyUnlockCommand(
            &audit->sampleDescriptor, audit->sampleDescriptor.commandCount,
            &result) != ESP_MAP_LINE_UNLOCK_INVALID || !resultIsZero(&result)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapLineTextureState_applyUnlockCommand(
            &badDescriptor, audit->sampleOffset, &result) !=
            ESP_MAP_LINE_UNLOCK_INVALID || !resultIsZero(&result)) return 0;
    audit->badDescriptorRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapLineTextureState_applyUnlockCommand(NULL, 0U, &result) !=
            ESP_MAP_LINE_UNLOCK_INVALID || !resultIsZero(&result)) return 0;
    audit->nullDescriptorRefused = 1U;

    if (EspMapLineTextureState_applyUnlockCommand(
            &audit->sampleDescriptor, audit->sampleOffset, NULL) !=
            ESP_MAP_LINE_UNLOCK_INVALID) return 0;
    audit->nullResultRefused = 1U;

    if (EspMapLineTextureState_getEffectiveTexture(EXPECTED_LINE_COUNT,
                                                   &textureAfter) ||
        EspMapLineTextureState_setDoorTexture(EXPECTED_LINE_COUNT,
                                              ESP_MAP_LINE_TEXTURE_LOCKED)) return 0;
    audit->badTextureIndexRefused = 1U;

    if (EspMapLineTextureState_setDoorTexture(audit->sampleResult.lineIndex,
                                              8U)) return 0;
    audit->badTextureValueRefused = 1U;

    for (eventIndex = 0U; eventIndex < EXPECTED_LINE_COUNT; ++eventIndex) {
        if (!EspMapRuntime_getLine(eventIndex, &line)) return 0;
        if (line.texture != ESP_MAP_LINE_TEXTURE_LOCKED &&
            line.texture != ESP_MAP_LINE_TEXTURE_UNLOCKED) {
            nonVariantLine = eventIndex;
            break;
        }
    }
    if (nonVariantLine == EXPECTED_LINE_COUNT ||
        EspMapLineTextureState_setDoorTexture(nonVariantLine,
                                              ESP_MAP_LINE_TEXTURE_LOCKED)) return 0;
    audit->nonVariantRefused = 1U;

    return EspMapLineState_view() != NULL &&
           EspMapLineState_view()->stateFNV1a == initialLineFNV &&
           EspMapLineTextureState_view() != NULL &&
           EspMapLineTextureState_view()->stateFNV1a == initialTextureFNV;
}

void Esp32Map1UnlockProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapLineTextureState_reset();
}

void Esp32Map1UnlockProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    EspMapEventDescriptor firstUnlockDescriptor;
    EspMapByteCode firstUnlockCommand;
    EspMapLineUnlockResult notReadyResult;
    UnlockAudit audit;
    uint8_t firstUnlockOffset;
    uint32_t heapBefore;
    uint32_t heapAfterBuild;
    uint32_t heapAfterAudit;
    uint32_t heapCost;
    uint32_t allocatorOverhead;
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
    uint32_t lineStateBefore;
    uint32_t lineStateAfter;
    uint32_t notebookBefore;
    uint32_t notebookAfter;
    uint32_t keysBefore;
    uint32_t keysAfter;
    uint32_t hudBefore;
    uint32_t hudAfter;
    uint32_t passwordCanvasBefore;
    uint32_t passwordCanvasAfter;
    uint32_t continuationBefore;
    uint32_t continuationAfter;
    uint32_t textureStateFNV;
    uint32_t variantCount;
    uint32_t initialTexture10;
    uint32_t startedMs;
    uint32_t elapsedMs;
    uint32_t notReadyRefused = 0U;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1LineDoorProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPUNLOCKPROBE] ARMED native line-door world state proven; EV_UNLOCK lock+texture execution starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO UNLOCK world state ===\n");
    printf("[MAPUNLOCKPROBE] CONTRACT preserve proven 120B OPEN/LOCKED owner + add 60B packed texture9/10 overlay; execute only EV_UNLOCK lock clear + optional 9->10 texture mutation; defer sound/special-entity/view effects; rollback every probe mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPUNLOCKPROBE] FAILED unsafe precondition\n");
        return;
    }
    if (!findFirstCommand(ESP_MAP_OPCODE_UNLOCK, &firstUnlockDescriptor,
                          &firstUnlockOffset, &firstUnlockCommand)) {
        printf("[MAPUNLOCKPROBE] FAILED no real EV_UNLOCK command\n");
        return;
    }

    memset(&notReadyResult, 0xa5, sizeof(notReadyResult));
    if (EspMapLineTextureState_applyUnlockCommand(
            &firstUnlockDescriptor, firstUnlockOffset, &notReadyResult) !=
            ESP_MAP_LINE_UNLOCK_NOT_READY || !resultIsZero(&notReadyResult)) {
        printf("[MAPUNLOCKPROBE] FAILED not-ready fail-closed\n");
        return;
    }
    notReadyRefused = 1U;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        lineState == NULL) {
        printf("[MAPUNLOCKPROBE] FAILED native prerequisites unavailable\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    lineStateBefore = lineState->stateFNV1a;
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                              (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = hudWitnessHash(doomRpg->hud);
    passwordCanvasBefore = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationBefore = gameContinuationHash(doomRpg->game);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspMapLineTextureState_buildFromRuntime()) {
        printf("[MAPUNLOCKPROBE] FAILED texture state build\n");
        return;
    }
    heapAfterBuild = heap8Free();
    textureState = EspMapLineTextureState_view();
    if (!validateTextureState(textureState, &textureStateFNV, &variantCount,
                              &initialTexture10)) {
        printf("[MAPUNLOCKPROBE] FAILED texture state validation\n");
        return;
    }

    if (!auditUnlockCommands(&audit, EXPECTED_LINE_STATE_FNV,
                             textureStateFNV)) {
        printf("[MAPUNLOCKPROBE] FAILED UNLOCK corpus/rollback audit\n");
        return;
    }

    heapAfterAudit = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    arenaAfter = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateAfter = EspMapState_view()->stateFNV1a;
    scriptAfter = fnv1a32(EspMapScriptState_view()->storage,
                           EspMapScriptState_view()->storageBytes);
    lineStateAfter = EspMapLineState_view()->stateFNV1a;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = hudWitnessHash(doomRpg->hud);
    passwordCanvasAfter = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationAfter = gameContinuationHash(doomRpg->game);
    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;

    if (heapBefore < heapAfterBuild || heapAfterBuild != heapAfterAudit) {
        printf("[MAPUNLOCKPROBE] FAILED heap lifecycle before=%u build=%u audit=%u\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfterBuild,
               (unsigned int)heapAfterAudit);
        return;
    }
    heapCost = heapBefore - heapAfterBuild;
    if (heapCost < EXPECTED_TEXTURE_STATE_BYTES ||
        heapCost > EXPECTED_TEXTURE_STATE_BYTES + MAX_ALLOCATOR_OVERHEAD) {
        printf("[MAPUNLOCKPROBE] FAILED heap cost=%u payload=%u\n",
               (unsigned int)heapCost,
               (unsigned int)EXPECTED_TEXTURE_STATE_BYTES);
        return;
    }
    allocatorOverhead = heapCost - EXPECTED_TEXTURE_STATE_BYTES;

    textureState = EspMapLineTextureState_view();
    if (textureState == NULL || textureState->stateFNV1a != textureStateFNV ||
        sizeof(EspMapLineUnlockResult) != EXPECTED_RESULT_BYTES ||
        lineStateAfter != lineStateBefore ||
        lineStateAfter != EXPECTED_LINE_STATE_FNV ||
        largestAfter < MIN_LARGEST8_AFTER_STATE || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore || mapStateAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV ||
        notebookAfter != notebookBefore ||
        notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV || keysAfter != keysBefore ||
        hudAfter != hudBefore || passwordCanvasAfter != passwordCanvasBefore ||
        continuationAfter != continuationBefore || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPUNLOCKPROBE] FAILED integrity regression\n");
        return;
    }

    printf("[MAPUNLOCK] READY refs=%u mutated=%u lockMutated=%u textureMutated=%u noMutation=%u removable=%u resultBytes=%u stateExecRefused=%u unlockFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs,
           (unsigned int)audit.mutatedRefs,
           (unsigned int)audit.lockMutatedRefs,
           (unsigned int)audit.textureMutatedRefs,
           (unsigned int)audit.noMutationRefs,
           (unsigned int)audit.removableRefs,
           (unsigned int)sizeof(EspMapLineUnlockResult),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.unlockFNV,
           (unsigned int)elapsedMs);
    printf("[MAPUNLOCK] SAMPLE cmd=%u event=%u off=%u line=%u locked=%u->%u texture=%u->%u lockMut=%u texMut=%u sound=%u effects=%02x handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.sampleResult.globalCommandIndex,
           (unsigned int)audit.sampleResult.sourceEventIndex,
           (unsigned int)audit.sampleResult.sourceCommandOffset,
           (unsigned int)audit.sampleResult.lineIndex,
           (unsigned int)audit.sampleResult.lockedBefore,
           (unsigned int)audit.sampleResult.lockedAfter,
           (unsigned int)audit.sampleResult.textureBefore,
           (unsigned int)audit.sampleResult.textureAfter,
           (unsigned int)audit.sampleResult.lockMutated,
           (unsigned int)audit.sampleResult.textureMutated,
           (unsigned int)audit.sampleResult.soundId,
           (unsigned int)audit.sampleResult.effectFlags,
           (unsigned int)audit.sampleResult.legacyReturnValue,
           (unsigned int)audit.sampleResult.removeCommandIfHandled);
    printf("[MAPUNLOCK] WORLD lineStateBytes=%u lineStateFNV=%08x textureBytes=%u variants=%u initialTexture10=%u textureStateFNV=%08x mutatedLineFNV=%08x mutatedTextureFNV=%08x rollback=%u/%u idempotentHandled=%u\n",
           (unsigned int)lineState->storageBytes,
           (unsigned int)EXPECTED_LINE_STATE_FNV,
           (unsigned int)textureState->storageBytes,
           (unsigned int)variantCount,
           (unsigned int)initialTexture10,
           (unsigned int)textureStateFNV,
           (unsigned int)audit.sampleMutatedLineFNV,
           (unsigned int)audit.sampleMutatedTextureFNV,
           (unsigned int)audit.rollbackProofs,
           (unsigned int)audit.mutatedRefs,
           (unsigned int)audit.idempotentHandledProof);
    printf("[MAPUNLOCK] FAILCLOSED notReady=%u unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullResult=%u badTextureIndex=%u badTextureValue=%u nonVariant=%u stateAtomic=yes worldRestored=yes\n",
           (unsigned int)notReadyRefused,
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullResultRefused,
           (unsigned int)audit.badTextureIndexRefused,
           (unsigned int)audit.badTextureValueRefused,
           (unsigned int)audit.nonVariantRefused);
    printf("[MAPUNLOCKPROBE] RAM heap8=%u->%u persistentHeapCost=%u payload=%u allocatorOverhead=%u largest8=%u->%u frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x lineStateFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfterAudit,
           (unsigned int)heapCost,
           (unsigned int)EXPECTED_TEXTURE_STATE_BYTES,
           (unsigned int)allocatorOverhead,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)arenaBefore,
           (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore,
           (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore,
           (unsigned int)scriptAfter,
           (unsigned int)lineStateBefore,
           (unsigned int)lineStateAfter);
    printf("[MAPUNLOCKPROBE] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x packIO=no legacyRuntimeClear=yes\n",
           (unsigned int)notebookBefore,
           (unsigned int)notebookAfter,
           (unsigned int)keysBefore,
           (unsigned int)keysAfter,
           (unsigned int)hudBefore,
           (unsigned int)hudAfter,
           (unsigned int)passwordCanvasBefore,
           (unsigned int)passwordCanvasAfter,
           (unsigned int)continuationBefore,
           (unsigned int)continuationAfter);
    printf("[MAPUNLOCKPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes nativeLineState=yes nativeDoorExec=yes nativeLineTextureState=yes nativeUnlockExec=yes textureStorageBytes=%u resultBytes=%u worldMutationProven=yes worldRestored=yes legacyWorldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)textureState->storageBytes,
           (unsigned int)sizeof(EspMapLineUnlockResult));

    probeState.done = 1;
}

int Esp32Map1UnlockProbe_isDone(void) {
    return probeState.done;
}
