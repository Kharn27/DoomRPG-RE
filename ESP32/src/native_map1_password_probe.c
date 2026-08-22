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
#include "esp_map_opcode_executor.h"
#include "esp_map_password.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_strings.h"
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
#include "native_map1_password_probe.h"
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
#define EXPECTED_OWNER_BYTES 20U
#define EXPECTED_SUBMIT_RESULT_BYTES 12U
#define SCRATCH_CAPACITY 314U
#define SCRATCH_STORAGE_BYTES (SCRATCH_CAPACITY + 2U)

typedef struct Esp32Map1PasswordProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1PasswordProbeState;

typedef struct PasswordAudit_s {
    uint32_t ownerFNV;
    uint32_t submitFNV;
    uint32_t refs;
    uint32_t stateExecutorRefused;
    uint32_t codeBytes;
    uint32_t promptBytes;
    uint32_t maxCodeLength;
    uint32_t resumeExact;
    uint32_t correctScenarios;
    uint32_t incorrectScenarios;
    uint32_t emptySemantics;
    uint32_t correctResume;
    uint32_t incorrectNoResume;
    uint32_t guardChecks;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullOwnerRefused;
    uint32_t badOwnerRefused;
    uint32_t tooLongRefused;
    uint32_t shortBufferRefused;
    uint32_t nullSubmitOwnerRefused;
    uint32_t nullSubmitResultRefused;
    uint32_t closedPackRefused;
    uint32_t resetProof;
    EspMapEventDescriptor sampleDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode sampleCommand;
    EspMapPasswordOwnerState sampleOwner;
    uint32_t sampleCodeFNV;
    uint32_t samplePromptFNV;
    char sampleCode[ESP_MAP_PASSWORD_INPUT_CAPACITY];
    uint8_t sampleCodeLength;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
} PasswordAudit;

static Esp32Map1PasswordProbeState probeState;

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

static uint32_t ownerHash(const EspMapPasswordOwnerState* state) {
    uint32_t hash = 2166136261U;

    if (state == NULL) return 0U;
    hash = hashU16(hash, state->expectedCode.index);
    hash = hashU16(hash, state->expectedCode.sourceOffset);
    hash = hashU16(hash, state->expectedCode.length);
    hash = hashU16(hash, state->prompt.index);
    hash = hashU16(hash, state->prompt.sourceOffset);
    hash = hashU16(hash, state->prompt.length);
    hash = hashU16(hash, state->sourceEventIndex);
    hash = hashU16(hash, state->globalCommandIndex);
    hash = hashByte(hash, state->sourceCommandOffset);
    hash = hashByte(hash, state->resumeCommandOffset);
    hash = hashByte(hash, state->flags);
    return hashByte(hash, state->active);
}

static uint32_t submitResultHash(const EspMapPasswordSubmitResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->feedbackDelayMs);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->resumeCommandOffset);
    hash = hashByte(hash, result->kind);
    hash = hashByte(hash, result->closeDialog);
    hash = hashByte(hash, result->resumeEvent);
    return hashByte(hash, result->forceStatusMessage);
}

static int sameOwner(const EspMapPasswordOwnerState* a,
                     const EspMapPasswordOwnerState* b) {
    return a != NULL && b != NULL && memcmp(a, b, sizeof(*a)) == 0;
}

static int ownerIsClear(const EspMapPasswordOwnerState* state) {
    EspMapPasswordOwnerState zero;

    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

static int resultIsZero(const EspMapPasswordSubmitResult* result) {
    EspMapPasswordSubmitResult zero;

    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}

static size_t boundedCStringLength(const char* text, size_t capacity) {
    size_t i;

    if (text == NULL) return 0U;
    for (i = 0U; i < capacity; ++i) {
        if (text[i] == '\0') return i;
    }
    return capacity;
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
           Esp32Map1KeyGateProbe_isDone() &&
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

static int readWithGuard(const EspAssetPackEntry* entry,
                         const EspMapStringRef* ref,
                         uint8_t storage[SCRATCH_STORAGE_BYTES],
                         size_t capacity,
                         size_t* outLength,
                         EspMapStringReadStatus* outStatus) {
    char* scratch = (char*)&storage[1];

    if (storage == NULL || outLength == NULL || outStatus == NULL ||
        capacity > SCRATCH_CAPACITY) return 0;
    storage[0] = 0xa5U;
    storage[SCRATCH_STORAGE_BYTES - 1U] = 0x5aU;
    memset(scratch, 0xcc, SCRATCH_CAPACITY);
    *outLength = (size_t)-1;
    *outStatus = EspMapStrings_read(entry, ref, scratch, capacity, outLength);
    return storage[0] == 0xa5U &&
           storage[SCRATCH_STORAGE_BYTES - 1U] == 0x5aU;
}

static int submitWithGuard(const EspAssetPackEntry* entry,
                           const EspMapPasswordOwnerState* owner,
                           const char* submitted,
                           size_t submittedLength,
                           uint8_t storage[SCRATCH_STORAGE_BYTES],
                           size_t capacity,
                           size_t* outExpectedLength,
                           EspMapPasswordSubmitResult* outResult,
                           EspMapPasswordSubmitStatus* outStatus) {
    char* scratch = (char*)&storage[1];

    if (storage == NULL || outExpectedLength == NULL || outResult == NULL ||
        outStatus == NULL || capacity > SCRATCH_CAPACITY) return 0;
    storage[0] = 0xa5U;
    storage[SCRATCH_STORAGE_BYTES - 1U] = 0x5aU;
    memset(scratch, 0xcc, SCRATCH_CAPACITY);
    *outExpectedLength = (size_t)-1;
    memset(outResult, 0x7b, sizeof(*outResult));
    *outStatus = EspMapPassword_evaluateSubmit(
        entry, owner, submitted, submittedLength, scratch, capacity,
        outExpectedLength, outResult);
    return storage[0] == 0xa5U &&
           storage[SCRATCH_STORAGE_BYTES - 1U] == 0x5aU;
}

static int validateOwner(const EspMapEventDescriptor* descriptor,
                         uint32_t commandOffset,
                         const EspMapByteCode* command,
                         const EspMapPasswordOwnerState* owner) {
    EspMapStringRef codeRef;
    EspMapStringRef promptRef;
    uint32_t codeIndex;
    uint32_t promptIndex;

    if (descriptor == NULL || command == NULL || owner == NULL ||
        command->id != ESP_MAP_OPCODE_PASSWORD ||
        commandOffset >= descriptor->commandCount ||
        commandOffset >= 0xffU) return 0;

    codeIndex = command->arg1 & 0xffU;
    promptIndex = (command->arg1 >> 8) & 0xffU;
    if (!EspMapStrings_getRef(codeIndex, &codeRef) ||
        !EspMapStrings_getRef(promptIndex, &promptRef)) return 0;

    return sizeof(*owner) == EXPECTED_OWNER_BYTES &&
           EspMapPasswordOwner_isActive(owner) &&
           owner->expectedCode.index == codeRef.index &&
           owner->expectedCode.sourceOffset == codeRef.sourceOffset &&
           owner->expectedCode.length == codeRef.length &&
           owner->prompt.index == promptRef.index &&
           owner->prompt.sourceOffset == promptRef.sourceOffset &&
           owner->prompt.length == promptRef.length &&
           owner->sourceEventIndex == descriptor->eventIndex &&
           owner->globalCommandIndex ==
               (uint16_t)((uint32_t)descriptor->firstCommandIndex +
                          commandOffset) &&
           owner->sourceCommandOffset == (uint8_t)commandOffset &&
           owner->resumeCommandOffset == (uint8_t)(commandOffset + 1U) &&
           owner->flags == ESP_MAP_PASSWORD_EXPECTED_FLAGS &&
           owner->active == 1U;
}

static int validateOutcome(const EspMapPasswordOwnerState* owner,
                           uint8_t expectedKind,
                           uint16_t expectedDelayMs,
                           const EspMapPasswordSubmitResult* result) {
    const char* message;

    if (owner == NULL || result == NULL ||
        result->sourceEventIndex != owner->sourceEventIndex ||
        result->globalCommandIndex != owner->globalCommandIndex ||
        result->feedbackDelayMs != expectedDelayMs ||
        result->sourceCommandOffset != owner->sourceCommandOffset ||
        result->resumeCommandOffset != owner->resumeCommandOffset ||
        result->kind != expectedKind || result->closeDialog != 1U) {
        return 0;
    }

    message = EspMapPassword_resultMessage(result);
    if (expectedKind == ESP_MAP_PASSWORD_OUTCOME_CORRECT) {
        return result->resumeEvent == 1U &&
               result->forceStatusMessage == 1U && message != NULL &&
               strcmp(message, "Correct code!") == 0;
    }
    if (expectedKind == ESP_MAP_PASSWORD_OUTCOME_INCORRECT) {
        return result->resumeEvent == 0U &&
               result->forceStatusMessage == 1U && message != NULL &&
               strcmp(message, "Invalid code!") == 0;
    }
    if (expectedKind == ESP_MAP_PASSWORD_OUTCOME_EMPTY) {
        return result->resumeEvent == 0U &&
               result->forceStatusMessage == 0U && message == NULL;
    }
    return 0;
}

static int auditPasswords(const EspAssetPackEntry* entry,
                          PasswordAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapPasswordOwnerState owner;
    EspMapPasswordOwnerState beforeOwner;
    EspMapPasswordOwnerState testOwner;
    EspMapPasswordSubmitResult result;
    EspMapPasswordSubmitStatus submitStatus;
    EspMapStringReadStatus readStatus;
    uint8_t codeStorage[SCRATCH_STORAGE_BYTES];
    uint8_t promptStorage[SCRATCH_STORAGE_BYTES];
    uint8_t submitStorage[SCRATCH_STORAGE_BYTES];
    char* codeScratch = (char*)&codeStorage[1];
    char* promptScratch = (char*)&promptStorage[1];
    char wrong[ESP_MAP_PASSWORD_INPUT_CAPACITY];
    char tooLong[ESP_MAP_PASSWORD_INPUT_CAPACITY];
    size_t codeReadLength;
    size_t promptReadLength;
    size_t codeLength;
    size_t expectedLength;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t codeFNV;
    uint32_t promptFNV;
    uint32_t ownerHashValue;
    uint32_t correctHash;
    uint32_t incorrectHash;
    uint32_t emptyHash;
    uint32_t ownerAggregate = 2166136261U;
    uint32_t submitAggregate = 2166136261U;
    uint8_t emptyKind;
    uint16_t wrongDelay;
    uint16_t emptyDelay;

    if (entry == NULL || audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    memset(&owner, 0, sizeof(owner));

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }

            if (!audit->haveUnsupported &&
                command.id != ESP_MAP_OPCODE_PASSWORD) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_PASSWORD) continue;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) {
                return 0;
            }
            ++audit->stateExecutorRefused;

            beforeOwner = owner;
            if (EspMapPasswordOwner_apply(&owner, &descriptor, commandOffset) !=
                    ESP_MAP_PASSWORD_OWNER_OK ||
                !validateOwner(&descriptor, commandOffset, &command, &owner)) {
                return 0;
            }
            ++audit->resumeExact;

            if (!readWithGuard(entry, &owner.expectedCode, codeStorage,
                               SCRATCH_CAPACITY, &codeReadLength, &readStatus) ||
                readStatus != ESP_MAP_STRING_READ_OK ||
                codeReadLength != owner.expectedCode.length ||
                codeScratch[codeReadLength] != '\0') return 0;
            ++audit->guardChecks;

            if (!readWithGuard(entry, &owner.prompt, promptStorage,
                               SCRATCH_CAPACITY, &promptReadLength, &readStatus) ||
                readStatus != ESP_MAP_STRING_READ_OK ||
                promptReadLength != owner.prompt.length ||
                promptScratch[promptReadLength] != '\0') return 0;
            ++audit->guardChecks;

            codeLength = boundedCStringLength(codeScratch, codeReadLength);
            if (codeLength >= ESP_MAP_PASSWORD_INPUT_CAPACITY) return 0;
            codeFNV = fnv1a32((const uint8_t*)codeScratch,
                              (uint32_t)codeReadLength);
            promptFNV = fnv1a32((const uint8_t*)promptScratch,
                                (uint32_t)promptReadLength);
            ownerHashValue = ownerHash(&owner);

            if (!submitWithGuard(entry, &owner, codeScratch, codeLength,
                                 submitStorage, SCRATCH_CAPACITY,
                                 &expectedLength, &result, &submitStatus) ||
                submitStatus != ESP_MAP_PASSWORD_SUBMIT_OK ||
                expectedLength != codeLength ||
                !validateOutcome(&owner, ESP_MAP_PASSWORD_OUTCOME_CORRECT,
                                 ESP_MAP_PASSWORD_MATCH_DELAY_MS,
                                 &result)) return 0;
            ++audit->guardChecks;
            ++audit->correctScenarios;
            ++audit->correctResume;
            correctHash = submitResultHash(&result);

            memset(wrong, 0, sizeof(wrong));
            if (codeLength == 0U) {
                wrong[0] = '0';
                wrongDelay = 0U;
                if (!submitWithGuard(entry, &owner, wrong, 1U,
                                     submitStorage, SCRATCH_CAPACITY,
                                     &expectedLength, &result, &submitStatus) ||
                    submitStatus != ESP_MAP_PASSWORD_SUBMIT_OK ||
                    expectedLength != 0U ||
                    !validateOutcome(&owner,
                                     ESP_MAP_PASSWORD_OUTCOME_INCORRECT,
                                     wrongDelay, &result)) return 0;
            }
            else {
                size_t wrongLength = codeLength;
                memcpy(wrong, codeScratch, wrongLength);
                wrong[0] = wrong[0] == '0' ? '1' : '0';
                wrongDelay = ESP_MAP_PASSWORD_MATCH_DELAY_MS;
                if (!submitWithGuard(entry, &owner, wrong, wrongLength,
                                     submitStorage, SCRATCH_CAPACITY,
                                     &expectedLength, &result, &submitStatus) ||
                    submitStatus != ESP_MAP_PASSWORD_SUBMIT_OK ||
                    expectedLength != codeLength ||
                    !validateOutcome(&owner,
                                     ESP_MAP_PASSWORD_OUTCOME_INCORRECT,
                                     wrongDelay, &result)) return 0;
            }
            ++audit->guardChecks;
            ++audit->incorrectScenarios;
            ++audit->incorrectNoResume;
            incorrectHash = submitResultHash(&result);

            if (!submitWithGuard(entry, &owner, NULL, 0U,
                                 submitStorage, SCRATCH_CAPACITY,
                                 &expectedLength, &result, &submitStatus) ||
                submitStatus != ESP_MAP_PASSWORD_SUBMIT_OK ||
                expectedLength != codeLength) return 0;
            ++audit->guardChecks;
            emptyKind = codeLength == 0U
                            ? ESP_MAP_PASSWORD_OUTCOME_CORRECT
                            : ESP_MAP_PASSWORD_OUTCOME_EMPTY;
            emptyDelay = codeLength == 0U
                             ? ESP_MAP_PASSWORD_MATCH_DELAY_MS
                             : 0U;
            if (!validateOutcome(&owner, emptyKind, emptyDelay, &result)) {
                return 0;
            }
            ++audit->emptySemantics;
            emptyHash = submitResultHash(&result);

            ownerAggregate = hashU16(ownerAggregate, owner.globalCommandIndex);
            ownerAggregate = hashU32(ownerAggregate, ownerHashValue);
            ownerAggregate = hashU32(ownerAggregate, codeFNV);
            ownerAggregate = hashU32(ownerAggregate, promptFNV);
            submitAggregate = hashU16(submitAggregate, owner.globalCommandIndex);
            submitAggregate = hashU32(submitAggregate, correctHash);
            submitAggregate = hashU32(submitAggregate, incorrectHash);
            submitAggregate = hashU32(submitAggregate, emptyHash);

            ++audit->refs;
            audit->codeBytes += (uint32_t)codeReadLength;
            audit->promptBytes += (uint32_t)promptReadLength;
            if ((uint32_t)codeLength > audit->maxCodeLength) {
                audit->maxCodeLength = (uint32_t)codeLength;
            }

            if (!audit->haveSample) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleOwner = owner;
                audit->sampleCodeFNV = codeFNV;
                audit->samplePromptFNV = promptFNV;
                audit->sampleCodeLength = (uint8_t)codeLength;
                if (codeLength != 0U) {
                    memcpy(audit->sampleCode, codeScratch, codeLength);
                }
                audit->sampleCode[codeLength] = '\0';
                audit->sampleOffset = (uint8_t)commandOffset;
                audit->haveSample = 1U;
            }

            (void)beforeOwner;
        }
    }

    if (audit->refs == 0U || !audit->haveSample || !audit->haveUnsupported ||
        audit->stateExecutorRefused != audit->refs ||
        audit->resumeExact != audit->refs ||
        audit->correctScenarios != audit->refs ||
        audit->incorrectScenarios != audit->refs ||
        audit->emptySemantics != audit->refs ||
        audit->correctResume != audit->refs ||
        audit->incorrectNoResume != audit->refs ||
        audit->guardChecks != audit->refs * 5U ||
        sizeof(EspMapPasswordOwnerState) != EXPECTED_OWNER_BYTES ||
        sizeof(EspMapPasswordSubmitResult) != EXPECTED_SUBMIT_RESULT_BYTES) {
        return 0;
    }

    audit->ownerFNV = ownerAggregate;
    audit->submitFNV = submitAggregate;

    testOwner = audit->sampleOwner;
    beforeOwner = testOwner;
    if (EspMapPasswordOwner_apply(&testOwner, &audit->unsupportedDescriptor,
                                  audit->unsupportedOffset) !=
            ESP_MAP_PASSWORD_OWNER_UNSUPPORTED ||
        !sameOwner(&testOwner, &beforeOwner)) return 0;
    audit->unsupportedRefused = 1U;

    testOwner = audit->sampleOwner;
    beforeOwner = testOwner;
    if (EspMapPasswordOwner_apply(&testOwner, &audit->sampleDescriptor,
                                  audit->sampleDescriptor.commandCount) !=
            ESP_MAP_PASSWORD_OWNER_INVALID ||
        !sameOwner(&testOwner, &beforeOwner)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.value ^= 1U;
    testOwner = audit->sampleOwner;
    beforeOwner = testOwner;
    if (EspMapPasswordOwner_apply(&testOwner, &badDescriptor,
                                  audit->sampleOffset) !=
            ESP_MAP_PASSWORD_OWNER_INVALID ||
        !sameOwner(&testOwner, &beforeOwner)) return 0;
    audit->badDescriptorRefused = 1U;

    testOwner = audit->sampleOwner;
    beforeOwner = testOwner;
    if (EspMapPasswordOwner_apply(&testOwner, NULL, audit->sampleOffset) !=
            ESP_MAP_PASSWORD_OWNER_INVALID ||
        !sameOwner(&testOwner, &beforeOwner)) return 0;
    audit->nullDescriptorRefused = 1U;

    if (EspMapPasswordOwner_apply(NULL, &audit->sampleDescriptor,
                                  audit->sampleOffset) !=
            ESP_MAP_PASSWORD_OWNER_INVALID) return 0;
    audit->nullOwnerRefused = 1U;

    testOwner = audit->sampleOwner;
    ++testOwner.expectedCode.sourceOffset;
    if (!submitWithGuard(entry, &testOwner, audit->sampleCode,
                         audit->sampleCodeLength, submitStorage,
                         SCRATCH_CAPACITY, &expectedLength, &result,
                         &submitStatus) ||
        submitStatus != ESP_MAP_PASSWORD_SUBMIT_INVALID ||
        !resultIsZero(&result)) return 0;
    audit->badOwnerRefused = 1U;

    memset(tooLong, '7', sizeof(tooLong));
    if (!submitWithGuard(entry, &audit->sampleOwner, tooLong,
                         sizeof(tooLong), submitStorage, SCRATCH_CAPACITY,
                         &expectedLength, &result, &submitStatus) ||
        submitStatus != ESP_MAP_PASSWORD_SUBMIT_INVALID ||
        !resultIsZero(&result)) return 0;
    audit->tooLongRefused = 1U;

    if (audit->sampleOwner.expectedCode.length == 0U) return 0;
    if (!submitWithGuard(entry, &audit->sampleOwner, audit->sampleCode,
                         audit->sampleCodeLength, submitStorage,
                         audit->sampleOwner.expectedCode.length,
                         &expectedLength, &result, &submitStatus) ||
        submitStatus != ESP_MAP_PASSWORD_SUBMIT_BUFFER_TOO_SMALL ||
        !resultIsZero(&result)) return 0;
    audit->shortBufferRefused = 1U;

    if (!submitWithGuard(entry, NULL, audit->sampleCode,
                         audit->sampleCodeLength, submitStorage,
                         SCRATCH_CAPACITY, &expectedLength, &result,
                         &submitStatus) ||
        submitStatus != ESP_MAP_PASSWORD_SUBMIT_INVALID ||
        !resultIsZero(&result)) return 0;
    audit->nullSubmitOwnerRefused = 1U;

    memset(&result, 0, sizeof(result));
    if (EspMapPassword_evaluateSubmit(entry, &audit->sampleOwner,
                                      audit->sampleCode,
                                      audit->sampleCodeLength,
                                      (char*)&submitStorage[1],
                                      SCRATCH_CAPACITY, &expectedLength,
                                      NULL) != ESP_MAP_PASSWORD_SUBMIT_INVALID) {
        return 0;
    }
    audit->nullSubmitResultRefused = 1U;

    return 1;
}

void Esp32Map1PasswordProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32Map1PasswordProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1PasswordProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    EspAssetPackEntry entry;
    PasswordAudit audit;
    EspMapPasswordOwnerState resetOwner;
    EspMapPasswordSubmitResult closedResult;
    EspMapPasswordSubmitStatus closedStatus;
    uint8_t submitStorage[SCRATCH_STORAGE_BYTES];
    size_t expectedLength;
    uint32_t heapBefore, heapOpen, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter;
    uint32_t scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter;
    uint32_t hudBefore, hudAfter;
    uint32_t passwordCanvasBefore, passwordCanvasAfter;
    uint32_t keysBefore, keysAfter;
    uint32_t started, elapsed;
    char* gamePassCodeBefore;
    int skipAdvanceTurnBefore;
    int saveTileEventBefore;
    int tileEventBefore;
    int tileEventIndexBefore;
    int tileEventFlagsBefore;

    if (probeState.done || probeState.attempted) return;
    if (!probeState.armed) {
        if (Esp32Map1KeyGateProbe_isDone()) {
            probeState.armed = 1;
            printf("[MAPPASSWORDPROBE] ARMED native CHECK_KEY gate proven; EV_PASSWORD owner/submission evaluation starts on next loop service\n");
        }
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO PASSWORD owner ===\n");
    printf("[MAPPASSWORDPROBE] CONTRACT consume only EV_PASSWORD -> 20B caller-owned two-ref pause/continuation state + 12B bounded submit result; no password UI, legacy DoomCanvas/Game/Hud/world/render mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPPASSWORDPROBE] FAILED unsafe precondition\n");
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
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = hudWitnessHash(doomRpg->hud);
    passwordCanvasBefore = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    gamePassCodeBefore = doomRpg->game->passCode;
    skipAdvanceTurnBefore = doomRpg->game->skipAdvanceTurn;
    saveTileEventBefore = doomRpg->game->saveTileEvent;
    tileEventBefore = doomRpg->game->tileEvent;
    tileEventIndexBefore = doomRpg->game->tileEventIndex;
    tileEventFlagsBefore = doomRpg->game->tileEventFlags;
    started = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPPASSWORDPROBE] FAILED open %s\n",
               ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry("/intro.bsp", &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        !auditPasswords(&entry, &audit)) {
        EspAssetPack_close();
        printf("[MAPPASSWORDPROBE] FAILED native PASSWORD audit\n");
        return;
    }

    EspAssetPack_close();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (!submitWithGuard(&entry, &audit.sampleOwner, audit.sampleCode,
                         audit.sampleCodeLength, submitStorage,
                         SCRATCH_CAPACITY, &expectedLength, &closedResult,
                         &closedStatus) ||
        closedStatus != ESP_MAP_PASSWORD_SUBMIT_IO_ERROR ||
        !resultIsZero(&closedResult)) {
        printf("[MAPPASSWORDPROBE] FAILED closed-pack fail-closed\n");
        return;
    }
    audit.closedPackRefused = 1U;

    resetOwner = audit.sampleOwner;
    EspMapPasswordOwner_reset(&resetOwner);
    if (!ownerIsClear(&resetOwner) ||
        EspMapPasswordOwner_isActive(&resetOwner)) {
        printf("[MAPPASSWORDPROBE] FAILED reset\n");
        return;
    }
    audit.resetProof = 1U;

    elapsed = DoomRPG_GetUpTimeMS() - started;
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
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = hudWitnessHash(doomRpg->hud);
    passwordCanvasAfter = canvasPasswordWitnessHash(doomRpg->doomCanvas);

    if (audit.refs == 0U || audit.stateExecutorRefused != audit.refs ||
        audit.resumeExact != audit.refs ||
        audit.correctScenarios != audit.refs ||
        audit.incorrectScenarios != audit.refs ||
        audit.emptySemantics != audit.refs ||
        audit.correctResume != audit.refs ||
        audit.incorrectNoResume != audit.refs ||
        audit.guardChecks != audit.refs * 5U ||
        audit.unsupportedRefused != 1U || audit.badOffsetRefused != 1U ||
        audit.badDescriptorRefused != 1U ||
        audit.nullDescriptorRefused != 1U || audit.nullOwnerRefused != 1U ||
        audit.badOwnerRefused != 1U || audit.tooLongRefused != 1U ||
        audit.shortBufferRefused != 1U ||
        audit.nullSubmitOwnerRefused != 1U ||
        audit.nullSubmitResultRefused != 1U ||
        audit.closedPackRefused != 1U || audit.resetProof != 1U ||
        sizeof(EspMapPasswordOwnerState) != EXPECTED_OWNER_BYTES ||
        sizeof(EspMapPasswordSubmitResult) != EXPECTED_SUBMIT_RESULT_BYTES ||
        EspAssetPack_isOpen() || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || mapStateAfter != mapStateBefore ||
        scriptAfter != scriptBefore || notebookAfter != notebookBefore ||
        notebookBefore != EXPECTED_LEGACY_NOTEBOOK_FNV ||
        keysAfter != keysBefore || hudAfter != hudBefore ||
        passwordCanvasAfter != passwordCanvasBefore ||
        doomRpg->game->passCode != gamePassCodeBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceTurnBefore ||
        doomRpg->game->saveTileEvent != saveTileEventBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->doomCanvas->state != ST_INTRO) {
        printf("[MAPPASSWORDPROBE] FAILED acceptance/integrity\n");
        return;
    }

    printf("[MAPPASSWORD] READY refs=%u ownerBytes=%u submitResultBytes=%u stateExecRefused=%u codeBytes=%u promptBytes=%u maxCodeLen=%u resumeExact=%u passwordOwnerFNV=%08x passwordSubmitFNV=%08x elapsed=%ums\n",
           (unsigned)audit.refs, (unsigned)sizeof(EspMapPasswordOwnerState),
           (unsigned)sizeof(EspMapPasswordSubmitResult),
           (unsigned)audit.stateExecutorRefused, (unsigned)audit.codeBytes,
           (unsigned)audit.promptBytes, (unsigned)audit.maxCodeLength,
           (unsigned)audit.resumeExact, (unsigned)audit.ownerFNV,
           (unsigned)audit.submitFNV, (unsigned)elapsed);
    printf("[MAPPASSWORD] SAMPLE cmd=%u event=%u off=%u resume=%u arg1=%08x arg2=%08x code=%u@%u+%u codeFNV=%08x prompt=%u@%u+%u promptFNV=%08x codeLen=%u\n",
           (unsigned)audit.sampleOwner.globalCommandIndex,
           (unsigned)audit.sampleOwner.sourceEventIndex,
           (unsigned)audit.sampleOwner.sourceCommandOffset,
           (unsigned)audit.sampleOwner.resumeCommandOffset,
           (unsigned)audit.sampleCommand.arg1,
           (unsigned)audit.sampleCommand.arg2,
           (unsigned)audit.sampleOwner.expectedCode.index,
           (unsigned)audit.sampleOwner.expectedCode.sourceOffset,
           (unsigned)audit.sampleOwner.expectedCode.length,
           (unsigned)audit.sampleCodeFNV,
           (unsigned)audit.sampleOwner.prompt.index,
           (unsigned)audit.sampleOwner.prompt.sourceOffset,
           (unsigned)audit.sampleOwner.prompt.length,
           (unsigned)audit.samplePromptFNV,
           (unsigned)audit.sampleCodeLength);
    printf("[MAPPASSWORD] OUTCOMES correct=%u incorrect=%u emptySemantics=%u correctResume=%u incorrectNoResume=%u delayMatch=%ums earlySubmit=0ms correctMessage=\"Correct code!\" invalidMessage=\"Invalid code!\" guards=%u/%u\n",
           (unsigned)audit.correctScenarios,
           (unsigned)audit.incorrectScenarios,
           (unsigned)audit.emptySemantics,
           (unsigned)audit.correctResume,
           (unsigned)audit.incorrectNoResume,
           (unsigned)ESP_MAP_PASSWORD_MATCH_DELAY_MS,
           (unsigned)audit.guardChecks,
           (unsigned)(audit.refs * 5U));
    printf("[MAPPASSWORD] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullOwner=%u badOwner=%u tooLong=%u shortBuffer=%u nullSubmitOwner=%u nullSubmitResult=%u closedPack=%u ownerAtomic=yes reset=%u\n",
           (unsigned)audit.unsupportedRefused,
           (unsigned)audit.badOffsetRefused,
           (unsigned)audit.badDescriptorRefused,
           (unsigned)audit.nullDescriptorRefused,
           (unsigned)audit.nullOwnerRefused,
           (unsigned)audit.badOwnerRefused,
           (unsigned)audit.tooLongRefused,
           (unsigned)audit.shortBufferRefused,
           (unsigned)audit.nullSubmitOwnerRefused,
           (unsigned)audit.nullSubmitResultRefused,
           (unsigned)audit.closedPackRefused,
           (unsigned)audit.resetProof);
    printf("[MAPPASSWORD] IO entry=/intro.bsp size=%u crc32=%08x heapOpen=%u transientHeapCost=%d largestOpen=%u packIO=yes persistentHeapBytes=0\n",
           (unsigned)entry.size, (unsigned)entry.crc32, (unsigned)heapOpen,
           (int)heapBefore - (int)heapOpen, (unsigned)largestOpen);
    printf("[MAPPASSWORDPROBE] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x legacyNotebookFNV=%08x->%08x legacyKeys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x gamePassCodeStable=yes\n",
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
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)passwordCanvasBefore, (unsigned)passwordCanvasAfter);
    printf("[MAPPASSWORDPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes ownerBytes=%u submitResultBytes=%u persistentBytes=0 legacyPasswordMutation=no legacyHudMutation=no legacyGameContinuationMutation=no worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)sizeof(EspMapPasswordOwnerState),
           (unsigned)sizeof(EspMapPasswordSubmitResult));

    probeState.done = 1;
}
