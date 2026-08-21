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

#include "esp_map_events.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_strings.h"
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
#define EXPECTED_STRING_PAYLOAD_BYTES 7779U
#define EXPECTED_MAX_STRING_BYTES 313U

typedef struct Esp32Map1UiIntentProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1UiIntentProbeState;

typedef struct StringAudit_s {
    uint32_t spanFNV;
    uint32_t count;
    uint32_t payloadBytes;
    uint32_t emptyCount;
    uint32_t maxLength;
    uint16_t firstOffset;
    uint16_t lastOffset;
    uint16_t lastLength;
} StringAudit;

typedef struct UiAudit_s {
    uint32_t intentFNV;
    uint32_t refs;
    uint32_t dialogRefs;
    uint32_t forceRefs;
    uint32_t noBackRefs;
    uint32_t noteRefs;
    uint32_t pauseRefs;
    uint32_t clearRefs;
    uint32_t stateExecutorRefused;
    EspMapUiIntent dialogSample;
    EspMapUiIntent forceSample;
    EspMapUiIntent noBackSample;
    EspMapUiIntent noteSample;
    uint8_t haveDialog;
    uint8_t haveForce;
    uint8_t haveNoBack;
    uint8_t haveNote;
} UiAudit;

static Esp32Map1UiIntentProbeState probeState;

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

    if (data == NULL) {
        return 0U;
    }
    for (i = 0U; i < length; ++i) {
        hash = hashByte(hash, data[i]);
    }
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

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL &&
           canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) {
        return 0;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32IntroDispose_isDone() &&
           Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() &&
           Esp32Map1AccessProbe_isDone() &&
           Esp32Map1StateProbe_isDone() &&
           Esp32Map1EventsProbe_isDone() &&
           Esp32Map1EventDescriptorProbe_isDone() &&
           Esp32Map1EventFilterProbe_isDone() &&
           Esp32Map1OpcodeExecProbe_isDone() &&
           runtime != NULL && mapState != NULL && scriptState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           runtime->stringCount == EXPECTED_STRING_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           !Esp32IntroClock_isActive() && !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int descriptorByIndex(uint32_t index,
                             EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t value;

    if (outDescriptor == NULL || index > 0xffffU ||
        !EspMapRuntime_getEvent(index, &value)) {
        return 0;
    }

    ref.index = (uint16_t)index;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int validateStrings(StringAudit* outAudit) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapStringRef ref;
    EspMapStringRef previous;
    uint32_t hash = 2166136261U;
    uint32_t stringDataEnd;
    uint32_t i;

    if (outAudit == NULL || runtime == NULL ||
        runtime->stringCount != EXPECTED_STRING_COUNT ||
        runtime->sourceBytes < runtime->blockMapBytes + runtime->planeMapBytes) {
        return 0;
    }

    memset(outAudit, 0, sizeof(*outAudit));
    memset(&previous, 0, sizeof(previous));
    stringDataEnd = runtime->sourceBytes -
                    runtime->blockMapBytes - runtime->planeMapBytes;

    for (i = 0U; i < runtime->stringCount; ++i) {
        if (!EspMapStrings_getRef(i, &ref) || ref.index != i) {
            return 0;
        }
        if (i > 0U &&
            (uint32_t)ref.sourceOffset !=
                (uint32_t)previous.sourceOffset +
                (uint32_t)previous.length + 2U) {
            return 0;
        }

        if (i == 0U) {
            outAudit->firstOffset = ref.sourceOffset;
        }
        outAudit->lastOffset = ref.sourceOffset;
        outAudit->lastLength = ref.length;
        outAudit->payloadBytes += ref.length;
        if (ref.length == 0U) {
            ++outAudit->emptyCount;
        }
        if (ref.length > outAudit->maxLength) {
            outAudit->maxLength = ref.length;
        }

        hash = hashU16(hash, ref.index);
        hash = hashU16(hash, ref.sourceOffset);
        hash = hashU16(hash, ref.length);
        previous = ref;
        ++outAudit->count;
    }

    if ((uint32_t)outAudit->lastOffset + outAudit->lastLength != stringDataEnd ||
        outAudit->payloadBytes != EXPECTED_STRING_PAYLOAD_BYTES ||
        outAudit->maxLength != EXPECTED_MAX_STRING_BYTES ||
        EspMapStrings_getRef(runtime->stringCount, &ref)) {
        return 0;
    }

    outAudit->spanFNV = hash;
    return 1;
}

static uint8_t expectedIntentKind(uint8_t codeId) {
    if (codeId == ESP_MAP_OPCODE_DIALOG ||
        codeId == ESP_MAP_OPCODE_DIALOG_NO_BACK) {
        return ESP_MAP_UI_INTENT_DIALOG;
    }
    if (codeId == ESP_MAP_OPCODE_FORCE_MESSAGE) {
        return ESP_MAP_UI_INTENT_FORCE_MESSAGE;
    }
    if (codeId == ESP_MAP_OPCODE_NOTE) {
        return ESP_MAP_UI_INTENT_APPEND_NOTE;
    }
    return ESP_MAP_UI_INTENT_NONE;
}

static uint8_t expectedIntentFlags(uint8_t codeId, uint16_t textLength) {
    uint8_t flags = 0U;

    if (codeId == ESP_MAP_OPCODE_DIALOG ||
        codeId == ESP_MAP_OPCODE_DIALOG_NO_BACK) {
        flags = ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
                ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN;
        if (codeId == ESP_MAP_OPCODE_DIALOG) {
            flags |= ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK;
        }
    }
    else if (codeId == ESP_MAP_OPCODE_FORCE_MESSAGE && textLength == 0U) {
        flags = ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY;
    }
    else if (codeId == ESP_MAP_OPCODE_NOTE) {
        flags = ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR;
    }
    return flags;
}

static uint32_t hashIntent(uint32_t hash, const EspMapUiIntent* intent) {
    hash = hashByte(hash, intent->codeId);
    hash = hashByte(hash, intent->kind);
    hash = hashByte(hash, intent->flags);
    hash = hashByte(hash, intent->status);
    hash = hashU32(hash, intent->arg1);
    hash = hashU32(hash, intent->arg2);
    hash = hashU16(hash, intent->sourceEventIndex);
    hash = hashU16(hash, intent->globalCommandIndex);
    hash = hashByte(hash, intent->sourceCommandOffset);
    hash = hashByte(hash, intent->resumeCommandOffset);
    hash = hashU16(hash, intent->text.index);
    hash = hashU16(hash, intent->text.sourceOffset);
    return hashU16(hash, intent->text.length);
}

static int auditUiIntents(UiAudit* outAudit) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapUiIntent intent;
    EspMapUiIntent unsupportedIntent;
    EspMapOpcodeExecResult opcodeResult;
    uint32_t hash = 2166136261U;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t expectedGlobal;
    uint8_t expectedKind;
    uint8_t expectedFlags;

    if (outAudit == NULL) {
        return 0;
    }
    memset(outAudit, 0, sizeof(*outAudit));

    if (!descriptorByIndex(0U, &descriptor) || descriptor.commandCount == 0U ||
        EspMapUiIntent_build(&descriptor, 0U, &unsupportedIntent) !=
            ESP_MAP_UI_INTENT_UNSUPPORTED ||
        unsupportedIntent.status != ESP_MAP_UI_INTENT_UNSUPPORTED ||
        EspMapUiIntent_build(NULL, 0U, &unsupportedIntent) !=
            ESP_MAP_UI_INTENT_INVALID ||
        EspMapUiIntent_build(&descriptor, descriptor.commandCount,
                             &unsupportedIntent) != ESP_MAP_UI_INTENT_INVALID ||
        EspMapUiIntent_build(&descriptor, 0U, NULL) !=
            ESP_MAP_UI_INTENT_INVALID) {
        return 0;
    }

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) {
            return 0;
        }

        for (commandOffset = 0U;
             commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }
            if (!EspMapUiIntent_supports(command.id)) {
                continue;
            }

            if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
                    ESP_MAP_UI_INTENT_OK ||
                intent.status != ESP_MAP_UI_INTENT_OK) {
                return 0;
            }

            expectedGlobal = (uint32_t)descriptor.firstCommandIndex + commandOffset;
            expectedKind = expectedIntentKind(command.id);
            expectedFlags = expectedIntentFlags(command.id, intent.text.length);
            if (intent.codeId != command.id || intent.arg1 != command.arg1 ||
                intent.arg2 != command.arg2 ||
                intent.sourceEventIndex != descriptor.eventIndex ||
                intent.globalCommandIndex != expectedGlobal ||
                intent.sourceCommandOffset != commandOffset ||
                intent.resumeCommandOffset != commandOffset + 1U ||
                intent.text.index != command.arg1 ||
                intent.kind != expectedKind || intent.flags != expectedFlags) {
                return 0;
            }

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) {
                return 0;
            }
            ++outAudit->stateExecutorRefused;

            ++outAudit->refs;
            if ((intent.flags & ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT) != 0U) {
                ++outAudit->pauseRefs;
            }
            if ((intent.flags & ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY) != 0U) {
                ++outAudit->clearRefs;
            }

            if (command.id == ESP_MAP_OPCODE_DIALOG) {
                ++outAudit->dialogRefs;
                if (!outAudit->haveDialog) {
                    outAudit->dialogSample = intent;
                    outAudit->haveDialog = 1U;
                }
            }
            else if (command.id == ESP_MAP_OPCODE_FORCE_MESSAGE) {
                ++outAudit->forceRefs;
                if (!outAudit->haveForce) {
                    outAudit->forceSample = intent;
                    outAudit->haveForce = 1U;
                }
            }
            else if (command.id == ESP_MAP_OPCODE_DIALOG_NO_BACK) {
                ++outAudit->noBackRefs;
                if (!outAudit->haveNoBack) {
                    outAudit->noBackSample = intent;
                    outAudit->haveNoBack = 1U;
                }
            }
            else if (command.id == ESP_MAP_OPCODE_NOTE) {
                ++outAudit->noteRefs;
                if (!outAudit->haveNote) {
                    outAudit->noteSample = intent;
                    outAudit->haveNote = 1U;
                }
            }

            hash = hashU16(hash, (uint16_t)expectedGlobal);
            hash = hashIntent(hash, &intent);
        }
    }

    if (outAudit->refs == 0U ||
        outAudit->refs != outAudit->dialogRefs + outAudit->forceRefs +
                         outAudit->noBackRefs + outAudit->noteRefs ||
        outAudit->stateExecutorRefused != outAudit->refs ||
        !outAudit->haveDialog || !outAudit->haveForce ||
        !outAudit->haveNoBack || !outAudit->haveNote ||
        outAudit->pauseRefs != outAudit->dialogRefs + outAudit->noBackRefs) {
        return 0;
    }

    outAudit->intentFNV = hash;
    return 1;
}

static void printIntentSample(const char* label,
                              const EspMapUiIntent* intent) {
    printf("[MAPUIPROBE] SAMPLE %s cmd=%u event=%u off=%u resume=%u id=%u kind=%u flags=%02x string=%u@%u+%u\n",
           label,
           (unsigned int)intent->globalCommandIndex,
           (unsigned int)intent->sourceEventIndex,
           (unsigned int)intent->sourceCommandOffset,
           (unsigned int)intent->resumeCommandOffset,
           (unsigned int)intent->codeId,
           (unsigned int)intent->kind,
           (unsigned int)intent->flags,
           (unsigned int)intent->text.index,
           (unsigned int)intent->text.sourceOffset,
           (unsigned int)intent->text.length);
}

void Esp32Map1UiIntentProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1UiIntentProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    StringAudit stringAudit;
    UiAudit uiAudit;
    uint32_t heapBefore;
    uint32_t heapAfter;
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
    uint32_t notebookBefore;
    uint32_t notebookAfter;
    uint32_t started;
    uint32_t elapsed;
    char* statBarBefore;
    int skipAdvanceBefore;
    int saveTileBefore;
    int tileEventBefore;
    int tileEventIndexBefore;
    int tileEventFlagsBefore;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }
    if (!Esp32Map1OpcodeExecProbe_isDone()) {
        return;
    }
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPUIPROBE] ARMED first native opcode execution proven; allocation-free UI/string intent translation starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO UI/string intents ===\n");
    printf("[MAPUIPROBE] CONTRACT resolve compact string spans + translate EV_DIALOG/FORCEMESSAGE/DIALOGNOBACK/NOTE to native intents; 0 persistent bytes; no DoomCanvas/Hud/Player/world mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPUIPROBE] FAILED precondition heap8=%u largest8=%u\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block());
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

    if (scriptBefore != EXPECTED_SCRIPT_FNV ||
        !validateStrings(&stringAudit) || !auditUiIntents(&uiAudit)) {
        printf("[MAPUIPROBE] FAILED validation scriptFNV=%08x\n",
               (unsigned int)scriptBefore);
        return;
    }

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState != NULL ?
                    fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfter = scriptState != NULL ?
                  fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                            (uint32_t)sizeof(doomRpg->player->NotebookString));

    if (!boundaryIsSafe(doomRpg) || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore ||
        arenaAfter != arenaBefore || arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore || mapStateAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV ||
        notebookAfter != notebookBefore ||
        doomRpg->hud->statBarMessage != statBarBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceBefore ||
        doomRpg->game->saveTileEvent != saveTileBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore) {
        printf("[MAPUIPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x notebook=%08x->%08x\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
               (unsigned int)scriptBefore, (unsigned int)scriptAfter,
               (unsigned int)notebookBefore, (unsigned int)notebookAfter);
        return;
    }

    probeState.done = 1;
    printf("[MAPSTRING] READY strings=%u payload=%u empty=%u max=%u firstOffset=%u last=%u+%u spanFNV=%08x persistentBytes=0\n",
           (unsigned int)stringAudit.count,
           (unsigned int)stringAudit.payloadBytes,
           (unsigned int)stringAudit.emptyCount,
           (unsigned int)stringAudit.maxLength,
           (unsigned int)stringAudit.firstOffset,
           (unsigned int)stringAudit.lastOffset,
           (unsigned int)stringAudit.lastLength,
           (unsigned int)stringAudit.spanFNV);
    printf("[MAPUI] READY refs=%u dialog=%u force=%u noBack=%u note=%u pause=%u clear=%u stateExecRefused=%u intentFNV=%08x elapsed=%ums persistentBytes=0\n",
           (unsigned int)uiAudit.refs,
           (unsigned int)uiAudit.dialogRefs,
           (unsigned int)uiAudit.forceRefs,
           (unsigned int)uiAudit.noBackRefs,
           (unsigned int)uiAudit.noteRefs,
           (unsigned int)uiAudit.pauseRefs,
           (unsigned int)uiAudit.clearRefs,
           (unsigned int)uiAudit.stateExecutorRefused,
           (unsigned int)uiAudit.intentFNV,
           (unsigned int)elapsed);
    printIntentSample("dialog", &uiAudit.dialogSample);
    printIntentSample("force", &uiAudit.forceSample);
    printIntentSample("noBack", &uiAudit.noBackSample);
    printIntentSample("note", &uiAudit.noteSample);
    printf("[MAPUIPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x notebookFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter,
           (unsigned int)notebookBefore, (unsigned int)notebookAfter);
    printf("[MAPUIPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes persistentBytes=0 legacyUiMutation=no worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1UiIntentProbe_isDone(void) {
    return probeState.done;
}
