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
#include "native_map1_givemap_probe.h"
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
#define EXPECTED_MAP_SPRITE_COUNT 344U
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_LINE_STATE_BYTES 120U
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_LINE_OPEN_COUNT 0U
#define EXPECTED_LINE_LOCKED_COUNT 7U
#define EXPECTED_TEXTURE_STATE_BYTES 60U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_TEXTURE_VARIANTS 6U
#define EXPECTED_TEXTURE10_COUNT 0U
#define EXPECTED_AUTOMAP_LINE_BYTES 60U
#define EXPECTED_AUTOMAP_SPRITE_BYTES 43U
#define EXPECTED_AUTOMAP_STORAGE_BYTES 103U
#define EXPECTED_RESULT_BYTES 20U
#define MAX_ALLOCATOR_OVERHEAD 64U
#define MIN_LARGEST8_AFTER_STATE 32768U

typedef struct Esp32Map1GiveMapProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1GiveMapProbeState;

typedef struct GiveMapAudit_s {
    uint32_t giveMapFNV;
    uint32_t refs;
    uint32_t mutatedRefs;
    uint32_t noMutationRefs;
    uint32_t removableRefs;
    uint32_t stateExecutorRefused;
    uint32_t rollbackProofs;
    uint32_t idempotentHandledProof;
    uint32_t lineMutatedTotal;
    uint32_t spriteMutatedTotal;
    uint32_t tileMutatedTotal;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullResultRefused;
    uint32_t badLineIndexRefused;
    uint32_t badSpriteIndexRefused;
    uint32_t badRevealValueRefused;
    uint32_t badVisitedIndexRefused;
    uint32_t badVisitedValueRefused;
    EspMapEventDescriptor sampleDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode sampleCommand;
    EspMapGiveMapResult sampleResult;
    uint32_t sampleMutatedAutomapFNV;
    uint32_t sampleMutatedMapStateFNV;
    uint8_t sampleOffset;
    uint8_t unsupportedOffset;
    uint8_t haveSample;
    uint8_t haveUnsupported;
} GiveMapAudit;

static Esp32Map1GiveMapProbeState probeState;

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

static uint32_t resultHash(const EspMapGiveMapResult* result) {
    uint32_t hash = 2166136261U;

    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->lineTargetCount);
    hash = hashU16(hash, result->spriteTargetCount);
    hash = hashU16(hash, result->entranceTargetCount);
    hash = hashU16(hash, result->linesMutated);
    hash = hashU16(hash, result->spritesMutated);
    hash = hashU16(hash, result->tilesMutated);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->mutated);
    hash = hashByte(hash, result->legacyReturnValue);
    return hashByte(hash, result->removeCommandIfHandled);
}

static int resultIsZero(const EspMapGiveMapResult* result) {
    EspMapGiveMapResult zero;

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

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32Map1UnlockProbe_isDone() && runtime != NULL &&
           mapState != NULL && scriptState != NULL && lineState != NULL &&
           textureState != NULL &&
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
           lineState->openCount == EXPECTED_LINE_OPEN_COUNT &&
           lineState->lockedCount == EXPECTED_LINE_LOCKED_COUNT &&
           textureState->storageBytes == EXPECTED_TEXTURE_STATE_BYTES &&
           textureState->stateFNV1a == EXPECTED_TEXTURE_STATE_FNV &&
           textureState->variantCount == EXPECTED_TEXTURE_VARIANTS &&
           textureState->texture10Count == EXPECTED_TEXTURE10_COUNT &&
           fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                   (uint32_t)sizeof(doomRpg->player->NotebookString)) ==
               EXPECTED_LEGACY_NOTEBOOK_FNV &&
           !EspMapAutomapState_isReady() && !EspAssetPack_isOpen() &&
           !Esp32IntroClock_isActive() && !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
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

static int currentGiveMapCounts(uint32_t* outLineTargets,
                                uint32_t* outSpriteTargets,
                                uint32_t* outEntranceTargets,
                                uint32_t* outLineMutations,
                                uint32_t* outSpriteMutations,
                                uint32_t* outTileMutations) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t i;
    uint32_t lineTargets = 0U;
    uint32_t entranceTargets = 0U;
    uint32_t lineMutations = 0U;
    uint32_t spriteMutations = 0U;
    uint32_t tileMutations = 0U;
    uint8_t revealed;
    uint8_t flags;

    if (runtime == NULL || outLineTargets == NULL || outSpriteTargets == NULL ||
        outEntranceTargets == NULL || outLineMutations == NULL ||
        outSpriteMutations == NULL || outTileMutations == NULL) return 0;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line) ||
            !EspMapAutomapState_getLineRevealed(i, &revealed)) return 0;
        if ((line.flags & ESP_MAP_LINE_FLAG_NO_AUTOMAP) == 0U) {
            ++lineTargets;
            if (revealed == 0U) ++lineMutations;
        }
    }
    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed)) return 0;
        if (revealed == 0U) ++spriteMutations;
    }
    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &flags)) return 0;
        if ((flags & ESP_MAP_TILE_ENTRANCE) != 0U) {
            ++entranceTargets;
            if ((flags & ESP_MAP_TILE_VISITED) == 0U) ++tileMutations;
        }
    }

    *outLineTargets = lineTargets;
    *outSpriteTargets = runtime->mapSpriteCount;
    *outEntranceTargets = entranceTargets;
    *outLineMutations = lineMutations;
    *outSpriteMutations = spriteMutations;
    *outTileMutations = tileMutations;
    return 1;
}

static int validateAutomapState(const EspMapAutomapStateView* state,
                                uint32_t* outFNV,
                                uint32_t* outLineRevealed,
                                uint32_t* outSpriteRevealed) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    EspMapSprite sprite;
    uint32_t hash = 2166136261U;
    uint32_t lineRevealedCount = 0U;
    uint32_t spriteRevealedCount = 0U;
    uint32_t i;
    uint8_t revealed;

    if (state == NULL || runtime == NULL || outFNV == NULL ||
        outLineRevealed == NULL || outSpriteRevealed == NULL ||
        state->lineCount != EXPECTED_LINE_COUNT ||
        state->spriteCount != EXPECTED_MAP_SPRITE_COUNT ||
        state->lineBitsetBytes != EXPECTED_AUTOMAP_LINE_BYTES ||
        state->spriteBitsetBytes != EXPECTED_AUTOMAP_SPRITE_BYTES ||
        state->storageBytes != EXPECTED_AUTOMAP_STORAGE_BYTES ||
        state->lineRevealedBits == NULL || state->spriteRevealedBits == NULL) {
        return 0;
    }

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line) ||
            !EspMapAutomapState_getLineRevealed(i, &revealed) ||
            revealed != (uint8_t)((line.flags &
                                    ESP_MAP_LINE_FLAG_AUTOMAP_REVEALED) != 0U)) {
            return 0;
        }
        lineRevealedCount += revealed;
    }
    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapAutomapState_getSpriteRevealed(i, &revealed) ||
            revealed != (uint8_t)((sprite.info &
                                    ESP_MAP_SPRITE_INFO_AUTOMAP_REVEALED) != 0U)) {
            return 0;
        }
        spriteRevealedCount += revealed;
    }
    for (i = 0U; i < state->lineBitsetBytes; ++i) {
        hash = hashByte(hash, state->lineRevealedBits[i]);
    }
    for (i = 0U; i < state->spriteBitsetBytes; ++i) {
        hash = hashByte(hash, state->spriteRevealedBits[i]);
    }

    if (state->stateFNV1a != hash ||
        state->lineRevealedCount != lineRevealedCount ||
        state->spriteRevealedCount != spriteRevealedCount) return 0;

    *outFNV = hash;
    *outLineRevealed = lineRevealedCount;
    *outSpriteRevealed = spriteRevealedCount;
    return 1;
}

static int restoreInitialAutomap(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    EspMapSprite sprite;
    uint32_t i;
    uint8_t flags;

    if (runtime == NULL) return 0;
    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line) ||
            !EspMapAutomapState_setLineRevealed(
                i, (uint8_t)((line.flags &
                              ESP_MAP_LINE_FLAG_AUTOMAP_REVEALED) != 0U))) {
            return 0;
        }
    }
    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapAutomapState_setSpriteRevealed(
                i, (uint8_t)((sprite.info &
                              ESP_MAP_SPRITE_INFO_AUTOMAP_REVEALED) != 0U))) {
            return 0;
        }
    }
    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &flags)) return 0;
        if ((flags & ESP_MAP_TILE_VISITED) != 0U &&
            !EspMapState_setVisited(i, 0U)) return 0;
    }
    return 1;
}

static int validateGiveMapResult(const EspMapEventDescriptor* descriptor,
                                 uint32_t commandOffset,
                                 const EspMapByteCode* command,
                                 uint32_t lineTargets,
                                 uint32_t spriteTargets,
                                 uint32_t entranceTargets,
                                 uint32_t lineMutations,
                                 uint32_t spriteMutations,
                                 uint32_t tileMutations,
                                 EspMapGiveMapStatus status,
                                 const EspMapGiveMapResult* result) {
    uint32_t globalCommandIndex;
    uint8_t mutated;

    if (descriptor == NULL || command == NULL || result == NULL ||
        lineTargets > 0xffffU || spriteTargets > 0xffffU ||
        entranceTargets > 0xffffU || lineMutations > 0xffffU ||
        spriteMutations > 0xffffU || tileMutations > 0xffffU) return 0;

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    mutated = (uint8_t)((lineMutations != 0U || spriteMutations != 0U ||
                         tileMutations != 0U) ? 1U : 0U);

    return status == ESP_MAP_GIVEMAP_OK &&
           result->sourceEventIndex == descriptor->eventIndex &&
           result->globalCommandIndex == (uint16_t)globalCommandIndex &&
           result->lineTargetCount == (uint16_t)lineTargets &&
           result->spriteTargetCount == (uint16_t)spriteTargets &&
           result->entranceTargetCount == (uint16_t)entranceTargets &&
           result->linesMutated == (uint16_t)lineMutations &&
           result->spritesMutated == (uint16_t)spriteMutations &&
           result->tilesMutated == (uint16_t)tileMutations &&
           result->sourceCommandOffset == (uint8_t)commandOffset &&
           result->mutated == mutated && result->legacyReturnValue == 1U &&
           result->removeCommandIfHandled ==
               (uint8_t)((command->arg2 &
                          ESP_MAP_GIVEMAP_COMMAND_FLAG_REMOVE) != 0U);
}

static int auditGiveMapCommands(GiveMapAudit* audit,
                                uint32_t initialAutomapFNV,
                                uint32_t initialMapStateFNV) {
    EspMapEventDescriptor descriptor;
    EspMapEventDescriptor badDescriptor;
    EspMapByteCode command;
    EspMapOpcodeExecResult opcodeResult;
    EspMapGiveMapResult result;
    EspMapGiveMapResult secondResult;
    EspMapGiveMapStatus status;
    uint32_t eventIndex;
    uint32_t commandOffset;
    uint32_t hash = 2166136261U;
    uint32_t lineTargets;
    uint32_t spriteTargets;
    uint32_t entranceTargets;
    uint32_t lineMutations;
    uint32_t spriteMutations;
    uint32_t tileMutations;
    uint32_t automapAfterFNV;
    uint32_t mapAfterFNV;
    uint8_t dummy;

    if (audit == NULL) return 0;
    memset(audit, 0, sizeof(*audit));

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command)) return 0;
            if (!audit->haveUnsupported && command.id != ESP_MAP_OPCODE_GIVEMAP) {
                audit->unsupportedDescriptor = descriptor;
                audit->unsupportedOffset = (uint8_t)commandOffset;
                audit->haveUnsupported = 1U;
            }
            if (command.id != ESP_MAP_OPCODE_GIVEMAP) continue;

            if (EspMapOpcodeExecutor_execute(&command, &opcodeResult) !=
                    ESP_MAP_OPCODE_EXEC_UNSUPPORTED ||
                opcodeResult.status != ESP_MAP_OPCODE_EXEC_UNSUPPORTED) return 0;
            ++audit->stateExecutorRefused;
            ++audit->refs;

            if (EspMapAutomapState_view() == NULL ||
                EspMapAutomapState_view()->stateFNV1a != initialAutomapFNV ||
                EspMapState_view() == NULL ||
                EspMapState_view()->stateFNV1a != initialMapStateFNV ||
                !currentGiveMapCounts(&lineTargets, &spriteTargets,
                                      &entranceTargets, &lineMutations,
                                      &spriteMutations, &tileMutations)) return 0;

            status = EspMapAutomapState_applyGiveMapCommand(
                &descriptor, commandOffset, &result);
            if (!validateGiveMapResult(&descriptor, commandOffset, &command,
                                       lineTargets, spriteTargets, entranceTargets,
                                       lineMutations, spriteMutations, tileMutations,
                                       status, &result)) return 0;

            audit->lineMutatedTotal += result.linesMutated;
            audit->spriteMutatedTotal += result.spritesMutated;
            audit->tileMutatedTotal += result.tilesMutated;
            if (result.removeCommandIfHandled != 0U) ++audit->removableRefs;
            if (result.mutated != 0U) ++audit->mutatedRefs;
            else ++audit->noMutationRefs;

            automapAfterFNV = EspMapAutomapState_view()->stateFNV1a;
            mapAfterFNV = EspMapState_view()->stateFNV1a;
            if (result.mutated != 0U &&
                automapAfterFNV == initialAutomapFNV &&
                mapAfterFNV == initialMapStateFNV) return 0;

            if (result.mutated != 0U && !audit->haveSample) {
                audit->sampleDescriptor = descriptor;
                audit->sampleCommand = command;
                audit->sampleResult = result;
                audit->sampleOffset = (uint8_t)commandOffset;
                audit->sampleMutatedAutomapFNV = automapAfterFNV;
                audit->sampleMutatedMapStateFNV = mapAfterFNV;
                audit->haveSample = 1U;
            }

            hash = hashU32(hash, resultHash(&result));
            if (!restoreInitialAutomap() ||
                EspMapAutomapState_view()->stateFNV1a != initialAutomapFNV ||
                EspMapState_view()->stateFNV1a != initialMapStateFNV) return 0;
            if (result.mutated != 0U) ++audit->rollbackProofs;
        }
    }

    if (audit->refs == 0U || audit->mutatedRefs == 0U ||
        audit->mutatedRefs + audit->noMutationRefs != audit->refs ||
        audit->stateExecutorRefused != audit->refs ||
        audit->rollbackProofs != audit->mutatedRefs ||
        !audit->haveSample || !audit->haveUnsupported) return 0;
    audit->giveMapFNV = hash;

    if (!currentGiveMapCounts(&lineTargets, &spriteTargets, &entranceTargets,
                              &lineMutations, &spriteMutations, &tileMutations)) {
        return 0;
    }
    status = EspMapAutomapState_applyGiveMapCommand(
        &audit->sampleDescriptor, audit->sampleOffset, &result);
    if (!validateGiveMapResult(&audit->sampleDescriptor, audit->sampleOffset,
                               &audit->sampleCommand, lineTargets, spriteTargets,
                               entranceTargets, lineMutations, spriteMutations,
                               tileMutations, status, &result) ||
        result.mutated == 0U) return 0;
    automapAfterFNV = EspMapAutomapState_view()->stateFNV1a;
    mapAfterFNV = EspMapState_view()->stateFNV1a;

    if (!currentGiveMapCounts(&lineTargets, &spriteTargets, &entranceTargets,
                              &lineMutations, &spriteMutations, &tileMutations) ||
        lineMutations != 0U || spriteMutations != 0U || tileMutations != 0U) {
        return 0;
    }
    status = EspMapAutomapState_applyGiveMapCommand(
        &audit->sampleDescriptor, audit->sampleOffset, &secondResult);
    if (!validateGiveMapResult(&audit->sampleDescriptor, audit->sampleOffset,
                               &audit->sampleCommand, lineTargets, spriteTargets,
                               entranceTargets, 0U, 0U, 0U, status,
                               &secondResult) || secondResult.mutated != 0U ||
        secondResult.legacyReturnValue != 1U ||
        secondResult.removeCommandIfHandled != result.removeCommandIfHandled ||
        EspMapAutomapState_view()->stateFNV1a != automapAfterFNV ||
        EspMapState_view()->stateFNV1a != mapAfterFNV) return 0;
    if (!restoreInitialAutomap() ||
        EspMapAutomapState_view()->stateFNV1a != initialAutomapFNV ||
        EspMapState_view()->stateFNV1a != initialMapStateFNV) return 0;
    audit->idempotentHandledProof = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapAutomapState_applyGiveMapCommand(
            &audit->unsupportedDescriptor, audit->unsupportedOffset, &result) !=
            ESP_MAP_GIVEMAP_UNSUPPORTED || !resultIsZero(&result)) return 0;
    audit->unsupportedRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapAutomapState_applyGiveMapCommand(
            &audit->sampleDescriptor, audit->sampleDescriptor.commandCount,
            &result) != ESP_MAP_GIVEMAP_INVALID || !resultIsZero(&result)) return 0;
    audit->badOffsetRefused = 1U;

    badDescriptor = audit->sampleDescriptor;
    badDescriptor.value ^= 0x20000000UL;
    memset(&result, 0xa5, sizeof(result));
    if (EspMapAutomapState_applyGiveMapCommand(
            &badDescriptor, audit->sampleOffset, &result) !=
            ESP_MAP_GIVEMAP_INVALID || !resultIsZero(&result)) return 0;
    audit->badDescriptorRefused = 1U;

    memset(&result, 0xa5, sizeof(result));
    if (EspMapAutomapState_applyGiveMapCommand(NULL, 0U, &result) !=
            ESP_MAP_GIVEMAP_INVALID || !resultIsZero(&result)) return 0;
    audit->nullDescriptorRefused = 1U;

    if (EspMapAutomapState_applyGiveMapCommand(
            &audit->sampleDescriptor, audit->sampleOffset, NULL) !=
            ESP_MAP_GIVEMAP_INVALID) return 0;
    audit->nullResultRefused = 1U;

    if (EspMapAutomapState_getLineRevealed(EXPECTED_LINE_COUNT, &dummy) ||
        EspMapAutomapState_setLineRevealed(EXPECTED_LINE_COUNT, 1U)) return 0;
    audit->badLineIndexRefused = 1U;
    if (EspMapAutomapState_getSpriteRevealed(EXPECTED_MAP_SPRITE_COUNT, &dummy) ||
        EspMapAutomapState_setSpriteRevealed(EXPECTED_MAP_SPRITE_COUNT, 1U)) return 0;
    audit->badSpriteIndexRefused = 1U;
    if (EspMapAutomapState_setLineRevealed(0U, 2U) ||
        EspMapAutomapState_setSpriteRevealed(0U, 2U)) return 0;
    audit->badRevealValueRefused = 1U;
    if (EspMapState_setVisited(ESP_MAP_STATE_TILE_COUNT, 1U)) return 0;
    audit->badVisitedIndexRefused = 1U;
    if (EspMapState_setVisited(0U, 2U)) return 0;
    audit->badVisitedValueRefused = 1U;

    return EspMapAutomapState_view() != NULL &&
           EspMapAutomapState_view()->stateFNV1a == initialAutomapFNV &&
           EspMapState_view() != NULL &&
           EspMapState_view()->stateFNV1a == initialMapStateFNV;
}

void Esp32Map1GiveMapProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapAutomapState_reset();
}

void Esp32Map1GiveMapProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapAutomapStateView* automapState;
    EspMapEventDescriptor firstDescriptor;
    EspMapGiveMapResult notReadyResult;
    GiveMapAudit audit;
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
    uint32_t automapFNV;
    uint32_t initialLineRevealed;
    uint32_t initialSpriteRevealed;
    uint32_t lineTargets;
    uint32_t spriteTargets;
    uint32_t entranceTargets;
    uint32_t lineMutations;
    uint32_t spriteMutations;
    uint32_t tileMutations;
    uint32_t startedMs;
    uint32_t elapsedMs;
    uint32_t notReadyRefused = 0U;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1UnlockProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPGIVEMAPPROBE] ARMED native UNLOCK world state proven; EV_GIVEMAP automap ownership starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO GIVEMAP automap state ===\n");
    printf("[MAPGIVEMAPPROBE] CONTRACT preserve proven line/texture world owners + add packed line/sprite reveal state (103B), mutate existing tile VISITED bits only for ENTRANCE cells, execute only EV_GIVEMAP, rollback every probe mutation; no legacy Render/Entity/gameplay mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPGIVEMAPPROBE] FAILED unsafe precondition\n");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        lineState == NULL || textureState == NULL ||
        !descriptorByIndex(0U, &firstDescriptor)) {
        printf("[MAPGIVEMAPPROBE] FAILED native prerequisites unavailable\n");
        return;
    }

    memset(&notReadyResult, 0xa5, sizeof(notReadyResult));
    if (EspMapAutomapState_applyGiveMapCommand(&firstDescriptor, 0U,
                                               &notReadyResult) !=
            ESP_MAP_GIVEMAP_NOT_READY || !resultIsZero(&notReadyResult)) {
        printf("[MAPGIVEMAPPROBE] FAILED not-ready fail-closed\n");
        return;
    }
    notReadyRefused = 1U;

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                              (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = hudWitnessHash(doomRpg->hud);
    passwordCanvasBefore = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationBefore = gameContinuationHash(doomRpg->game);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspMapAutomapState_buildFromRuntime()) {
        printf("[MAPGIVEMAPPROBE] FAILED automap state build\n");
        return;
    }
    heapAfterBuild = heap8Free();
    automapState = EspMapAutomapState_view();
    if (!validateAutomapState(automapState, &automapFNV,
                              &initialLineRevealed,
                              &initialSpriteRevealed) ||
        !currentGiveMapCounts(&lineTargets, &spriteTargets, &entranceTargets,
                              &lineMutations, &spriteMutations, &tileMutations) ||
        mapStateBefore != EXPECTED_MAP_STATE_FNV) {
        printf("[MAPGIVEMAPPROBE] FAILED initial automap validation\n");
        return;
    }

    if (!auditGiveMapCommands(&audit, automapFNV, mapStateBefore)) {
        printf("[MAPGIVEMAPPROBE] FAILED GIVEMAP corpus/rollback audit\n");
        return;
    }

    heapAfterAudit = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    arenaAfter = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateAfter = EspMapState_view()->stateFNV1a;
    scriptAfter = fnv1a32(EspMapScriptState_view()->storage,
                           EspMapScriptState_view()->storageBytes);
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = hudWitnessHash(doomRpg->hud);
    passwordCanvasAfter = canvasPasswordWitnessHash(doomRpg->doomCanvas);
    continuationAfter = gameContinuationHash(doomRpg->game);
    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;

    if (heapBefore < heapAfterBuild || heapAfterBuild != heapAfterAudit) {
        printf("[MAPGIVEMAPPROBE] FAILED heap lifecycle before=%u build=%u audit=%u\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfterBuild,
               (unsigned int)heapAfterAudit);
        return;
    }
    heapCost = heapBefore - heapAfterBuild;
    if (heapCost < EXPECTED_AUTOMAP_STORAGE_BYTES ||
        heapCost > EXPECTED_AUTOMAP_STORAGE_BYTES + MAX_ALLOCATOR_OVERHEAD) {
        printf("[MAPGIVEMAPPROBE] FAILED heap cost=%u payload=%u\n",
               (unsigned int)heapCost,
               (unsigned int)EXPECTED_AUTOMAP_STORAGE_BYTES);
        return;
    }
    allocatorOverhead = heapCost - EXPECTED_AUTOMAP_STORAGE_BYTES;

    automapState = EspMapAutomapState_view();
    if (automapState == NULL || automapState->stateFNV1a != automapFNV ||
        sizeof(EspMapGiveMapResult) != EXPECTED_RESULT_BYTES ||
        largestAfter < MIN_LARGEST8_AFTER_STATE ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV || scriptAfter != scriptBefore ||
        scriptAfter != EXPECTED_SCRIPT_FNV ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        EspMapLineTextureState_view() == NULL ||
        EspMapLineTextureState_view()->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        notebookAfter != notebookBefore ||
        notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV || keysAfter != keysBefore ||
        hudAfter != hudBefore || passwordCanvasAfter != passwordCanvasBefore ||
        continuationAfter != continuationBefore || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPGIVEMAPPROBE] FAILED integrity regression\n");
        return;
    }

    printf("[MAPGIVEMAP] READY refs=%u mutated=%u noMutation=%u removable=%u resultBytes=%u stateExecRefused=%u lineTargets=%u spriteTargets=%u entranceTargets=%u lineMutTotal=%u spriteMutTotal=%u tileMutTotal=%u giveMapFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs,
           (unsigned int)audit.mutatedRefs,
           (unsigned int)audit.noMutationRefs,
           (unsigned int)audit.removableRefs,
           (unsigned int)sizeof(EspMapGiveMapResult),
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.sampleResult.lineTargetCount,
           (unsigned int)audit.sampleResult.spriteTargetCount,
           (unsigned int)audit.sampleResult.entranceTargetCount,
           (unsigned int)audit.lineMutatedTotal,
           (unsigned int)audit.spriteMutatedTotal,
           (unsigned int)audit.tileMutatedTotal,
           (unsigned int)audit.giveMapFNV,
           (unsigned int)elapsedMs);
    printf("[MAPGIVEMAP] SAMPLE cmd=%u event=%u off=%u lineMut=%u spriteMut=%u tileMut=%u handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.sampleResult.globalCommandIndex,
           (unsigned int)audit.sampleResult.sourceEventIndex,
           (unsigned int)audit.sampleResult.sourceCommandOffset,
           (unsigned int)audit.sampleResult.linesMutated,
           (unsigned int)audit.sampleResult.spritesMutated,
           (unsigned int)audit.sampleResult.tilesMutated,
           (unsigned int)audit.sampleResult.legacyReturnValue,
           (unsigned int)audit.sampleResult.removeCommandIfHandled);
    printf("[MAPGIVEMAP] WORLD lineBytes=%u spriteBytes=%u storageBytes=%u initialLineRevealed=%u initialSpriteRevealed=%u automapStateFNV=%08x mutatedAutomapFNV=%08x mapStateFNV=%08x mutatedMapStateFNV=%08x rollback=%u/%u idempotentHandled=%u\n",
           (unsigned int)automapState->lineBitsetBytes,
           (unsigned int)automapState->spriteBitsetBytes,
           (unsigned int)automapState->storageBytes,
           (unsigned int)initialLineRevealed,
           (unsigned int)initialSpriteRevealed,
           (unsigned int)automapFNV,
           (unsigned int)audit.sampleMutatedAutomapFNV,
           (unsigned int)mapStateBefore,
           (unsigned int)audit.sampleMutatedMapStateFNV,
           (unsigned int)audit.rollbackProofs,
           (unsigned int)audit.mutatedRefs,
           (unsigned int)audit.idempotentHandledProof);
    printf("[MAPGIVEMAP] FAILCLOSED notReady=%u unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullResult=%u badLineIndex=%u badSpriteIndex=%u badRevealValue=%u badVisitedIndex=%u badVisitedValue=%u stateAtomic=yes worldRestored=yes\n",
           (unsigned int)notReadyRefused,
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullResultRefused,
           (unsigned int)audit.badLineIndexRefused,
           (unsigned int)audit.badSpriteIndexRefused,
           (unsigned int)audit.badRevealValueRefused,
           (unsigned int)audit.badVisitedIndexRefused,
           (unsigned int)audit.badVisitedValueRefused);
    printf("[MAPGIVEMAPPROBE] RAM heap8=%u->%u persistentHeapCost=%u payload=%u allocatorOverhead=%u largest8=%u->%u frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x lineStateFNV=%08x textureStateFNV=%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfterAudit,
           (unsigned int)heapCost,
           (unsigned int)EXPECTED_AUTOMAP_STORAGE_BYTES,
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
           (unsigned int)EspMapLineState_view()->stateFNV1a,
           (unsigned int)EspMapLineTextureState_view()->stateFNV1a);
    printf("[MAPGIVEMAPPROBE] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x packIO=no legacyRuntimeClear=yes\n",
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
    printf("[MAPGIVEMAPPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes nativeLineState=yes nativeDoorExec=yes nativeLineTextureState=yes nativeUnlockExec=yes nativeAutomapState=yes nativeGiveMapExec=yes storageBytes=%u resultBytes=%u worldMutationProven=yes worldRestored=yes legacyWorldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)automapState->storageBytes,
           (unsigned int)sizeof(EspMapGiveMapResult),
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);

    probeState.done = 1;
}

int Esp32Map1GiveMapProbe_isDone(void) {
    return probeState.done;
}
