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
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_status_message.h"
#include "esp_map_ui_intent.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
#include "native_map1_events_probe.h"
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
#define EXPECTED_FORCE_REFS 3U
#define EXPECTED_FORCE_SET_REFS 1U
#define EXPECTED_FORCE_CLEAR_REFS 2U
#define EXPECTED_SET_STRING_ID 1U
#define EXPECTED_SET_STRING_FNV 0xf6da01bbU
#define EXPECTED_OWNER_BYTES 8U
#define SCRATCH_CAPACITY 314U
#define SCRATCH_STORAGE_BYTES (SCRATCH_CAPACITY + 2U)

typedef struct Esp32Map1StatusMessageProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1StatusMessageProbeState;

typedef struct StatusMessageAudit_s {
    uint32_t applyFNV;
    uint32_t refs;
    uint32_t setRefs;
    uint32_t clearRefs;
    uint32_t guardChecks;
    uint32_t stateExecutorRefused;
    uint32_t transitionProof;
    uint32_t unsupportedRefused;
    uint32_t badFlagsRefused;
    uint32_t badRefRefused;
    uint32_t shortBufferRefused;
    uint32_t nullIntentRefused;
    uint32_t closedPackRefused;
    uint32_t setPayloadFNV;
    EspMapUiIntent setIntent;
    EspMapUiIntent clearIntent;
    EspMapUiIntent unsupportedIntent;
    uint8_t haveSet;
    uint8_t haveClear;
    uint8_t haveUnsupported;
} StatusMessageAudit;

static Esp32Map1StatusMessageProbeState probeState;

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

static uint32_t statusStateHash(const EspMapStatusMessageState* state) {
    uint32_t hash = 2166136261U;

    if (state == NULL) return 0U;
    hash = hashByte(hash, state->active);
    hash = hashU16(hash, state->text.index);
    hash = hashU16(hash, state->text.sourceOffset);
    return hashU16(hash, state->text.length);
}

static int sameStatusState(const EspMapStatusMessageState* a,
                           const EspMapStatusMessageState* b) {
    return a != NULL && b != NULL &&
           a->active == b->active &&
           a->text.index == b->text.index &&
           a->text.sourceOffset == b->text.sourceOffset &&
           a->text.length == b->text.length;
}

static int statusStateIsClear(const EspMapStatusMessageState* state) {
    EspMapStringRef ref;

    return state != NULL && !EspMapStatusMessage_isActive(state) &&
           !EspMapStatusMessage_getRef(state, &ref) &&
           ref.index == 0U && ref.sourceOffset == 0U && ref.length == 0U;
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
           Esp32Map1OpcodeExecProbe_isDone() && Esp32Map1UiIntentProbe_isDone() &&
           Esp32Map1StringReaderProbe_isDone() && runtime != NULL &&
           mapState != NULL && scriptState != NULL &&
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
           !EspAssetPack_isOpen() && !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() && doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) && legacyRuntimeIsClear(doomRpg->render) &&
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

static int applyAndCheck(const EspAssetPackEntry* entry,
                         const EspMapUiIntent* intent,
                         EspMapStatusMessageState* state,
                         uint8_t storage[SCRATCH_STORAGE_BYTES],
                         size_t* outLength,
                         EspMapStatusMessageApplyStatus* outStatus) {
    char* scratch;
    EspMapStatusMessageApplyStatus status;

    if (entry == NULL || intent == NULL || state == NULL || storage == NULL ||
        outLength == NULL || outStatus == NULL) return 0;

    scratch = (char*)&storage[1];
    storage[0] = 0xa5U;
    storage[SCRATCH_STORAGE_BYTES - 1U] = 0x5aU;
    memset(scratch, 0xcc, SCRATCH_CAPACITY);
    *outLength = (size_t)-1;

    status = EspMapStatusMessage_apply(state, entry, intent, scratch,
                                       SCRATCH_CAPACITY, outLength);
    *outStatus = status;
    return storage[0] == 0xa5U &&
           storage[SCRATCH_STORAGE_BYTES - 1U] == 0x5aU;
}

static int auditStatusMessages(const EspAssetPackEntry* entry,
                               StatusMessageAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapUiIntent intent;
    EspMapStatusMessageState state;
    EspMapStatusMessageState snapshot;
    EspMapStringRef activeRef;
    uint8_t storage[SCRATCH_STORAGE_BYTES];
    char* scratch = (char*)&storage[1];
    size_t readLength;
    EspMapStatusMessageApplyStatus applyStatus;
    uint32_t hash = 2166136261U;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t beforeHash;
    uint32_t afterHash;
    uint32_t payloadFNV;

    if (entry == NULL || audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    EspMapStatusMessage_reset(&state);
    if (!statusStateIsClear(&state)) return 0;

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;

        for (commandOffset = 0U;
             commandOffset < descriptor.commandCount;
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

            if (command.id != ESP_MAP_OPCODE_FORCE_MESSAGE) continue;

            if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
                    ESP_MAP_UI_INTENT_OK ||
                intent.status != ESP_MAP_UI_INTENT_OK ||
                intent.kind != ESP_MAP_UI_INTENT_FORCE_MESSAGE ||
                intent.flags != ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY ||
                intent.arg1 != (uint32_t)intent.text.index) return 0;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;

            beforeHash = statusStateHash(&state);
            if (!applyAndCheck(entry, &intent, &state, storage,
                               &readLength, &applyStatus) ||
                applyStatus != ESP_MAP_STATUS_MESSAGE_APPLY_OK ||
                readLength != intent.text.length ||
                scratch[intent.text.length] != '\0') return 0;
            ++audit->guardChecks;

            payloadFNV = fnv1a32((const uint8_t*)scratch, intent.text.length);
            if (scratch[0] == '\0') {
                ++audit->clearRefs;
                if (!statusStateIsClear(&state)) return 0;
                if (!audit->haveClear) {
                    audit->clearIntent = intent;
                    audit->haveClear = 1U;
                }
            }
            else {
                ++audit->setRefs;
                if (!EspMapStatusMessage_isActive(&state) ||
                    !EspMapStatusMessage_getRef(&state, &activeRef) ||
                    activeRef.index != intent.text.index ||
                    activeRef.sourceOffset != intent.text.sourceOffset ||
                    activeRef.length != intent.text.length) return 0;
                audit->setPayloadFNV = payloadFNV;
                if (!audit->haveSet) {
                    audit->setIntent = intent;
                    audit->haveSet = 1U;
                }
            }

            afterHash = statusStateHash(&state);
            hash = hashU16(hash, intent.globalCommandIndex);
            hash = hashU16(hash, intent.text.index);
            hash = hashU16(hash, intent.text.sourceOffset);
            hash = hashU16(hash, intent.text.length);
            hash = hashU32(hash, beforeHash);
            hash = hashU32(hash, afterHash);
            hash = hashU32(hash, payloadFNV);
            ++audit->refs;
        }
    }

    if (audit->refs != EXPECTED_FORCE_REFS ||
        audit->setRefs != EXPECTED_FORCE_SET_REFS ||
        audit->clearRefs != EXPECTED_FORCE_CLEAR_REFS ||
        audit->guardChecks != EXPECTED_FORCE_REFS ||
        audit->stateExecutorRefused != EXPECTED_FORCE_REFS ||
        !audit->haveSet || !audit->haveClear || !audit->haveUnsupported ||
        audit->setIntent.text.index != EXPECTED_SET_STRING_ID ||
        audit->setPayloadFNV != EXPECTED_SET_STRING_FNV) return 0;

    /* Exact recovered transition: non-empty sets the immutable ref, then an
     * empty C string clears it. */
    EspMapStatusMessage_reset(&state);
    if (!applyAndCheck(entry, &audit->setIntent, &state, storage,
                       &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_STATUS_MESSAGE_APPLY_OK ||
        !EspMapStatusMessage_isActive(&state) ||
        !applyAndCheck(entry, &audit->clearIntent, &state, storage,
                       &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_STATUS_MESSAGE_APPLY_OK ||
        !statusStateIsClear(&state)) return 0;
    audit->transitionProof = 1U;

    /* Re-establish a real non-empty owner state, then prove every refused path
     * leaves that owner bit-for-bit semantically unchanged. */
    if (!applyAndCheck(entry, &audit->setIntent, &state, storage,
                       &readLength, &applyStatus) ||
        applyStatus != ESP_MAP_STATUS_MESSAGE_APPLY_OK ||
        !EspMapStatusMessage_isActive(&state)) return 0;
    snapshot = state;

    readLength = 123U;
    if (EspMapStatusMessage_apply(&state, entry, &audit->unsupportedIntent,
                                  scratch, SCRATCH_CAPACITY, &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_UNSUPPORTED ||
        readLength != 0U || !sameStatusState(&state, &snapshot)) return 0;
    audit->unsupportedRefused = 1U;

    intent = audit->setIntent;
    intent.flags = 0U;
    readLength = 123U;
    if (EspMapStatusMessage_apply(&state, entry, &intent,
                                  scratch, SCRATCH_CAPACITY, &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_INVALID ||
        readLength != 0U || !sameStatusState(&state, &snapshot)) return 0;
    audit->badFlagsRefused = 1U;

    intent = audit->setIntent;
    ++intent.text.sourceOffset;
    readLength = 123U;
    if (EspMapStatusMessage_apply(&state, entry, &intent,
                                  scratch, SCRATCH_CAPACITY, &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_INVALID ||
        readLength != 0U || !sameStatusState(&state, &snapshot)) return 0;
    audit->badRefRefused = 1U;

    readLength = 123U;
    if (EspMapStatusMessage_apply(&state, entry, &audit->setIntent,
                                  scratch, audit->setIntent.text.length,
                                  &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_BUFFER_TOO_SMALL ||
        readLength != 0U || !sameStatusState(&state, &snapshot)) return 0;
    audit->shortBufferRefused = 1U;

    readLength = 123U;
    if (EspMapStatusMessage_apply(&state, entry, NULL,
                                  scratch, SCRATCH_CAPACITY, &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_INVALID ||
        readLength != 0U || !sameStatusState(&state, &snapshot)) return 0;
    audit->nullIntentRefused = 1U;

    EspMapStatusMessage_reset(&state);
    if (!statusStateIsClear(&state)) return 0;

    audit->applyFNV = hash;
    return audit->transitionProof == 1U && audit->unsupportedRefused == 1U &&
           audit->badFlagsRefused == 1U && audit->badRefRefused == 1U &&
           audit->shortBufferRefused == 1U && audit->nullIntentRefused == 1U;
}

void Esp32Map1StatusMessageProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1StatusMessageProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    EspAssetPackEntry entry = {0};
    StatusMessageAudit audit;
    EspMapStatusMessageState closedState;
    EspMapStatusMessageState closedSnapshot;
    uint8_t scratch[SCRATCH_CAPACITY];
    size_t readLength;
    const char* mapFile;
    uint32_t heapBefore, heapOpen, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter;
    uint32_t scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter;
    uint32_t started, elapsed;
    char* statBarBefore;
    int skipAdvanceBefore, saveTileBefore, tileEventBefore;
    int tileEventIndexBefore, tileEventFlagsBefore;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1StringReaderProbe_isDone()) return;
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPSTATUSPROBE] ARMED bounded native string reader proven; EV_FORCEMESSAGE status-owner execution starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO FORCE_MESSAGE owner ===\n");
    printf("[MAPSTATUSPROBE] CONTRACT consume only EV_FORCEMESSAGE intent + bounded reader -> 8B caller-owned ref state; exact first-byte set/clear semantics; no legacy Hud/world/render mutation\n");

    if (!boundaryIsSafe(doomRpg) ||
        sizeof(EspMapStatusMessageState) != EXPECTED_OWNER_BYTES) {
        printf("[MAPSTATUSPROBE] FAILED precondition heap8=%u largest8=%u packOpen=%d ownerBytes=%u\n",
               (unsigned int)heap8Free(), (unsigned int)largest8Block(),
               EspAssetPack_isOpen(),
               (unsigned int)sizeof(EspMapStatusMessageState));
        return;
    }

    mapFile = doomRpg->game->mapFiles[doomRpg->doomCanvas->startupMap - 1];
    if (mapFile == NULL || SDL_strcasecmp(mapFile, "/intro.bsp") != 0) {
        printf("[MAPSTATUSPROBE] FAILED startup map resolves to '%s'\n",
               mapFile != NULL ? mapFile : "<null>");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = fnv1a32(mapState->tileFlags, mapState->tileCount);
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    statBarBefore = doomRpg->hud->statBarMessage;
    skipAdvanceBefore = doomRpg->game->skipAdvanceTurn;
    saveTileBefore = doomRpg->game->saveTileEvent;
    tileEventBefore = doomRpg->game->tileEvent;
    tileEventIndexBefore = doomRpg->game->tileEventIndex;
    tileEventFlagsBefore = doomRpg->game->tileEventFlags;
    started = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPSTATUSPROBE] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry(mapFile, &entry) ||
        entry.size != runtime->sourceBytes || entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != runtime->sourceCrc32 || entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !auditStatusMessages(&entry, &audit)) {
        printf("[MAPSTATUSPROBE] FAILED source/status audit entrySize=%u crc=%08x\n",
               (unsigned int)entry.size, (unsigned int)entry.crc32);
        EspAssetPack_close();
        return;
    }

    EspMapStatusMessage_reset(&closedState);
    readLength = 0U;
    if (EspMapStatusMessage_apply(&closedState, &entry, &audit.setIntent,
                                  (char*)scratch, sizeof(scratch), &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_OK ||
        !EspMapStatusMessage_isActive(&closedState)) {
        printf("[MAPSTATUSPROBE] FAILED preparing closed-pack atomicity state\n");
        EspAssetPack_close();
        return;
    }
    closedSnapshot = closedState;
    EspAssetPack_close();

    readLength = 123U;
    if (EspMapStatusMessage_apply(&closedState, &entry, &audit.setIntent,
                                  (char*)scratch, sizeof(scratch), &readLength) !=
            ESP_MAP_STATUS_MESSAGE_APPLY_IO_ERROR ||
        readLength != 0U || !sameStatusState(&closedState, &closedSnapshot)) {
        printf("[MAPSTATUSPROBE] FAILED closed-pack atomic refusal\n");
        return;
    }
    audit.closedPackRefused = 1U;
    EspMapStatusMessage_reset(&closedState);

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState != NULL ? fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfter = scriptState != NULL ? fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                            (uint32_t)sizeof(doomRpg->player->NotebookString));

    if (EspAssetPack_isOpen() || !boundaryIsSafe(doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV || scriptAfter != scriptBefore ||
        scriptAfter != EXPECTED_SCRIPT_FNV || notebookAfter != notebookBefore ||
        audit.closedPackRefused != 1U || doomRpg->hud->statBarMessage != statBarBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceBefore ||
        doomRpg->game->saveTileEvent != saveTileBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore) {
        printf("[MAPSTATUSPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x notebook=%08x->%08x packOpen=%d\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
               (unsigned int)scriptBefore, (unsigned int)scriptAfter,
               (unsigned int)notebookBefore, (unsigned int)notebookAfter,
               EspAssetPack_isOpen());
        return;
    }

    probeState.done = 1;
    printf("[MAPSTATUS] READY refs=%u set=%u clear=%u transition=%u ownerBytes=%u textCopyBytes=0 stateExecRefused=%u applyFNV=%08x\n",
           (unsigned int)audit.refs, (unsigned int)audit.setRefs,
           (unsigned int)audit.clearRefs, (unsigned int)audit.transitionProof,
           (unsigned int)sizeof(EspMapStatusMessageState),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.applyFNV);
    printf("[MAPSTATUS] SAMPLE set cmd=%u event=%u off=%u string=%u@%u+%u payloadFNV=%08x clear cmd=%u event=%u off=%u string=%u@%u+%u\n",
           (unsigned int)audit.setIntent.globalCommandIndex,
           (unsigned int)audit.setIntent.sourceEventIndex,
           (unsigned int)audit.setIntent.sourceCommandOffset,
           (unsigned int)audit.setIntent.text.index,
           (unsigned int)audit.setIntent.text.sourceOffset,
           (unsigned int)audit.setIntent.text.length,
           (unsigned int)audit.setPayloadFNV,
           (unsigned int)audit.clearIntent.globalCommandIndex,
           (unsigned int)audit.clearIntent.sourceEventIndex,
           (unsigned int)audit.clearIntent.sourceCommandOffset,
           (unsigned int)audit.clearIntent.text.index,
           (unsigned int)audit.clearIntent.text.sourceOffset,
           (unsigned int)audit.clearIntent.text.length);
    printf("[MAPSTATUS] FAILCLOSED unsupported=%u badFlags=%u badRef=%u shortBuffer=%u nullIntent=%u closedPack=%u ownerAtomic=yes\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badFlagsRefused,
           (unsigned int)audit.badRefRefused,
           (unsigned int)audit.shortBufferRefused,
           (unsigned int)audit.nullIntentRefused,
           (unsigned int)audit.closedPackRefused);
    printf("[MAPSTATUS] IO entry=%s size=%u crc32=%08x heapOpen=%u transientHeapCost=%d largestOpen=%u elapsed=%ums heapPersistentBytes=0\n",
           mapFile, (unsigned int)entry.size, (unsigned int)entry.crc32,
           (unsigned int)heapOpen, (int)heapBefore - (int)heapOpen,
           (unsigned int)largestOpen, (unsigned int)elapsed);
    printf("[MAPSTATUSPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x notebookFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter,
           (unsigned int)notebookBefore, (unsigned int)notebookAfter);
    printf("[MAPSTATUSPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes ownerValueBytes=%u textCopyBytes=0 legacyHudMutation=no worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspMapStatusMessageState),
           doomRpg->game->numEntities, doomRpg->game->numMonsters);
}

int Esp32Map1StatusMessageProbe_isDone(void) {
    return probeState.done;
}
