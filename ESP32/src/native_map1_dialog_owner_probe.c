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
#include "esp_map_dialog_owner.h"
#include "esp_map_events.h"
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
#define EXPECTED_DIALOG_REFS 84U
#define EXPECTED_DIALOG_BACK_REFS 76U
#define EXPECTED_DIALOG_NO_BACK_REFS 8U
#define EXPECTED_OWNER_BYTES 12U

#define EXPECTED_DIALOG_CMD 11U
#define EXPECTED_DIALOG_EVENT 6U
#define EXPECTED_DIALOG_OFF 0U
#define EXPECTED_DIALOG_RESUME 1U
#define EXPECTED_DIALOG_STRING 25U
#define EXPECTED_DIALOG_STRING_OFFSET 13558U
#define EXPECTED_DIALOG_STRING_LENGTH 23U

#define EXPECTED_NOBACK_CMD 19U
#define EXPECTED_NOBACK_EVENT 6U
#define EXPECTED_NOBACK_OFF 8U
#define EXPECTED_NOBACK_RESUME 9U
#define EXPECTED_NOBACK_STRING 30U
#define EXPECTED_NOBACK_STRING_OFFSET 13679U
#define EXPECTED_NOBACK_STRING_LENGTH 14U

typedef struct Esp32Map1DialogOwnerProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1DialogOwnerProbeState;

typedef struct DialogOwnerAudit_s {
    uint32_t applyFNV;
    uint32_t refs;
    uint32_t backRefs;
    uint32_t noBackRefs;
    uint32_t pauseRefs;
    uint32_t skipTurnRefs;
    uint32_t resumeExact;
    uint32_t stateExecutorRefused;
    uint32_t resetProof;
    uint32_t unsupportedRefused;
    uint32_t badFlagsRefused;
    uint32_t badKindRefused;
    uint32_t badRefRefused;
    uint32_t badEventRefused;
    uint32_t badGlobalRefused;
    uint32_t badResumeRefused;
    uint32_t nullIntentRefused;
    EspMapUiIntent dialogSample;
    EspMapUiIntent noBackSample;
    EspMapUiIntent unsupportedIntent;
    uint8_t haveDialog;
    uint8_t haveNoBack;
    uint8_t haveUnsupported;
} DialogOwnerAudit;

static Esp32Map1DialogOwnerProbeState probeState;

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

static uint32_t ownerHash(const EspMapDialogOwnerState* state) {
    uint32_t hash = 2166136261U;
    if (state == NULL) return 0U;
    hash = hashByte(hash, state->active);
    hash = hashU16(hash, state->text.index);
    hash = hashU16(hash, state->text.sourceOffset);
    hash = hashU16(hash, state->text.length);
    hash = hashU16(hash, state->sourceEventIndex);
    hash = hashByte(hash, state->sourceCommandOffset);
    hash = hashByte(hash, state->resumeCommandOffset);
    return hashByte(hash, state->flags);
}

static int sameOwner(const EspMapDialogOwnerState* a,
                     const EspMapDialogOwnerState* b) {
    return a != NULL && b != NULL &&
           a->active == b->active &&
           a->text.index == b->text.index &&
           a->text.sourceOffset == b->text.sourceOffset &&
           a->text.length == b->text.length &&
           a->sourceEventIndex == b->sourceEventIndex &&
           a->sourceCommandOffset == b->sourceCommandOffset &&
           a->resumeCommandOffset == b->resumeCommandOffset &&
           a->flags == b->flags;
}

static int ownerIsClear(const EspMapDialogOwnerState* state) {
    EspMapStringRef ref;
    return state != NULL && !EspMapDialogOwner_isActive(state) &&
           !EspMapDialogOwner_getRef(state, &ref) &&
           ref.index == 0U && ref.sourceOffset == 0U && ref.length == 0U &&
           state->sourceEventIndex == 0U &&
           state->sourceCommandOffset == 0U &&
           state->resumeCommandOffset == 0U && state->flags == 0U;
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
           Esp32Map1StringReaderProbe_isDone() &&
           Esp32Map1StatusMessageProbe_isDone() &&
           runtime != NULL && mapState != NULL && scriptState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           runtime->stringCount == EXPECTED_STRING_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
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

static int validateAppliedOwner(const EspMapUiIntent* intent,
                                const EspMapDialogOwnerState* state) {
    EspMapStringRef ref;
    return intent != NULL && state != NULL &&
           EspMapDialogOwner_isActive(state) &&
           EspMapDialogOwner_getRef(state, &ref) &&
           ref.index == intent->text.index &&
           ref.sourceOffset == intent->text.sourceOffset &&
           ref.length == intent->text.length &&
           state->sourceEventIndex == intent->sourceEventIndex &&
           state->sourceCommandOffset == intent->sourceCommandOffset &&
           state->resumeCommandOffset == intent->resumeCommandOffset &&
           state->flags == intent->flags;
}

static int auditDialogOwners(DialogOwnerAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapUiIntent intent;
    EspMapDialogOwnerState state;
    EspMapDialogOwnerState snapshot;
    uint32_t hash = 2166136261U;
    uint32_t eventIndex, commandOffset, beforeHash, afterHash;
    uint8_t expectedFlags;
    if (audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    EspMapDialogOwner_reset(&state);
    if (!ownerIsClear(&state)) return 0;

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) return 0;
            if (!audit->haveUnsupported &&
                command.id == ESP_MAP_OPCODE_FORCE_MESSAGE) {
                if (EspMapUiIntent_build(&descriptor, commandOffset,
                                         &audit->unsupportedIntent) !=
                    ESP_MAP_UI_INTENT_OK) return 0;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_DIALOG &&
                command.id != ESP_MAP_OPCODE_DIALOG_NO_BACK) continue;

            if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
                    ESP_MAP_UI_INTENT_OK ||
                intent.status != ESP_MAP_UI_INTENT_OK ||
                intent.kind != ESP_MAP_UI_INTENT_DIALOG) return 0;
            expectedFlags = ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
                            ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN;
            if (command.id == ESP_MAP_OPCODE_DIALOG)
                expectedFlags |= ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK;
            if (intent.flags != expectedFlags ||
                intent.sourceEventIndex != descriptor.eventIndex ||
                intent.sourceCommandOffset != commandOffset ||
                (uint16_t)intent.sourceCommandOffset + 1U !=
                    (uint16_t)intent.resumeCommandOffset ||
                intent.globalCommandIndex !=
                    (uint16_t)((uint32_t)descriptor.firstCommandIndex +
                               commandOffset)) return 0;
            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;

            beforeHash = ownerHash(&state);
            if (EspMapDialogOwner_apply(&state, &intent) !=
                    ESP_MAP_DIALOG_OWNER_APPLY_OK ||
                !validateAppliedOwner(&intent, &state)) return 0;
            afterHash = ownerHash(&state);
            hash = hashU16(hash, intent.globalCommandIndex);
            hash = hashByte(hash, intent.codeId);
            hash = hashU16(hash, intent.text.index);
            hash = hashU16(hash, intent.text.sourceOffset);
            hash = hashU16(hash, intent.text.length);
            hash = hashU16(hash, intent.sourceEventIndex);
            hash = hashByte(hash, intent.sourceCommandOffset);
            hash = hashByte(hash, intent.resumeCommandOffset);
            hash = hashByte(hash, intent.flags);
            hash = hashU32(hash, beforeHash);
            hash = hashU32(hash, afterHash);

            ++audit->refs;
            ++audit->resumeExact;
            if (intent.flags & ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT)
                ++audit->pauseRefs;
            if (intent.flags & ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN)
                ++audit->skipTurnRefs;
            if (command.id == ESP_MAP_OPCODE_DIALOG) {
                ++audit->backRefs;
                if (!audit->haveDialog) {
                    audit->dialogSample = intent;
                    audit->haveDialog = 1U;
                }
            } else {
                ++audit->noBackRefs;
                if (!audit->haveNoBack) {
                    audit->noBackSample = intent;
                    audit->haveNoBack = 1U;
                }
            }
        }
    }

    if (audit->refs != EXPECTED_DIALOG_REFS ||
        audit->backRefs != EXPECTED_DIALOG_BACK_REFS ||
        audit->noBackRefs != EXPECTED_DIALOG_NO_BACK_REFS ||
        audit->pauseRefs != EXPECTED_DIALOG_REFS ||
        audit->skipTurnRefs != EXPECTED_DIALOG_REFS ||
        audit->resumeExact != EXPECTED_DIALOG_REFS ||
        audit->stateExecutorRefused != EXPECTED_DIALOG_REFS ||
        !audit->haveDialog || !audit->haveNoBack || !audit->haveUnsupported)
        return 0;

    if (audit->dialogSample.globalCommandIndex != EXPECTED_DIALOG_CMD ||
        audit->dialogSample.sourceEventIndex != EXPECTED_DIALOG_EVENT ||
        audit->dialogSample.sourceCommandOffset != EXPECTED_DIALOG_OFF ||
        audit->dialogSample.resumeCommandOffset != EXPECTED_DIALOG_RESUME ||
        audit->dialogSample.text.index != EXPECTED_DIALOG_STRING ||
        audit->dialogSample.text.sourceOffset != EXPECTED_DIALOG_STRING_OFFSET ||
        audit->dialogSample.text.length != EXPECTED_DIALOG_STRING_LENGTH ||
        audit->dialogSample.flags !=
            (ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK |
             ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
             ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN)) return 0;
    if (audit->noBackSample.globalCommandIndex != EXPECTED_NOBACK_CMD ||
        audit->noBackSample.sourceEventIndex != EXPECTED_NOBACK_EVENT ||
        audit->noBackSample.sourceCommandOffset != EXPECTED_NOBACK_OFF ||
        audit->noBackSample.resumeCommandOffset != EXPECTED_NOBACK_RESUME ||
        audit->noBackSample.text.index != EXPECTED_NOBACK_STRING ||
        audit->noBackSample.text.sourceOffset != EXPECTED_NOBACK_STRING_OFFSET ||
        audit->noBackSample.text.length != EXPECTED_NOBACK_STRING_LENGTH ||
        audit->noBackSample.flags !=
            (ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
             ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN)) return 0;

    EspMapDialogOwner_reset(&state);
    if (!ownerIsClear(&state) ||
        EspMapDialogOwner_apply(&state, &audit->dialogSample) !=
            ESP_MAP_DIALOG_OWNER_APPLY_OK ||
        !validateAppliedOwner(&audit->dialogSample, &state)) return 0;
    snapshot = state;

    if (EspMapDialogOwner_apply(&state, &audit->unsupportedIntent) !=
            ESP_MAP_DIALOG_OWNER_APPLY_UNSUPPORTED ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->unsupportedRefused = 1U;
    intent = audit->dialogSample; intent.flags = 0U;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badFlagsRefused = 1U;
    intent = audit->dialogSample; intent.kind = ESP_MAP_UI_INTENT_FORCE_MESSAGE;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badKindRefused = 1U;
    intent = audit->dialogSample; ++intent.text.sourceOffset;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badRefRefused = 1U;
    intent = audit->dialogSample; intent.sourceEventIndex = EXPECTED_EVENT_COUNT;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badEventRefused = 1U;
    intent = audit->dialogSample; ++intent.globalCommandIndex;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badGlobalRefused = 1U;
    intent = audit->dialogSample; ++intent.resumeCommandOffset;
    if (EspMapDialogOwner_apply(&state, &intent) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->badResumeRefused = 1U;
    if (EspMapDialogOwner_apply(&state, NULL) != ESP_MAP_DIALOG_OWNER_APPLY_INVALID ||
        !sameOwner(&state, &snapshot)) return 0;
    audit->nullIntentRefused = 1U;

    EspMapDialogOwner_reset(&state);
    if (!ownerIsClear(&state)) return 0;
    audit->resetProof = 1U;
    audit->applyFNV = hash;
    return 1;
}

void Esp32Map1DialogOwnerProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1DialogOwnerProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    DialogOwnerAudit audit;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter, scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter, started, elapsed;
    char* statBarBefore;
    int skipAdvanceBefore, saveTileBefore, tileEventBefore;
    int tileEventIndexBefore, tileEventFlagsBefore;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1StatusMessageProbe_isDone()) return;
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPDIALOGPROBE] ARMED native FORCE_MESSAGE owner proven; DIALOG/NOBACK pause-owner execution starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO DIALOG pause owner ===\n");
    printf("[MAPDIALOGPROBE] CONTRACT consume only EV_DIALOG/EV_DIALOGNOBACK intents -> 12B caller-owned ref+resume state; no text copy, pack IO, DoomCanvas/Game continuation mutation, presentation, world/render mutation\n");

    if (!boundaryIsSafe(doomRpg) ||
        sizeof(EspMapDialogOwnerState) != EXPECTED_OWNER_BYTES) {
        printf("[MAPDIALOGPROBE] FAILED precondition heap8=%u largest8=%u packOpen=%d ownerBytes=%u\n",
               (unsigned int)heap8Free(), (unsigned int)largest8Block(),
               EspAssetPack_isOpen(),
               (unsigned int)sizeof(EspMapDialogOwnerState));
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    heapBefore = heap8Free(); largestBefore = largest8Block();
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

    if (!auditDialogOwners(&audit)) {
        printf("[MAPDIALOGPROBE] FAILED owner audit\n");
        return;
    }

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = heap8Free(); largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view(); mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState ? fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfter = scriptState ? fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                            (uint32_t)sizeof(doomRpg->player->NotebookString));

    if (EspAssetPack_isOpen() || !boundaryIsSafe(doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV || scriptAfter != scriptBefore ||
        scriptAfter != EXPECTED_SCRIPT_FNV || notebookAfter != notebookBefore ||
        doomRpg->hud->statBarMessage != statBarBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceBefore ||
        doomRpg->game->saveTileEvent != saveTileBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore) {
        printf("[MAPDIALOGPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x notebook=%08x->%08x packOpen=%d\n",
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
    printf("[MAPDIALOG] READY refs=%u back=%u noBack=%u pause=%u skipTurn=%u resumeExact=%u ownerBytes=%u textCopyBytes=0 stateExecRefused=%u dialogApplyFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs, (unsigned int)audit.backRefs,
           (unsigned int)audit.noBackRefs, (unsigned int)audit.pauseRefs,
           (unsigned int)audit.skipTurnRefs, (unsigned int)audit.resumeExact,
           (unsigned int)sizeof(EspMapDialogOwnerState),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.applyFNV, (unsigned int)elapsed);
    printf("[MAPDIALOG] SAMPLE back cmd=%u event=%u off=%u resume=%u flags=%02x string=%u@%u+%u noBack cmd=%u event=%u off=%u resume=%u flags=%02x string=%u@%u+%u\n",
           (unsigned int)audit.dialogSample.globalCommandIndex,
           (unsigned int)audit.dialogSample.sourceEventIndex,
           (unsigned int)audit.dialogSample.sourceCommandOffset,
           (unsigned int)audit.dialogSample.resumeCommandOffset,
           (unsigned int)audit.dialogSample.flags,
           (unsigned int)audit.dialogSample.text.index,
           (unsigned int)audit.dialogSample.text.sourceOffset,
           (unsigned int)audit.dialogSample.text.length,
           (unsigned int)audit.noBackSample.globalCommandIndex,
           (unsigned int)audit.noBackSample.sourceEventIndex,
           (unsigned int)audit.noBackSample.sourceCommandOffset,
           (unsigned int)audit.noBackSample.resumeCommandOffset,
           (unsigned int)audit.noBackSample.flags,
           (unsigned int)audit.noBackSample.text.index,
           (unsigned int)audit.noBackSample.text.sourceOffset,
           (unsigned int)audit.noBackSample.text.length);
    printf("[MAPDIALOG] FAILCLOSED unsupported=%u badFlags=%u badKind=%u badRef=%u badEvent=%u badGlobal=%u badResume=%u nullIntent=%u ownerAtomic=yes reset=%u\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badFlagsRefused,
           (unsigned int)audit.badKindRefused,
           (unsigned int)audit.badRefRefused,
           (unsigned int)audit.badEventRefused,
           (unsigned int)audit.badGlobalRefused,
           (unsigned int)audit.badResumeRefused,
           (unsigned int)audit.nullIntentRefused,
           (unsigned int)audit.resetProof);
    printf("[MAPDIALOGPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x notebookFNV=%08x->%08x packIO=no persistentHeapBytes=0\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter,
           (unsigned int)notebookBefore, (unsigned int)notebookAfter);
    printf("[MAPDIALOGPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes ownerValueBytes=%u textCopyBytes=0 legacyDialogMutation=no legacyGameContinuationMutation=no worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspMapDialogOwnerState),
           doomRpg->game->numEntities, doomRpg->game->numMonsters);
}

int Esp32Map1DialogOwnerProbe_isDone(void) {
    return probeState.done;
}
