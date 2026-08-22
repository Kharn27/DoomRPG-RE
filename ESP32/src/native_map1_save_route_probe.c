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
#include "esp_map_automap_state.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_save_route.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_strings.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_givemap_probe.h"
#include "native_map1_save_route_probe.h"
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
#define EXPECTED_MAP_SPRITE_COUNT 344U
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_LINE_STATE_BYTES 120U
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_STATE_BYTES 60U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_STORAGE_BYTES 103U
#define EXPECTED_AUTOMAP_STATE_FNV 0x669b1aa7U
#define EXPECTED_OWNER_BYTES 20U
#define EXPECTED_RESULT_BYTES 16U
#define NAME_BUFFER_BYTES ESP_MAP_SAVE_ROUTE_LEGACY_NAME_CAPACITY

typedef struct Esp32Map1SaveRouteProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1SaveRouteProbeState;

typedef struct SaveRouteAudit_s {
    uint32_t refs;
    uint32_t removableRefs;
    uint32_t stateExecutorRefused;
    uint32_t rollbackProofs;
    uint32_t mapNameBytes;
    uint32_t maxMapNameLength;
    uint32_t ownerFNV;
    uint32_t resultFNV;
    uint32_t contentFNV;
    uint32_t initialOwnerFNV;
    uint32_t sampleOwnerFNV;
    uint32_t reapplyExact;
    uint32_t closedPackApply;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullStateRefused;
    uint32_t nullResultRefused;
    uint32_t resetProof;
    EspMapEventDescriptor sampleDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapSaveRouteState sampleState;
    EspMapSaveRouteResult sampleResult;
    EspMapByteCode sampleCommand;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
    char sampleName[NAME_BUFFER_BYTES];
} SaveRouteAudit;

static Esp32Map1SaveRouteProbeState probeState;
static EspMapSaveRouteState parkedRouteState;

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

static uint32_t legacySaveRouteHash(const Game_t* game) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (game == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)game->newDestX);
    hash = hashU32(hash, (uint32_t)game->newDestY);
    hash = hashU32(hash, (uint32_t)game->newAngle);
    for (i = 0U; i < (uint32_t)sizeof(game->newMapName); ++i) {
        hash = hashByte(hash, (uint8_t)game->newMapName[i]);
    }
    return hash;
}

static uint32_t routeStateHash(const EspMapSaveRouteState* state) {
    uint32_t hash = 2166136261U;

    if (state == NULL) return 0U;
    hash = hashU16(hash, state->mapName.index);
    hash = hashU16(hash, state->mapName.sourceOffset);
    hash = hashU16(hash, state->mapName.length);
    hash = hashU16(hash, state->destinationX);
    hash = hashU16(hash, state->destinationY);
    hash = hashU16(hash, state->sourceEventIndex);
    hash = hashU16(hash, state->globalCommandIndex);
    hash = hashByte(hash, state->sourceCommandOffset);
    hash = hashByte(hash, state->angle);
    hash = hashByte(hash, state->rawX);
    hash = hashByte(hash, state->rawY);
    return hashByte(hash, state->active);
}

static uint32_t routeResultHash(const EspMapSaveRouteResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->mapStringIndex);
    hash = hashU16(hash, result->destinationX);
    hash = hashU16(hash, result->destinationY);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->rawX);
    hash = hashByte(hash, result->rawY);
    hash = hashByte(hash, result->angle);
    hash = hashByte(hash, result->legacyReturnValue);
    return hashByte(hash, result->removeCommandIfHandled);
}

static int sameRouteState(const EspMapSaveRouteState* a,
                          const EspMapSaveRouteState* b) {
    return a != NULL && b != NULL &&
           a->mapName.index == b->mapName.index &&
           a->mapName.sourceOffset == b->mapName.sourceOffset &&
           a->mapName.length == b->mapName.length &&
           a->destinationX == b->destinationX &&
           a->destinationY == b->destinationY &&
           a->sourceEventIndex == b->sourceEventIndex &&
           a->globalCommandIndex == b->globalCommandIndex &&
           a->sourceCommandOffset == b->sourceCommandOffset &&
           a->angle == b->angle && a->rawX == b->rawX &&
           a->rawY == b->rawY && a->active == b->active;
}

static int resultIsZero(const EspMapSaveRouteResult* result) {
    EspMapSaveRouteResult zero;

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

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapAutomapStateView* automapState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32Map1GiveMapProbe_isDone() && runtime != NULL &&
           mapState != NULL && scriptState != NULL && lineState != NULL &&
           textureState != NULL && automapState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           runtime->sourceCrc32 == EXPECTED_INTRO_BSP_CRC32 &&
           runtime->lineCount == EXPECTED_LINE_COUNT &&
           runtime->mapSpriteCount == EXPECTED_MAP_SPRITE_COUNT &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           fnv1a32(scriptState->storage, scriptState->storageBytes) ==
               EXPECTED_SCRIPT_FNV &&
           lineState->storageBytes == EXPECTED_LINE_STATE_BYTES &&
           lineState->stateFNV1a == EXPECTED_LINE_STATE_FNV &&
           textureState->storageBytes == EXPECTED_TEXTURE_STATE_BYTES &&
           textureState->stateFNV1a == EXPECTED_TEXTURE_STATE_FNV &&
           automapState->storageBytes == EXPECTED_AUTOMAP_STORAGE_BYTES &&
           automapState->stateFNV1a == EXPECTED_AUTOMAP_STATE_FNV &&
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

static int validateRoute(const EspMapEventDescriptor* descriptor,
                         uint32_t commandOffset,
                         const EspMapByteCode* command,
                         const EspMapSaveRouteState* state,
                         const EspMapSaveRouteResult* result) {
    EspMapStringRef expectedRef;
    uint32_t packedDestination;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t angle;
    uint16_t destinationX;
    uint16_t destinationY;
    uint32_t globalCommandIndex;

    if (descriptor == NULL || command == NULL || state == NULL ||
        result == NULL || command->id != ESP_MAP_OPCODE_SAVEGAME ||
        !EspMapStrings_getRef(command->arg1 & 0xffU, &expectedRef)) return 0;

    packedDestination = command->arg1 >> 8;
    rawX = (uint8_t)(packedDestination & 0xffU);
    rawY = (uint8_t)((packedDestination >> 8) & 0xffU);
    angle = (uint8_t)((packedDestination >> 16) & 0xffU);
    destinationX = (uint16_t)(32U + ((uint32_t)rawX << 6));
    destinationY = (uint16_t)(32U + ((uint32_t)rawY << 6));
    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;

    return expectedRef.length < ESP_MAP_SAVE_ROUTE_LEGACY_NAME_CAPACITY &&
           state->mapName.index == expectedRef.index &&
           state->mapName.sourceOffset == expectedRef.sourceOffset &&
           state->mapName.length == expectedRef.length &&
           state->destinationX == destinationX &&
           state->destinationY == destinationY &&
           state->sourceEventIndex == descriptor->eventIndex &&
           state->globalCommandIndex == (uint16_t)globalCommandIndex &&
           state->sourceCommandOffset == (uint8_t)commandOffset &&
           state->angle == angle && state->rawX == rawX &&
           state->rawY == rawY && state->active == 1U &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->mapStringIndex == expectedRef.index &&
           result->destinationX == destinationX &&
           result->destinationY == destinationY &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->rawX == rawX && result->rawY == rawY &&
           result->angle == angle && result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_SAVE_ROUTE_COMMAND_FLAG_REMOVE) != 0U);
}

static int auditSaveRoutes(const EspAssetPackEntry* entry,
                           SaveRouteAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapSaveRouteState state;
    EspMapSaveRouteState beforeState;
    EspMapSaveRouteState repeatedState;
    EspMapSaveRouteResult result;
    EspMapSaveRouteResult repeatedResult;
    EspMapSaveRouteStatus status;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t ownerAggregate = 2166136261U;
    uint32_t resultAggregate = 2166136261U;
    uint32_t contentAggregate = 2166136261U;
    size_t mapNameLength;
    char mapName[NAME_BUFFER_BYTES];

    if (entry == NULL || audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    EspMapSaveRoute_reset(&state);
    audit->initialOwnerFNV = routeStateHash(&state);

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }
            if (!audit->haveUnsupported && command.id != ESP_MAP_OPCODE_SAVEGAME) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_SAVEGAME) continue;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;
            ++audit->refs;

            EspMapSaveRoute_reset(&state);
            status = EspMapSaveRoute_apply(&state, &descriptor, commandOffset,
                                           &result);
            if (status != ESP_MAP_SAVE_ROUTE_OK ||
                !validateRoute(&descriptor, commandOffset, &command, &state,
                               &result) ||
                sizeof(EspMapSaveRouteState) != EXPECTED_OWNER_BYTES ||
                sizeof(EspMapSaveRouteResult) != EXPECTED_RESULT_BYTES) return 0;

            if (result.removeCommandIfHandled != 0U) ++audit->removableRefs;
            ownerAggregate = hashU32(ownerAggregate, routeStateHash(&state));
            resultAggregate = hashU32(resultAggregate, routeResultHash(&result));

            memset(mapName, 0, sizeof(mapName));
            mapNameLength = 0U;
            if (EspMapStrings_read(entry, &state.mapName, mapName,
                                   sizeof(mapName), &mapNameLength) !=
                    ESP_MAP_STRING_READ_OK ||
                mapNameLength != state.mapName.length ||
                mapNameLength >= sizeof(mapName) ||
                mapName[mapNameLength] != '\0') return 0;
            audit->mapNameBytes += (uint32_t)mapNameLength;
            if (mapNameLength > audit->maxMapNameLength) {
                audit->maxMapNameLength = (uint32_t)mapNameLength;
            }
            contentAggregate = hashU16(contentAggregate, state.mapName.index);
            contentAggregate = hashU16(contentAggregate, state.mapName.length);
            for (size_t i = 0U; i < mapNameLength; ++i) {
                contentAggregate = hashByte(contentAggregate,
                                            (uint8_t)mapName[i]);
            }

            if (!audit->haveSample) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleState = state;
                audit->sampleResult = result;
                audit->sampleOffset = (uint8_t)commandOffset;
                memcpy(audit->sampleName, mapName, mapNameLength + 1U);
                audit->sampleOwnerFNV = routeStateHash(&state);
                audit->haveSample = 1U;
            }

            EspMapSaveRoute_reset(&state);
            if (EspMapSaveRoute_isActive(&state) ||
                routeStateHash(&state) != audit->initialOwnerFNV) return 0;
            ++audit->rollbackProofs;
        }
    }

    if (audit->refs == 0U || audit->stateExecutorRefused != audit->refs ||
        audit->rollbackProofs != audit->refs || !audit->haveSample ||
        !audit->haveUnsupported) return 0;
    audit->ownerFNV = ownerAggregate;
    audit->resultFNV = resultAggregate;
    audit->contentFNV = contentAggregate;

    EspMapSaveRoute_reset(&state);
    if (EspMapSaveRoute_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_SAVE_ROUTE_OK) return 0;
    repeatedState = state;
    repeatedResult = result;
    if (EspMapSaveRoute_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_SAVE_ROUTE_OK ||
        !sameRouteState(&state, &repeatedState) ||
        memcmp(&result, &repeatedResult, sizeof(result)) != 0) return 0;
    audit->reapplyExact = 1U;
    EspMapSaveRoute_reset(&state);

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapSaveRoute_apply(&state, &audit->unsupportedDescriptor,
                              audit->unsupportedOffset, &result) !=
            ESP_MAP_SAVE_ROUTE_UNSUPPORTED ||
        !sameRouteState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->unsupportedRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapSaveRoute_apply(&state, &audit->sampleDescriptor,
                              audit->sampleDescriptor.commandCount, &result) !=
            ESP_MAP_SAVE_ROUTE_INVALID ||
        !sameRouteState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapSaveRoute_apply(&state, &badDescriptor, audit->sampleOffset,
                              &result) != ESP_MAP_SAVE_ROUTE_INVALID ||
        !sameRouteState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->badDescriptorRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapSaveRoute_apply(&state, NULL, audit->sampleOffset, &result) !=
            ESP_MAP_SAVE_ROUTE_INVALID ||
        !sameRouteState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->nullDescriptorRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapSaveRoute_apply(NULL, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_SAVE_ROUTE_INVALID || !resultIsZero(&result)) return 0;
    audit->nullStateRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    if (EspMapSaveRoute_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, NULL) !=
            ESP_MAP_SAVE_ROUTE_INVALID ||
        !sameRouteState(&state, &beforeState)) return 0;
    audit->nullResultRefused = 1U;

    state = audit->sampleState;
    EspMapSaveRoute_reset(&state);
    if (EspMapSaveRoute_isActive(&state) ||
        routeStateHash(&state) != audit->initialOwnerFNV) return 0;
    audit->resetProof = 1U;
    return 1;
}

void Esp32Map1SaveRouteProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapSaveRoute_reset(&parkedRouteState);
}

int Esp32Map1SaveRouteProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1SaveRouteProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapAutomapStateView* automapState;
    EspAssetPackEntry entry;
    SaveRouteAudit audit;
    EspMapSaveRouteResult closedResult;
    uint32_t heapBefore;
    uint32_t heapOpen;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestOpen;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t arenaBefore;
    uint32_t arenaAfter;
    uint32_t mapStateBefore;
    uint32_t mapStateAfter;
    uint32_t scriptBefore;
    uint32_t scriptAfter;
    uint32_t automapBefore;
    uint32_t automapAfter;
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
    uint32_t legacyRouteBefore;
    uint32_t legacyRouteAfter;
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1GiveMapProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPSAVEROUTEPROBE] ARMED native GIVEMAP automap state proven; EV_SAVEGAME route ownership starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO SAVEGAME route owner ===\n");
    printf("[MAPSAVEROUTEPROBE] CONTRACT consume only EV_SAVEGAME -> caller-owned map-string/destination route state; verify names through native PAK but perform no save-file write, no Game/newMapName mutation, no map transition or world/render/entity mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPSAVEROUTEPROBE] FAILED unsafe precondition\n");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        automapState == NULL) {
        printf("[MAPSAVEROUTEPROBE] FAILED native prerequisites unavailable\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = runtime->arenaFNV1a;
    mapStateBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    automapBefore = automapState->stateFNV1a;
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                              (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = hudWitnessHash(doomRpg->hud);
    passwordCanvasBefore = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationBefore = gameContinuationHash(doomRpg->game);
    legacyRouteBefore = legacySaveRouteHash(doomRpg->game);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPSAVEROUTEPROBE] FAILED open %s\n",
               ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry("/intro.bsp", &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        !auditSaveRoutes(&entry, &audit)) {
        EspAssetPack_close();
        printf("[MAPSAVEROUTEPROBE] FAILED SAVEGAME route audit\n");
        return;
    }

    EspAssetPack_close();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    EspMapSaveRoute_reset(&parkedRouteState);
    memset(&closedResult, 0, sizeof(closedResult));
    if (EspAssetPack_isOpen() ||
        EspMapSaveRoute_apply(&parkedRouteState, &audit.sampleDescriptor,
                              audit.sampleOffset, &closedResult) !=
            ESP_MAP_SAVE_ROUTE_OK ||
        !sameRouteState(&parkedRouteState, &audit.sampleState) ||
        routeResultHash(&closedResult) != routeResultHash(&audit.sampleResult)) {
        printf("[MAPSAVEROUTEPROBE] FAILED closed-pack apply proof\n");
        return;
    }
    audit.closedPackApply = 1U;
    EspMapSaveRoute_reset(&parkedRouteState);

    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    arenaAfter = runtime != NULL ? runtime->arenaFNV1a : 0U;
    mapStateAfter = mapState != NULL ? mapState->stateFNV1a : 0U;
    scriptAfter = scriptState != NULL
                      ? fnv1a32(scriptState->storage, scriptState->storageBytes)
                      : 0U;
    automapAfter = automapState != NULL ? automapState->stateFNV1a : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = hudWitnessHash(doomRpg->hud);
    passwordCanvasAfter = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationAfter = gameContinuationHash(doomRpg->game);
    legacyRouteAfter = legacySaveRouteHash(doomRpg->game);

    if (sizeof(EspMapSaveRouteState) != EXPECTED_OWNER_BYTES ||
        sizeof(EspMapSaveRouteResult) != EXPECTED_RESULT_BYTES ||
        audit.refs == 0U || audit.stateExecutorRefused != audit.refs ||
        audit.rollbackProofs != audit.refs || !audit.reapplyExact ||
        !audit.closedPackApply || !audit.resetProof ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV || scriptAfter != scriptBefore ||
        scriptAfter != EXPECTED_SCRIPT_FNV || automapAfter != automapBefore ||
        automapAfter != EXPECTED_AUTOMAP_STATE_FNV ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        EspMapLineTextureState_view() == NULL ||
        EspMapLineTextureState_view()->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        notebookAfter != notebookBefore ||
        notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV || keysAfter != keysBefore ||
        hudAfter != hudBefore || passwordCanvasAfter != passwordCanvasBefore ||
        continuationAfter != continuationBefore ||
        legacyRouteAfter != legacyRouteBefore || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPSAVEROUTEPROBE] FAILED integrity regression\n");
        return;
    }

    printf("[MAPSAVEROUTE] READY refs=%u removable=%u ownerBytes=%u resultBytes=%u stateExecRefused=%u mapNameBytes=%u maxMapName=%u ownerFNV=%08x resultFNV=%08x contentFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs,
           (unsigned int)audit.removableRefs,
           (unsigned int)sizeof(EspMapSaveRouteState),
           (unsigned int)sizeof(EspMapSaveRouteResult),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.mapNameBytes,
           (unsigned int)audit.maxMapNameLength,
           (unsigned int)audit.ownerFNV,
           (unsigned int)audit.resultFNV,
           (unsigned int)audit.contentFNV,
           (unsigned int)elapsedMs);
    printf("[MAPSAVEROUTE] SAMPLE cmd=%u event=%u off=%u arg1=%08x arg2=%08x map=%u@%u+%u name=\"%s\" tile=%u,%u dest=%u,%u angle=%u handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.sampleResult.globalCommandIndex,
           (unsigned int)audit.sampleResult.sourceEventIndex,
           (unsigned int)audit.sampleResult.sourceCommandOffset,
           (unsigned int)audit.sampleCommand.arg1,
           (unsigned int)audit.sampleCommand.arg2,
           (unsigned int)audit.sampleState.mapName.index,
           (unsigned int)audit.sampleState.mapName.sourceOffset,
           (unsigned int)audit.sampleState.mapName.length,
           audit.sampleName,
           (unsigned int)audit.sampleState.rawX,
           (unsigned int)audit.sampleState.rawY,
           (unsigned int)audit.sampleState.destinationX,
           (unsigned int)audit.sampleState.destinationY,
           (unsigned int)audit.sampleState.angle,
           (unsigned int)audit.sampleResult.legacyReturnValue,
           (unsigned int)audit.sampleResult.removeCommandIfHandled);
    printf("[MAPSAVEROUTE] STATE initialOwnerFNV=%08x sampleOwnerFNV=%08x rollback=%u/%u reapplyExact=%u closedPackApply=%u activeAtPark=%u\n",
           (unsigned int)audit.initialOwnerFNV,
           (unsigned int)audit.sampleOwnerFNV,
           (unsigned int)audit.rollbackProofs,
           (unsigned int)audit.refs,
           (unsigned int)audit.reapplyExact,
           (unsigned int)audit.closedPackApply,
           (unsigned int)EspMapSaveRoute_isActive(&parkedRouteState));
    printf("[MAPSAVEROUTE] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullState=%u nullResult=%u reset=%u stateAtomic=yes\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullStateRefused,
           (unsigned int)audit.nullResultRefused,
           (unsigned int)audit.resetProof);
    printf("[MAPSAVEROUTE] IO entry=/intro.bsp size=%u crc32=%08x heapOpen=%u transientHeapCost=%u largestOpen=%u packIO=yes persistentHeapBytes=0 saveFileWrite=no\n",
           (unsigned int)entry.size,
           (unsigned int)entry.crc32,
           (unsigned int)heapOpen,
           (unsigned int)(heapBefore >= heapOpen ? heapBefore - heapOpen : 0U),
           (unsigned int)largestOpen);
    printf("[MAPSAVEROUTEPROBE] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x automapFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (int)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (int)((int32_t)largestAfter - (int32_t)largestBefore),
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)arenaBefore,
           (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore,
           (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore,
           (unsigned int)scriptAfter,
           (unsigned int)automapBefore,
           (unsigned int)automapAfter);
    printf("[MAPSAVEROUTEPROBE] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x saveRouteFNV=%08x->%08x legacyRuntimeClear=yes\n",
           (unsigned int)notebookBefore,
           (unsigned int)notebookAfter,
           (unsigned int)keysBefore,
           (unsigned int)keysAfter,
           (unsigned int)hudBefore,
           (unsigned int)hudAfter,
           (unsigned int)passwordCanvasBefore,
           (unsigned int)passwordCanvasAfter,
           (unsigned int)continuationBefore,
           (unsigned int)continuationAfter,
           (unsigned int)legacyRouteBefore,
           (unsigned int)legacyRouteAfter);
    printf("[MAPSAVEROUTEPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes nativeLineState=yes nativeDoorExec=yes nativeLineTextureState=yes nativeUnlockExec=yes nativeAutomapState=yes nativeGiveMapExec=yes nativeSaveRoute=yes ownerBytes=%u resultBytes=%u persistentBytes=0 legacySaveRouteMutation=no saveFileWrite=no worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspMapSaveRouteState),
           (unsigned int)sizeof(EspMapSaveRouteResult),
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);

    probeState.done = 1;
}
