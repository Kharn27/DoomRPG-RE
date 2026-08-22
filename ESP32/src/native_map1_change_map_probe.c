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
#include "esp_map_change_map_state.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_strings.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_change_map_probe.h"
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
#define EXPECTED_OWNER_BYTES 16U
#define EXPECTED_RESULT_BYTES 20U
#define MAP_NAME_SCRATCH_BYTES 64U

typedef struct Esp32Map1ChangeMapProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1ChangeMapProbeState;

typedef struct ChangeMapAudit_s {
    uint32_t refs;
    uint32_t pendingRefs;
    uint32_t zeroParamRefs;
    uint32_t showStatsRefs;
    uint32_t directLoadRefs;
    uint32_t removableRefs;
    uint32_t fallbackMapRefs;
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
    EspMapChangeMapState sampleState;
    EspMapChangeMapResult sampleResult;
    EspMapByteCode sampleCommand;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
    int sampleTargetMapId;
    char sampleName[MAP_NAME_SCRATCH_BYTES];
} ChangeMapAudit;

static Esp32Map1ChangeMapProbeState probeState;
static EspMapChangeMapState parkedChangeMapState;

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

static uint32_t legacyTransitionHash(const DoomRPG_t* doomRpg) {
    uint32_t hash = 2166136261U;

    if (doomRpg == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL) return 0U;

    hash = hashU32(hash, (uint32_t)doomRpg->game->changeMapParam);
    hash = hashU32(hash, (uint32_t)doomRpg->game->spawnParam);
    hash = hashU32(hash, (uint32_t)(uint16_t)doomRpg->menu->mapNameId);
    hash = hashU32(hash, (uint32_t)doomRpg->menuSystem->menu);
    hash = hashU32(hash, (uint32_t)doomRpg->doomCanvas->state);
    hash = hashU32(hash, (uint32_t)doomRpg->doomCanvas->loadMapID);
    hash = hashU32(hash, (uint32_t)doomRpg->doomCanvas->loadType);
    return hashU32(hash, (uint32_t)doomRpg->doomCanvas->saveType);
}

static uint32_t playerStatsWitnessHash(const Player_t* player) {
    uint32_t hash = 2166136261U;

    if (player == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)player->time);
    hash = hashU32(hash, (uint32_t)player->totalTime);
    hash = hashU32(hash, (uint32_t)player->moves);
    hash = hashU32(hash, (uint32_t)player->totalMoves);
    hash = hashU32(hash, (uint32_t)player->completedLevels);
    hash = hashU32(hash, (uint32_t)player->killedMonstersLevels);
    hash = hashU32(hash, (uint32_t)player->foundSecretsLevels);
    return hashU32(hash, (uint32_t)player->xpGained);
}

static uint32_t stateHash(const EspMapChangeMapState* state) {
    uint32_t hash = 2166136261U;

    if (state == NULL) return 0U;
    hash = hashU32(hash, state->rawParam);
    hash = hashU16(hash, state->mapName.index);
    hash = hashU16(hash, state->mapName.sourceOffset);
    hash = hashU16(hash, state->mapName.length);
    hash = hashU16(hash, state->sourceEventIndex);
    hash = hashU16(hash, state->globalCommandIndex);
    hash = hashByte(hash, state->sourceCommandOffset);
    return hashByte(hash, state->active);
}

static uint32_t resultHash(const EspMapChangeMapResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU32(hash, result->rawParam);
    hash = hashU32(hash, result->spawnParam);
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->mapStringIndex);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->showStats);
    hash = hashByte(hash, result->pending);
    hash = hashByte(hash, result->legacyReturnValue);
    hash = hashByte(hash, result->removeCommandIfHandled);
    return hashByte(hash, result->effectFlags);
}

static int sameState(const EspMapChangeMapState* a,
                     const EspMapChangeMapState* b) {
    return a != NULL && b != NULL &&
           a->rawParam == b->rawParam &&
           a->mapName.index == b->mapName.index &&
           a->mapName.sourceOffset == b->mapName.sourceOffset &&
           a->mapName.length == b->mapName.length &&
           a->sourceEventIndex == b->sourceEventIndex &&
           a->globalCommandIndex == b->globalCommandIndex &&
           a->sourceCommandOffset == b->sourceCommandOffset &&
           a->active == b->active;
}

static int resultIsZero(const EspMapChangeMapResult* result) {
    EspMapChangeMapResult zero;

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
        doomRpg->menu == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->hud == NULL || doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32Map1SaveRouteProbe_isDone() && runtime != NULL &&
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

static int mapIdForName(const char* name) {
    static const char* const mapFiles[] = {
        "/intro.bsp",
        "/level01.bsp",
        "/level02.bsp",
        "/level03.bsp",
        "/level04.bsp",
        "/level05.bsp",
        "/level06.bsp",
        "/level07.bsp",
        "/junction.bsp",
        "/junction_destroyed.bsp",
        "/items.bsp",
        "/reactor.bsp",
        "/endgame.bsp"
    };
    uint32_t i;

    if (name == NULL) return MAP_INTRO;
    for (i = 0U; i < (uint32_t)(sizeof(mapFiles) / sizeof(mapFiles[0])); ++i) {
        if (strcmp(name, mapFiles[i]) == 0) return (int)i + MAP_INTRO;
    }
    return MAP_INTRO;
}

static int nameIsKnownMap(const char* name) {
    static const char* const mapFiles[] = {
        "/intro.bsp",
        "/level01.bsp",
        "/level02.bsp",
        "/level03.bsp",
        "/level04.bsp",
        "/level05.bsp",
        "/level06.bsp",
        "/level07.bsp",
        "/junction.bsp",
        "/junction_destroyed.bsp",
        "/items.bsp",
        "/reactor.bsp",
        "/endgame.bsp"
    };
    uint32_t i;

    if (name == NULL) return 0;
    for (i = 0U; i < (uint32_t)(sizeof(mapFiles) / sizeof(mapFiles[0])); ++i) {
        if (strcmp(name, mapFiles[i]) == 0) return 1;
    }
    return 0;
}

static int validateChangeMap(const EspMapEventDescriptor* descriptor,
                             uint32_t commandOffset,
                             const EspMapByteCode* command,
                             const EspMapStringRef* expectedRef,
                             const EspMapChangeMapState* state,
                             const EspMapChangeMapResult* result) {
    const uint32_t rawParam = command != NULL ? command->arg1 : 0U;
    const uint32_t spawnParam = (rawParam << 1U) >> 9U;
    const uint8_t showStats =
        (uint8_t)((rawParam & ESP_MAP_CHANGE_MAP_SHOW_STATS_BIT) != 0U);
    const uint8_t pending = (uint8_t)(rawParam != 0U);
    const uint8_t expectedEffects =
        pending == 0U
            ? 0U
            : (uint8_t)(ESP_MAP_CHANGE_MAP_EFFECT_ADD_LEVEL_STATS |
                        (showStats != 0U
                             ? ESP_MAP_CHANGE_MAP_EFFECT_SHOW_STATS_MENU
                             : ESP_MAP_CHANGE_MAP_EFFECT_LOAD_MAP));
    uint32_t globalCommandIndex;

    if (descriptor == NULL || command == NULL || state == NULL ||
        result == NULL || command->id != ESP_MAP_OPCODE_CHANGE_MAP) return 0;

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;

    if (pending != 0U) {
        if (expectedRef == NULL || state->active != 1U ||
            state->rawParam != rawParam ||
            state->mapName.index != expectedRef->index ||
            state->mapName.sourceOffset != expectedRef->sourceOffset ||
            state->mapName.length != expectedRef->length ||
            state->sourceEventIndex != descriptor->eventIndex ||
            state->globalCommandIndex != (uint16_t)globalCommandIndex ||
            state->sourceCommandOffset != (uint8_t)commandOffset) return 0;
    }
    else {
        EspMapChangeMapState zero;
        memset(&zero, 0, sizeof(zero));
        if (memcmp(state, &zero, sizeof(zero)) != 0) return 0;
    }

    return result->rawParam == rawParam &&
           result->spawnParam == spawnParam &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->mapStringIndex == (uint16_t)(rawParam & 0xffU) &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->showStats == showStats && result->pending == pending &&
           result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_CHANGE_MAP_COMMAND_FLAG_REMOVE) != 0U) &&
           result->effectFlags == expectedEffects;
}

static int auditChangeMaps(const EspAssetPackEntry* entry,
                           ChangeMapAudit* audit) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapStringRef expectedRef;
    EspMapChangeMapState state;
    EspMapChangeMapState beforeState;
    EspMapChangeMapState repeatedState;
    EspMapChangeMapResult result;
    EspMapChangeMapResult repeatedResult;
    EspMapChangeMapStatus status;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t ownerAggregate = 2166136261U;
    uint32_t resultAggregate = 2166136261U;
    uint32_t contentAggregate = 2166136261U;
    size_t nameLength;
    uint32_t i;
    char mapName[MAP_NAME_SCRATCH_BYTES];

    if (entry == NULL || audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));
    EspMapChangeMap_reset(&state);
    audit->initialOwnerFNV = stateHash(&state);

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            const uint8_t pending =
                (uint8_t)(EspMapEvents_getCommand(&descriptor, commandOffset,
                                                  &command) &&
                          command.arg1 != 0U);
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) {
                return 0;
            }
            if (!audit->haveUnsupported &&
                command.id != ESP_MAP_OPCODE_CHANGE_MAP) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_CHANGE_MAP) continue;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;
            ++audit->refs;

            memset(&expectedRef, 0, sizeof(expectedRef));
            memset(mapName, 0, sizeof(mapName));
            nameLength = 0U;
            if (pending != 0U) {
                if (!EspMapStrings_getRef(command.arg1 & 0xffU, &expectedRef) ||
                    expectedRef.length + 1U > sizeof(mapName) ||
                    EspMapStrings_read(entry, &expectedRef, mapName,
                                       sizeof(mapName), &nameLength) !=
                        ESP_MAP_STRING_READ_OK ||
                    nameLength != expectedRef.length ||
                    mapName[nameLength] != '\0') return 0;

                ++audit->pendingRefs;
                audit->mapNameBytes += (uint32_t)nameLength;
                if (nameLength > audit->maxMapNameLength) {
                    audit->maxMapNameLength = (uint32_t)nameLength;
                }
                if (!nameIsKnownMap(mapName)) ++audit->fallbackMapRefs;
                if ((command.arg1 & ESP_MAP_CHANGE_MAP_SHOW_STATS_BIT) != 0U) {
                    ++audit->showStatsRefs;
                }
                else {
                    ++audit->directLoadRefs;
                }
            }
            else {
                ++audit->zeroParamRefs;
            }

            EspMapChangeMap_reset(&state);
            memset(&result, 0, sizeof(result));
            status = EspMapChangeMap_apply(&state, &descriptor,
                                           commandOffset, &result);
            if (status != ESP_MAP_CHANGE_MAP_OK ||
                !validateChangeMap(&descriptor, commandOffset, &command,
                                   pending != 0U ? &expectedRef : NULL,
                                   &state, &result) ||
                sizeof(EspMapChangeMapState) != EXPECTED_OWNER_BYTES ||
                sizeof(EspMapChangeMapResult) != EXPECTED_RESULT_BYTES) return 0;

            if (result.removeCommandIfHandled != 0U) ++audit->removableRefs;
            ownerAggregate = hashU32(ownerAggregate, stateHash(&state));
            resultAggregate = hashU32(resultAggregate, resultHash(&result));
            if (pending != 0U) {
                contentAggregate = hashU16(contentAggregate, expectedRef.index);
                contentAggregate = hashU16(contentAggregate, expectedRef.length);
                contentAggregate = hashU32(contentAggregate,
                                           (uint32_t)mapIdForName(mapName));
                for (i = 0U; i < (uint32_t)nameLength; ++i) {
                    contentAggregate = hashByte(contentAggregate,
                                                (uint8_t)mapName[i]);
                }
            }

            if (!audit->haveSample && pending != 0U) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleState = state;
                audit->sampleResult = result;
                audit->sampleOffset = (uint8_t)commandOffset;
                audit->sampleOwnerFNV = stateHash(&state);
                audit->sampleTargetMapId = mapIdForName(mapName);
                strncpy(audit->sampleName, mapName,
                        sizeof(audit->sampleName) - 1U);
                audit->sampleName[sizeof(audit->sampleName) - 1U] = '\0';
                audit->haveSample = 1U;
            }

            EspMapChangeMap_reset(&state);
            if (EspMapChangeMap_isActive(&state) ||
                stateHash(&state) != audit->initialOwnerFNV) return 0;
            ++audit->rollbackProofs;
        }
    }

    if (audit->refs == 0U || audit->pendingRefs == 0U ||
        audit->stateExecutorRefused != audit->refs ||
        audit->rollbackProofs != audit->refs || !audit->haveSample ||
        !audit->haveUnsupported ||
        audit->pendingRefs + audit->zeroParamRefs != audit->refs ||
        audit->showStatsRefs + audit->directLoadRefs != audit->pendingRefs) {
        return 0;
    }
    audit->ownerFNV = ownerAggregate;
    audit->resultFNV = resultAggregate;
    audit->contentFNV = contentAggregate;

    EspMapChangeMap_reset(&state);
    if (EspMapChangeMap_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_CHANGE_MAP_OK) return 0;
    repeatedState = state;
    repeatedResult = result;
    if (EspMapChangeMap_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_CHANGE_MAP_OK ||
        !sameState(&state, &repeatedState) ||
        memcmp(&result, &repeatedResult, sizeof(result)) != 0) return 0;
    audit->reapplyExact = 1U;
    EspMapChangeMap_reset(&state);

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapChangeMap_apply(&state, &audit->unsupportedDescriptor,
                              audit->unsupportedOffset, &result) !=
            ESP_MAP_CHANGE_MAP_UNSUPPORTED ||
        !sameState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->unsupportedRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapChangeMap_apply(&state, &audit->sampleDescriptor,
                              audit->sampleDescriptor.commandCount, &result) !=
            ESP_MAP_CHANGE_MAP_INVALID ||
        !sameState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapChangeMap_apply(&state, &badDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_CHANGE_MAP_INVALID ||
        !sameState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->badDescriptorRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapChangeMap_apply(&state, NULL, audit->sampleOffset, &result) !=
            ESP_MAP_CHANGE_MAP_INVALID ||
        !sameState(&state, &beforeState) || !resultIsZero(&result)) return 0;
    audit->nullDescriptorRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapChangeMap_apply(NULL, &audit->sampleDescriptor,
                              audit->sampleOffset, &result) !=
            ESP_MAP_CHANGE_MAP_INVALID || !resultIsZero(&result)) return 0;
    audit->nullStateRefused = 1U;

    state = audit->sampleState;
    beforeState = state;
    if (EspMapChangeMap_apply(&state, &audit->sampleDescriptor,
                              audit->sampleOffset, NULL) !=
            ESP_MAP_CHANGE_MAP_INVALID ||
        !sameState(&state, &beforeState)) return 0;
    audit->nullResultRefused = 1U;

    state = audit->sampleState;
    EspMapChangeMap_reset(&state);
    if (EspMapChangeMap_isActive(&state) ||
        stateHash(&state) != audit->initialOwnerFNV) return 0;
    audit->resetProof = 1U;
    return 1;
}

void Esp32Map1ChangeMapProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapChangeMap_reset(&parkedChangeMapState);
}

int Esp32Map1ChangeMapProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1ChangeMapProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapAutomapStateView* automapState;
    EspAssetPackEntry entry;
    ChangeMapAudit audit;
    EspMapChangeMapResult closedPackResult;
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
    uint32_t saveRouteBefore;
    uint32_t saveRouteAfter;
    uint32_t transitionBefore;
    uint32_t transitionAfter;
    uint32_t statsBefore;
    uint32_t statsAfter;
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1SaveRouteProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPCHANGEMAPPROBE] ARMED native SAVEGAME route proven; EV_CHANGEMAP pending-transition ownership starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO CHANGEMAP pending transition ===\n");
    printf("[MAPCHANGEMAPPROBE] CONTRACT consume only EV_CHANGEMAP -> 16B caller-owned pending raw-param/string-ref state + 20B result; decode spawn/show-stats/deferred effects, no PAK IO in executor, no stats/menu/load/sound/legacy Game/world/render/entity mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPCHANGEMAPPROBE] FAILED unsafe precondition\n");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        automapState == NULL) {
        printf("[MAPCHANGEMAPPROBE] FAILED native prerequisites unavailable\n");
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
    saveRouteBefore = legacySaveRouteHash(doomRpg->game);
    transitionBefore = legacyTransitionHash(doomRpg);
    statsBefore = playerStatsWitnessHash(doomRpg->player);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPCHANGEMAPPROBE] FAILED open %s\n",
               ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry("/intro.bsp", &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        !auditChangeMaps(&entry, &audit)) {
        EspAssetPack_close();
        printf("[MAPCHANGEMAPPROBE] FAILED CHANGEMAP audit\n");
        return;
    }

    EspAssetPack_close();
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    EspMapChangeMap_reset(&parkedChangeMapState);
    memset(&closedPackResult, 0, sizeof(closedPackResult));
    if (EspAssetPack_isOpen() ||
        EspMapChangeMap_apply(&parkedChangeMapState,
                              &audit.sampleDescriptor, audit.sampleOffset,
                              &closedPackResult) != ESP_MAP_CHANGE_MAP_OK ||
        !sameState(&parkedChangeMapState, &audit.sampleState) ||
        memcmp(&closedPackResult, &audit.sampleResult,
               sizeof(closedPackResult)) != 0) {
        printf("[MAPCHANGEMAPPROBE] FAILED closed-pack executor proof\n");
        return;
    }
    audit.closedPackApply = 1U;
    EspMapChangeMap_reset(&parkedChangeMapState);

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
    saveRouteAfter = legacySaveRouteHash(doomRpg->game);
    transitionAfter = legacyTransitionHash(doomRpg);
    statsAfter = playerStatsWitnessHash(doomRpg->player);

    if (sizeof(EspMapChangeMapState) != EXPECTED_OWNER_BYTES ||
        sizeof(EspMapChangeMapResult) != EXPECTED_RESULT_BYTES ||
        audit.refs == 0U || audit.pendingRefs == 0U ||
        audit.stateExecutorRefused != audit.refs ||
        audit.rollbackProofs != audit.refs || !audit.reapplyExact ||
        !audit.closedPackApply || !audit.resetProof ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore || mapStateAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV ||
        automapAfter != automapBefore ||
        automapAfter != EXPECTED_AUTOMAP_STATE_FNV ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        EspMapLineTextureState_view() == NULL ||
        EspMapLineTextureState_view()->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        notebookAfter != notebookBefore ||
        notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV || keysAfter != keysBefore ||
        hudAfter != hudBefore || passwordCanvasAfter != passwordCanvasBefore ||
        continuationAfter != continuationBefore ||
        saveRouteAfter != saveRouteBefore || transitionAfter != transitionBefore ||
        statsAfter != statsBefore || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspMapChangeMap_isActive(&parkedChangeMapState)) {
        printf("[MAPCHANGEMAPPROBE] FAILED integrity regression\n");
        return;
    }

    printf("[MAPCHANGEMAP] READY refs=%u pending=%u zeroParam=%u showStats=%u directLoad=%u removable=%u fallbackMap=%u ownerBytes=%u resultBytes=%u stateExecRefused=%u mapNameBytes=%u maxMapName=%u ownerFNV=%08x resultFNV=%08x contentFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs,
           (unsigned int)audit.pendingRefs,
           (unsigned int)audit.zeroParamRefs,
           (unsigned int)audit.showStatsRefs,
           (unsigned int)audit.directLoadRefs,
           (unsigned int)audit.removableRefs,
           (unsigned int)audit.fallbackMapRefs,
           (unsigned int)sizeof(EspMapChangeMapState),
           (unsigned int)sizeof(EspMapChangeMapResult),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.mapNameBytes,
           (unsigned int)audit.maxMapNameLength,
           (unsigned int)audit.ownerFNV,
           (unsigned int)audit.resultFNV,
           (unsigned int)audit.contentFNV,
           (unsigned int)elapsedMs);
    printf("[MAPCHANGEMAP] SAMPLE cmd=%u event=%u off=%u arg1=%08x arg2=%08x mapString=%u name=\"%s\" targetMap=%d spawnParam=%u showStats=%u effects=%02x pending=%u handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.sampleResult.globalCommandIndex,
           (unsigned int)audit.sampleResult.sourceEventIndex,
           (unsigned int)audit.sampleResult.sourceCommandOffset,
           (unsigned int)audit.sampleCommand.arg1,
           (unsigned int)audit.sampleCommand.arg2,
           (unsigned int)audit.sampleResult.mapStringIndex,
           audit.sampleName,
           audit.sampleTargetMapId,
           (unsigned int)audit.sampleResult.spawnParam,
           (unsigned int)audit.sampleResult.showStats,
           (unsigned int)audit.sampleResult.effectFlags,
           (unsigned int)audit.sampleResult.pending,
           (unsigned int)audit.sampleResult.legacyReturnValue,
           (unsigned int)audit.sampleResult.removeCommandIfHandled);
    printf("[MAPCHANGEMAP] STATE initialOwnerFNV=%08x sampleOwnerFNV=%08x rollback=%u/%u reapplyExact=%u closedPackApply=%u activeAtPark=%u\n",
           (unsigned int)audit.initialOwnerFNV,
           (unsigned int)audit.sampleOwnerFNV,
           (unsigned int)audit.rollbackProofs,
           (unsigned int)audit.refs,
           (unsigned int)audit.reapplyExact,
           (unsigned int)audit.closedPackApply,
           (unsigned int)EspMapChangeMap_isActive(&parkedChangeMapState));
    printf("[MAPCHANGEMAP] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullState=%u nullResult=%u reset=%u stateAtomic=yes\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullStateRefused,
           (unsigned int)audit.nullResultRefused,
           (unsigned int)audit.resetProof);
    printf("[MAPCHANGEMAP] IO entry=/intro.bsp size=%u crc32=%08x heapOpen=%u transientHeapCost=%u largestOpen=%u packIO=yes verificationOnly=yes executorPackIO=no persistentHeapBytes=0\n",
           (unsigned int)entry.size,
           (unsigned int)entry.crc32,
           (unsigned int)heapOpen,
           (unsigned int)(heapBefore >= heapOpen ? heapBefore - heapOpen : 0U),
           (unsigned int)largestOpen);
    printf("[MAPCHANGEMAPPROBE] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x automapFNV=%08x->%08x\n",
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
    printf("[MAPCHANGEMAPPROBE] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x saveRouteFNV=%08x->%08x transitionFNV=%08x->%08x statsFNV=%08x->%08x legacyRuntimeClear=yes\n",
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
           (unsigned int)saveRouteBefore,
           (unsigned int)saveRouteAfter,
           (unsigned int)transitionBefore,
           (unsigned int)transitionAfter,
           (unsigned int)statsBefore,
           (unsigned int)statsAfter);
    printf("[MAPCHANGEMAPPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes nativeLineState=yes nativeDoorExec=yes nativeLineTextureState=yes nativeUnlockExec=yes nativeAutomapState=yes nativeGiveMapExec=yes nativeSaveRoute=yes nativeChangeMapIntent=yes ownerBytes=%u resultBytes=%u persistentBytes=0 transitionArmedProven=yes transitionTriggered=no statsMutation=no menuMutation=no mapLoad=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspMapChangeMapState),
           (unsigned int)sizeof(EspMapChangeMapResult),
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);

    probeState.done = 1;
}
