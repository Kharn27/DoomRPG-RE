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
#include "esp_map_notebook.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_ui_intent.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_dialog_owner_probe.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
#include "native_map1_events_probe.h"
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
#define EXPECTED_NOTE_REFS 7U
#define EXPECTED_OWNER_BYTES 514U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U

#define EXPECTED_SAMPLE_CMD 103U
#define EXPECTED_SAMPLE_EVENT 40U
#define EXPECTED_SAMPLE_OFF 8U
#define EXPECTED_SAMPLE_STRING 85U
#define EXPECTED_SAMPLE_STRING_OFFSET 18964U
#define EXPECTED_SAMPLE_STRING_LENGTH 54U
#define EXPECTED_SAMPLE_PAYLOAD_FNV 0xee639dc1U

#define SCRATCH_CAPACITY 314U
#define SCRATCH_STORAGE_BYTES (SCRATCH_CAPACITY + 2U)

typedef struct Esp32Map1NotebookProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1NotebookProbeState;

typedef struct NotebookAudit_s {
    uint32_t applyFNV;
    uint32_t refs;
    uint32_t separatorRefs;
    uint32_t appendMatches;
    uint32_t guardChecks;
    uint32_t stateExecutorRefused;
    uint32_t totalSourceBytes;
    uint32_t samplePayloadFNV;
    uint32_t finalContentFNV;
    uint32_t finalStorageFNV;
    uint16_t finalLength;
    uint32_t separatorProof;
    uint32_t truncationProof;
    uint32_t fullStableProof;
    uint32_t unsupportedRefused;
    uint32_t badFlagsRefused;
    uint32_t badKindRefused;
    uint32_t badRefRefused;
    uint32_t badEventRefused;
    uint32_t badGlobalRefused;
    uint32_t shortBufferRefused;
    uint32_t nullIntentRefused;
    uint32_t closedPackRefused;
    uint32_t resetProof;
    EspMapUiIntent sampleIntent;
    EspMapUiIntent unsupportedIntent;
    EspMapNotebookState finalState;
    uint8_t haveSample;
    uint8_t haveUnsupported;
} NotebookAudit;

static Esp32Map1NotebookProbeState probeState;

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

static uint32_t notebookStateHash(const EspMapNotebookState* state) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (state == NULL) return 0U;
    hash = hashU16(hash, state->length);
    for (i = 0U; i < ESP_MAP_NOTEBOOK_CAPACITY; ++i) {
        hash = hashByte(hash, (uint8_t)state->text[i]);
    }
    return hash;
}

static int sameNotebook(const EspMapNotebookState* a,
                        const EspMapNotebookState* b) {
    return a != NULL && b != NULL && a->length == b->length &&
           memcmp(a->text, b->text, ESP_MAP_NOTEBOOK_CAPACITY) == 0;
}

static int notebookIsClear(const EspMapNotebookState* state) {
    uint32_t i;

    if (state == NULL || state->length != 0U || state->text[0] != '\0') {
        return 0;
    }
    for (i = 0U; i < ESP_MAP_NOTEBOOK_CAPACITY; ++i) {
        if (state->text[i] != '\0') return 0;
    }
    return 1;
}

static size_t boundedCStringLength(const char* text, size_t capacity) {
    size_t i;
    if (text == NULL) return 0U;
    for (i = 0U; i < capacity; ++i) {
        if (text[i] == '\0') return i;
    }
    return capacity;
}

static void simulateAppend(EspMapNotebookState* state,
                           const char* source,
                           size_t sourceBytes) {
    size_t sourceLength;
    size_t available;
    size_t copyBytes;
    uint16_t length;

    if (state == NULL || source == NULL) return;
    sourceLength = boundedCStringLength(source, sourceBytes);
    length = state->length;
    available = (ESP_MAP_NOTEBOOK_CAPACITY - 1U) - (size_t)length;
    copyBytes = sourceLength < available ? sourceLength : available;
    if (copyBytes != 0U) {
        memcpy(&state->text[length], source, copyBytes);
        length = (uint16_t)(length + copyBytes);
    }
    if ((size_t)length < ESP_MAP_NOTEBOOK_CAPACITY - 1U) {
        state->text[length++] = '|';
    }
    if ((size_t)length < ESP_MAP_NOTEBOOK_CAPACITY - 1U) {
        state->text[length++] = '|';
    }
    state->text[length] = '\0';
    state->length = length;
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

static int applyWithGuard(const EspAssetPackEntry* entry,
                          const EspMapUiIntent* intent,
                          EspMapNotebookState* state,
                          uint8_t storage[SCRATCH_STORAGE_BYTES],
                          size_t capacity,
                          size_t* outLength,
                          EspMapNotebookApplyStatus* outStatus) {
    char* scratch = (char*)&storage[1];

    if (state == NULL || storage == NULL || outLength == NULL ||
        outStatus == NULL || capacity > SCRATCH_CAPACITY) return 0;
    storage[0] = 0xa5U;
    storage[SCRATCH_STORAGE_BYTES - 1U] = 0x5aU;
    memset(scratch, 0xcc, SCRATCH_CAPACITY);
    *outLength = (size_t)-1;
    *outStatus = EspMapNotebook_apply(state, entry, intent, scratch,
                                      capacity, outLength);
    return storage[0] == 0xa5U &&
           storage[SCRATCH_STORAGE_BYTES - 1U] == 0x5aU;
}

static int refusedWithoutMutation(const EspAssetPackEntry* entry,
                                  const EspMapUiIntent* intent,
                                  EspMapNotebookState* state,
                                  uint8_t storage[SCRATCH_STORAGE_BYTES],
                                  size_t capacity,
                                  EspMapNotebookApplyStatus expectedStatus) {
    EspMapNotebookState before;
    EspMapNotebookApplyStatus status;
    size_t readLength;

    if (state == NULL || storage == NULL) return 0;
    before = *state;
    if (!applyWithGuard(entry, intent, state, storage, capacity,
                        &readLength, &status)) return 0;
    return status == expectedStatus && sameNotebook(&before, state);
}

static int auditNotes(const EspAssetPackEntry* entry, NotebookAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapUiIntent intent;
    EspMapUiIntent bad;
    EspMapNotebookState state;
    EspMapNotebookState expected;
    uint8_t storage[SCRATCH_STORAGE_BYTES];
    char* scratch = (char*)&storage[1];
    size_t readLength;
    EspMapNotebookApplyStatus applyStatus;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t beforeHash;
    uint32_t afterHash;
    uint32_t payloadFNV;
    uint32_t hash = 2166136261U;

    if (entry == NULL || audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    EspMapNotebook_reset(&state);
    if (!notebookIsClear(&state) || sizeof(state) != EXPECTED_OWNER_BYTES) {
        return 0;
    }

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }
            if (!audit->haveUnsupported &&
                command.id == ESP_MAP_OPCODE_DIALOG) {
                if (EspMapUiIntent_build(&descriptor, commandOffset,
                                         &audit->unsupportedIntent) !=
                    ESP_MAP_UI_INTENT_OK) return 0;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_NOTE) continue;

            if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
                    ESP_MAP_UI_INTENT_OK ||
                intent.status != ESP_MAP_UI_INTENT_OK ||
                intent.kind != ESP_MAP_UI_INTENT_APPEND_NOTE ||
                intent.flags != ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR ||
                intent.arg1 != (uint32_t)intent.text.index ||
                intent.sourceEventIndex != descriptor.eventIndex ||
                intent.sourceCommandOffset != commandOffset ||
                intent.globalCommandIndex !=
                    (uint16_t)((uint32_t)descriptor.firstCommandIndex +
                               commandOffset)) return 0;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;

            expected = state;
            beforeHash = notebookStateHash(&state);
            if (!applyWithGuard(entry, &intent, &state, storage,
                                SCRATCH_CAPACITY, &readLength, &applyStatus) ||
                applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK ||
                readLength != intent.text.length ||
                scratch[readLength] != '\0') return 0;
            ++audit->guardChecks;

            simulateAppend(&expected, scratch, readLength);
            if (!sameNotebook(&expected, &state) ||
                EspMapNotebook_length(&state) != state.length ||
                EspMapNotebook_text(&state) != state.text) return 0;
            ++audit->appendMatches;

            payloadFNV = fnv1a32((const uint8_t*)scratch,
                                 (uint32_t)readLength);
            afterHash = notebookStateHash(&state);
            hash = hashU16(hash, intent.globalCommandIndex);
            hash = hashU16(hash, intent.sourceEventIndex);
            hash = hashByte(hash, intent.sourceCommandOffset);
            hash = hashU16(hash, intent.text.index);
            hash = hashU16(hash, intent.text.sourceOffset);
            hash = hashU16(hash, intent.text.length);
            hash = hashU32(hash, payloadFNV);
            hash = hashU32(hash, beforeHash);
            hash = hashU32(hash, afterHash);

            ++audit->refs;
            ++audit->separatorRefs;
            audit->totalSourceBytes += (uint32_t)readLength;

            if (!audit->haveSample &&
                intent.globalCommandIndex == EXPECTED_SAMPLE_CMD) {
                audit->sampleIntent = intent;
                audit->samplePayloadFNV = payloadFNV;
                audit->haveSample = 1U;
            }
        }
    }

    if (audit->refs != EXPECTED_NOTE_REFS ||
        audit->separatorRefs != EXPECTED_NOTE_REFS ||
        audit->appendMatches != EXPECTED_NOTE_REFS ||
        audit->guardChecks != EXPECTED_NOTE_REFS ||
        audit->stateExecutorRefused != EXPECTED_NOTE_REFS ||
        !audit->haveSample || !audit->haveUnsupported ||
        audit->sampleIntent.sourceEventIndex != EXPECTED_SAMPLE_EVENT ||
        audit->sampleIntent.sourceCommandOffset != EXPECTED_SAMPLE_OFF ||
        audit->sampleIntent.text.index != EXPECTED_SAMPLE_STRING ||
        audit->sampleIntent.text.sourceOffset != EXPECTED_SAMPLE_STRING_OFFSET ||
        audit->sampleIntent.text.length != EXPECTED_SAMPLE_STRING_LENGTH ||
        audit->samplePayloadFNV != EXPECTED_SAMPLE_PAYLOAD_FNV) return 0;

    audit->applyFNV = hash;
    audit->finalLength = state.length;
    audit->finalContentFNV =
        fnv1a32((const uint8_t*)state.text, (uint32_t)state.length);
    audit->finalStorageFNV =
        fnv1a32((const uint8_t*)state.text, ESP_MAP_NOTEBOOK_CAPACITY);
    audit->finalState = state;

    if (!refusedWithoutMutation(entry, &audit->unsupportedIntent, &state,
                                storage, SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_UNSUPPORTED)) return 0;
    audit->unsupportedRefused = 1U;

    bad = audit->sampleIntent;
    bad.flags = 0U;
    if (!refusedWithoutMutation(entry, &bad, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->badFlagsRefused = 1U;

    bad = audit->sampleIntent;
    bad.kind = ESP_MAP_UI_INTENT_DIALOG;
    if (!refusedWithoutMutation(entry, &bad, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->badKindRefused = 1U;

    bad = audit->sampleIntent;
    ++bad.text.sourceOffset;
    if (!refusedWithoutMutation(entry, &bad, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->badRefRefused = 1U;

    bad = audit->sampleIntent;
    bad.sourceEventIndex = 0xffffU;
    if (!refusedWithoutMutation(entry, &bad, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->badEventRefused = 1U;

    bad = audit->sampleIntent;
    ++bad.globalCommandIndex;
    if (!refusedWithoutMutation(entry, &bad, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->badGlobalRefused = 1U;

    if (!refusedWithoutMutation(entry, &audit->sampleIntent, &state,
                                storage, audit->sampleIntent.text.length,
                                ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL)) return 0;
    audit->shortBufferRefused = 1U;

    if (!refusedWithoutMutation(entry, NULL, &state, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_INVALID)) return 0;
    audit->nullIntentRefused = 1U;

    EspMapNotebook_reset(&state);
    if (!applyWithGuard(entry, &audit->sampleIntent, &state, storage,
                        SCRATCH_CAPACITY, &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK ||
        readLength != EXPECTED_SAMPLE_STRING_LENGTH ||
        state.length != EXPECTED_SAMPLE_STRING_LENGTH + 2U ||
        state.text[EXPECTED_SAMPLE_STRING_LENGTH] != '|' ||
        state.text[EXPECTED_SAMPLE_STRING_LENGTH + 1U] != '|' ||
        state.text[EXPECTED_SAMPLE_STRING_LENGTH + 2U] != '\0') return 0;
    audit->separatorProof = 1U;

    EspMapNotebook_reset(&state);
    memset(state.text, 'X', ESP_MAP_NOTEBOOK_CAPACITY - 2U);
    state.length = ESP_MAP_NOTEBOOK_CAPACITY - 2U;
    state.text[state.length] = '\0';
    state.text[ESP_MAP_NOTEBOOK_CAPACITY - 1U] = '\0';
    if (!applyWithGuard(entry, &audit->sampleIntent, &state, storage,
                        SCRATCH_CAPACITY, &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK ||
        scratch[0] == '\0' ||
        state.length != ESP_MAP_NOTEBOOK_CAPACITY - 1U ||
        state.text[ESP_MAP_NOTEBOOK_CAPACITY - 2U] != scratch[0] ||
        state.text[ESP_MAP_NOTEBOOK_CAPACITY - 1U] != '\0') return 0;
    audit->truncationProof = 1U;

    expected = state;
    if (!applyWithGuard(entry, &audit->sampleIntent, &state, storage,
                        SCRATCH_CAPACITY, &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK ||
        !sameNotebook(&expected, &state)) return 0;
    audit->fullStableProof = 1U;

    return 1;
}

void Esp32Map1NotebookProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32Map1NotebookProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1NotebookProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    EspAssetPackEntry entry;
    NotebookAudit audit;
    EspMapNotebookState closedPackState;
    uint8_t storage[SCRATCH_STORAGE_BYTES];
    uint32_t heapBefore, heapOpen, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter;
    uint32_t scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter;
    uint32_t started, elapsed;
    char* hudMessageBefore;
    int skipAdvanceTurnBefore;
    int saveTileEventBefore;
    int tileEventBefore;
    int tileEventIndexBefore;
    int tileEventFlagsBefore;

    if (probeState.done || probeState.attempted) return;
    if (!probeState.armed) {
        if (Esp32Map1DialogOwnerProbe_isDone()) {
            probeState.armed = 1;
            printf("[MAPNOTEPROBE] ARMED native DIALOG/NOBACK owner proven; EV_NOTE notebook execution starts on next loop service\n");
        }
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO NOTE notebook owner ===\n");
    printf("[MAPNOTEPROBE] CONTRACT consume only EV_NOTE intents + bounded reader -> 512B map-local text + length; append exact text+|| with 511B truncation; no legacy Player/world/render mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPNOTEPROBE] FAILED unsafe precondition\n");
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
    hudMessageBefore = doomRpg->hud->statBarMessage;
    skipAdvanceTurnBefore = doomRpg->game->skipAdvanceTurn;
    saveTileEventBefore = doomRpg->game->saveTileEvent;
    tileEventBefore = doomRpg->game->tileEvent;
    tileEventIndexBefore = doomRpg->game->tileEventIndex;
    tileEventFlagsBefore = doomRpg->game->tileEventFlags;
    started = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPNOTEPROBE] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry("/intro.bsp", &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        !auditNotes(&entry, &audit)) {
        EspAssetPack_close();
        printf("[MAPNOTEPROBE] FAILED native NOTE audit\n");
        return;
    }

    EspAssetPack_close();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    closedPackState = audit.finalState;
    if (!refusedWithoutMutation(&entry, &audit.sampleIntent,
                                &closedPackState, storage,
                                SCRATCH_CAPACITY,
                                ESP_MAP_NOTEBOOK_APPLY_IO_ERROR)) {
        printf("[MAPNOTEPROBE] FAILED closed-pack atomicity\n");
        return;
    }
    audit.closedPackRefused = 1U;
    EspMapNotebook_reset(&closedPackState);
    if (!notebookIsClear(&closedPackState)) {
        printf("[MAPNOTEPROBE] FAILED reset\n");
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

    if (audit.refs != EXPECTED_NOTE_REFS ||
        audit.separatorRefs != EXPECTED_NOTE_REFS ||
        audit.appendMatches != EXPECTED_NOTE_REFS ||
        audit.guardChecks != EXPECTED_NOTE_REFS ||
        audit.stateExecutorRefused != EXPECTED_NOTE_REFS ||
        audit.separatorProof != 1U || audit.truncationProof != 1U ||
        audit.fullStableProof != 1U || audit.unsupportedRefused != 1U ||
        audit.badFlagsRefused != 1U || audit.badKindRefused != 1U ||
        audit.badRefRefused != 1U || audit.badEventRefused != 1U ||
        audit.badGlobalRefused != 1U || audit.shortBufferRefused != 1U ||
        audit.nullIntentRefused != 1U || audit.closedPackRefused != 1U ||
        audit.resetProof != 1U || sizeof(EspMapNotebookState) != EXPECTED_OWNER_BYTES ||
        EspAssetPack_isOpen() || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || mapStateAfter != mapStateBefore ||
        scriptAfter != scriptBefore || notebookBefore != EXPECTED_LEGACY_NOTEBOOK_FNV ||
        notebookAfter != notebookBefore ||
        doomRpg->hud->statBarMessage != hudMessageBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceTurnBefore ||
        doomRpg->game->saveTileEvent != saveTileEventBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->doomCanvas->state != ST_INTRO) {
        printf("[MAPNOTEPROBE] FAILED acceptance/integrity\n");
        return;
    }

    printf("[MAPNOTE] READY refs=%u separators=%u appendMatches=%u ownerBytes=%u textCapacity=%u stateExecRefused=%u sourceBytes=%u finalLen=%u noteApplyFNV=%08x contentFNV=%08x storageFNV=%08x elapsed=%ums\n",
           (unsigned)audit.refs, (unsigned)audit.separatorRefs,
           (unsigned)audit.appendMatches, (unsigned)sizeof(EspMapNotebookState),
           (unsigned)ESP_MAP_NOTEBOOK_CAPACITY,
           (unsigned)audit.stateExecutorRefused,
           (unsigned)audit.totalSourceBytes, (unsigned)audit.finalLength,
           (unsigned)audit.applyFNV, (unsigned)audit.finalContentFNV,
           (unsigned)audit.finalStorageFNV, (unsigned)elapsed);
    printf("[MAPNOTE] SAMPLE cmd=%u event=%u off=%u string=%u@%u+%u payloadFNV=%08x\n",
           (unsigned)audit.sampleIntent.globalCommandIndex,
           (unsigned)audit.sampleIntent.sourceEventIndex,
           (unsigned)audit.sampleIntent.sourceCommandOffset,
           (unsigned)audit.sampleIntent.text.index,
           (unsigned)audit.sampleIntent.text.sourceOffset,
           (unsigned)audit.sampleIntent.text.length,
           (unsigned)audit.samplePayloadFNV);
    printf("[MAPNOTE] BOUNDS separator=%u truncation=%u fullStable=%u guards=%u/%u terminator=yes\n",
           (unsigned)audit.separatorProof, (unsigned)audit.truncationProof,
           (unsigned)audit.fullStableProof, (unsigned)audit.guardChecks,
           (unsigned)EXPECTED_NOTE_REFS);
    printf("[MAPNOTE] FAILCLOSED unsupported=%u badFlags=%u badKind=%u badRef=%u badEvent=%u badGlobal=%u shortBuffer=%u nullIntent=%u closedPack=%u ownerAtomic=yes reset=%u\n",
           (unsigned)audit.unsupportedRefused, (unsigned)audit.badFlagsRefused,
           (unsigned)audit.badKindRefused, (unsigned)audit.badRefRefused,
           (unsigned)audit.badEventRefused, (unsigned)audit.badGlobalRefused,
           (unsigned)audit.shortBufferRefused, (unsigned)audit.nullIntentRefused,
           (unsigned)audit.closedPackRefused, (unsigned)audit.resetProof);
    printf("[MAPNOTE] IO entry=/intro.bsp size=%u crc32=%08x heapOpen=%u transientHeapCost=%d largestOpen=%u packIO=yes persistentHeapBytes=0\n",
           (unsigned)entry.size, (unsigned)entry.crc32, (unsigned)heapOpen,
           (int)heapBefore - (int)heapOpen, (unsigned)largestOpen);
    printf("[MAPNOTEPROBE] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x legacyNotebookFNV=%08x->%08x\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (int)heapAfter - (int)heapBefore,
           (unsigned)largestBefore, (unsigned)largestAfter,
           (int)largestAfter - (int)largestBefore,
           (unsigned)frameBefore, (unsigned)frameAfter,
           (unsigned)arenaBefore, (unsigned)arenaAfter,
           (unsigned)mapStateBefore, (unsigned)mapStateAfter,
           (unsigned)scriptBefore, (unsigned)scriptAfter,
           (unsigned)notebookBefore, (unsigned)notebookAfter);
    printf("[MAPNOTEPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes ownerValueBytes=%u textCapacity=%u legacyNotebookMutation=no legacyHudMutation=no legacyGameContinuationMutation=no worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)sizeof(EspMapNotebookState),
           (unsigned)ESP_MAP_NOTEBOOK_CAPACITY);

    probeState.done = 1;
}
