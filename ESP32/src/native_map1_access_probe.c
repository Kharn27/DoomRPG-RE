#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_map_runtime.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_runtime_load.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_TEXTURE_RESOURCES 33U
#define EXPECTED_SPRITE_RESOURCES 45U
#define EXPECTED_PLANE_TEXTURES 12U

typedef struct Esp32Map1AccessProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1AccessProbeState;

static Esp32Map1AccessProbeState probeState;

static uint16_t readLe16(const uint8_t* bytes) {
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static uint32_t readLe32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    hash *= 16777619U;
    return hash;
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

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    uint32_t hash = 2166136261U;
    size_t i;

    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }

    for (i = 0; i < bytes; ++i) {
        hash = hashByte(hash, framebuffer[i]);
    }
    return hash;
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
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->tileEvents == NULL &&
           render->mapByteCode == NULL &&
           render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() &&
           EspMapRuntime_isLoaded() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int validateAccessors(const EspMapRuntimeView* view,
                             uint32_t* outDecodedFNV,
                             uint32_t blockCounts[4],
                             uint32_t* outTextureCount,
                             uint32_t* outSpriteCount,
                             uint32_t* outPlaneCount,
                             uint16_t* outFirstString,
                             uint16_t* outLastString) {
    EspMapNode node;
    EspMapLine line;
    EspMapSprite sprite;
    EspMapByteCode byteCode;
    uint32_t eventValue;
    uint32_t hash = 2166136261U;
    uint32_t i;
    uint32_t plane;
    uint32_t expected;
    uint32_t textureCount = 0U;
    uint32_t spriteCount = 0U;
    uint32_t planeCount = 0U;
    uint16_t stringOffset = 0U;
    uint16_t previousStringOffset = 0U;
    uint8_t value;
    const uint8_t* raw;

    if (view == NULL || outDecodedFNV == NULL || blockCounts == NULL ||
        outTextureCount == NULL || outSpriteCount == NULL ||
        outPlaneCount == NULL || outFirstString == NULL ||
        outLastString == NULL || view->arenaBytes != EXPECTED_ARENA_BYTES ||
        view->arenaFNV1a != EXPECTED_ARENA_FNV) {
        return 0;
    }

    memset(blockCounts, 0, 4U * sizeof(blockCounts[0]));

    for (i = 0; i < view->nodeCount; ++i) {
        raw = view->nodes + (i * ESP_MAP_NODE_RECORD_BYTES);
        if (!EspMapRuntime_getNode(i, &node) ||
            node.x1 != ((uint16_t)raw[0] << 3) ||
            node.y1 != ((uint16_t)raw[1] << 3) ||
            node.x2 != ((uint16_t)raw[2] << 3) ||
            node.y2 != ((uint16_t)raw[3] << 3) ||
            node.args1 != (((uint32_t)raw[4] << 16) |
                           ((uint32_t)raw[5] << 3)) ||
            node.args2 != ((uint32_t)readLe16(raw + 6U) |
                           ((uint32_t)readLe16(raw + 8U) << 16))) {
            return 0;
        }
        hash = hashU16(hash, node.x1);
        hash = hashU16(hash, node.y1);
        hash = hashU16(hash, node.x2);
        hash = hashU16(hash, node.y2);
        hash = hashU32(hash, node.args1);
        hash = hashU32(hash, node.args2);
    }

    for (i = 0; i < view->lineCount; ++i) {
        raw = view->lines + (i * ESP_MAP_LINE_RECORD_BYTES);
        if (!EspMapRuntime_getLine(i, &line) ||
            line.x1 != ((uint16_t)raw[0] << 3) ||
            line.y1 != ((uint16_t)raw[1] << 3) ||
            line.x2 != ((uint16_t)raw[2] << 3) ||
            line.y2 != ((uint16_t)raw[3] << 3) ||
            line.texture != readLe16(raw + 4U) ||
            line.flags != readLe32(raw + 6U)) {
            return 0;
        }
        hash = hashU16(hash, line.x1);
        hash = hashU16(hash, line.y1);
        hash = hashU16(hash, line.x2);
        hash = hashU16(hash, line.y2);
        hash = hashU16(hash, line.texture);
        hash = hashU32(hash, line.flags);
    }

    for (i = 0; i < view->mapSpriteCount; ++i) {
        raw = view->mapSprites + (i * ESP_MAP_SPRITE_RECORD_BYTES);
        expected = (uint32_t)raw[2] |
                   ((uint32_t)readLe16(raw + 3U) << 16);
        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            sprite.x != ((uint16_t)raw[0] << 3) ||
            sprite.y != ((uint16_t)raw[1] << 3) ||
            sprite.info != expected) {
            return 0;
        }
        hash = hashU16(hash, sprite.x);
        hash = hashU16(hash, sprite.y);
        hash = hashU32(hash, sprite.info);
    }

    for (i = 0; i < view->eventCount; ++i) {
        raw = view->events + (i * ESP_MAP_EVENT_RECORD_BYTES);
        if (!EspMapRuntime_getEvent(i, &eventValue) ||
            eventValue != readLe32(raw)) {
            return 0;
        }
        hash = hashU32(hash, eventValue);
    }

    for (i = 0; i < view->byteCodeCount; ++i) {
        raw = view->byteCodes + (i * ESP_MAP_BYTECODE_RECORD_BYTES);
        if (!EspMapRuntime_getByteCode(i, &byteCode) ||
            byteCode.id != raw[0] ||
            byteCode.arg1 != readLe32(raw + 1U) ||
            byteCode.arg2 != readLe32(raw + 5U)) {
            return 0;
        }
        hash = hashByte(hash, byteCode.id);
        hash = hashU32(hash, byteCode.arg1);
        hash = hashU32(hash, byteCode.arg2);
    }

    for (i = 0; i < view->stringCount; ++i) {
        if (!EspMapRuntime_getStringSourceOffset(i, &stringOffset) ||
            stringOffset >= view->sourceBytes ||
            (i > 0U && stringOffset <= previousStringOffset)) {
            return 0;
        }
        if (i == 0U) {
            *outFirstString = stringOffset;
        }
        previousStringOffset = stringOffset;
        hash = hashU16(hash, stringOffset);
    }
    *outLastString = previousStringOffset;

    for (i = 0; i < ESP_MAP_BLOCK_CELL_COUNT; ++i) {
        expected = (uint32_t)((view->blockMap[i >> 2] >>
                              ((i & 3U) * 2U)) & 3U);
        if (!EspMapRuntime_getBlockCell(i, &value) || value != expected) {
            return 0;
        }
        ++blockCounts[value];
        hash = hashByte(hash, value);
    }

    for (plane = 0; plane < ESP_MAP_PLANE_COUNT; ++plane) {
        for (i = 0; i < ESP_MAP_PLANE_CELL_COUNT; ++i) {
            expected = view->planeMap[(plane * ESP_MAP_PLANE_CELL_COUNT) + i];
            if (!EspMapRuntime_getPlaneTexture(plane, i, &value) ||
                value != expected) {
                return 0;
            }
            hash = hashByte(hash, value);
        }
    }

    for (i = 0; i < ESP_BSP_RESOURCE_ID_LIMIT; ++i) {
        value = (uint8_t)(EspMapRuntime_textureRequired(i) ? 1U : 0U);
        textureCount += value;
        hash = hashByte(hash, value);

        value = (uint8_t)(EspMapRuntime_spriteRequired(i) ? 1U : 0U);
        spriteCount += value;
        hash = hashByte(hash, value);

        value = (uint8_t)(EspMapRuntime_planeTextureUsed(i) ? 1U : 0U);
        planeCount += value;
        hash = hashByte(hash, value);
    }

    if (textureCount != EXPECTED_TEXTURE_RESOURCES ||
        spriteCount != EXPECTED_SPRITE_RESOURCES ||
        planeCount != EXPECTED_PLANE_TEXTURES ||
        EspMapRuntime_getNode(view->nodeCount, &node) ||
        EspMapRuntime_getLine(view->lineCount, &line) ||
        EspMapRuntime_getMapSprite(view->mapSpriteCount, &sprite) ||
        EspMapRuntime_getEvent(view->eventCount, &eventValue) ||
        EspMapRuntime_getByteCode(view->byteCodeCount, &byteCode) ||
        EspMapRuntime_getStringSourceOffset(view->stringCount, &stringOffset) ||
        EspMapRuntime_getBlockCell(ESP_MAP_BLOCK_CELL_COUNT, &value) ||
        EspMapRuntime_getPlaneTexture(ESP_MAP_PLANE_COUNT, 0U, &value) ||
        EspMapRuntime_getPlaneTexture(0U, ESP_MAP_PLANE_CELL_COUNT, &value) ||
        EspMapRuntime_textureRequired(ESP_BSP_RESOURCE_ID_LIMIT) ||
        EspMapRuntime_spriteRequired(ESP_BSP_RESOURCE_ID_LIMIT) ||
        EspMapRuntime_planeTextureUsed(ESP_BSP_RESOURCE_ID_LIMIT)) {
        return 0;
    }

    *outDecodedFNV = hash;
    *outTextureCount = textureCount;
    *outSpriteCount = spriteCount;
    *outPlaneCount = planeCount;
    return 1;
}

void Esp32Map1AccessProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1AccessProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* view;
    EspMapNode node0;
    EspMapLine line0;
    EspMapSprite sprite0;
    EspMapByteCode code0;
    uint32_t event0;
    uint16_t string0;
    uint32_t decodedFNV;
    uint32_t blockCounts[4];
    uint32_t textureCount;
    uint32_t spriteCount;
    uint32_t planeCount;
    uint16_t firstString;
    uint16_t lastString;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) {
        return;
    }

    if (!Esp32Map1RuntimeLoad_isDone()) {
        return;
    }

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPACCESS] ARMED native arena resident; read-only accessor validation starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO compact access contract ===\n");
    printf("[MAPACCESS] CONTRACT allocation-free indexed decode over resident arena; no mutation, overlays, entities, rendering or gameplay\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPACCESS] FAILED precondition heap8=%u largest8=%u nativeLoaded=%d\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               EspMapRuntime_isLoaded());
        return;
    }

    view = EspMapRuntime_view();
    if (view == NULL || view->arenaBytes != EXPECTED_ARENA_BYTES ||
        view->arenaFNV1a != EXPECTED_ARENA_FNV) {
        printf("[MAPACCESS] FAILED arena regression bytes=%u fnv=%08x\n",
               view != NULL ? (unsigned int)view->arenaBytes : 0U,
               view != NULL ? (unsigned int)view->arenaFNV1a : 0U);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    startedMs = DoomRPG_GetUpTimeMS();

    if (!validateAccessors(view,
                           &decodedFNV,
                           blockCounts,
                           &textureCount,
                           &spriteCount,
                           &planeCount,
                           &firstString,
                           &lastString)) {
        printf("[MAPACCESS] FAILED accessor validation\n");
        return;
    }

    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();

    if (!boundaryIsSafe(doomRpg) || heapAfter != heapBefore ||
        largestAfter != largestBefore || frameAfter != frameBefore) {
        printf("[MAPACCESS] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (unsigned int)frameBefore,
               (unsigned int)frameAfter);
        return;
    }

    if (!EspMapRuntime_getNode(0U, &node0) ||
        !EspMapRuntime_getLine(0U, &line0) ||
        !EspMapRuntime_getMapSprite(0U, &sprite0) ||
        !EspMapRuntime_getEvent(0U, &event0) ||
        !EspMapRuntime_getByteCode(0U, &code0) ||
        !EspMapRuntime_getStringSourceOffset(0U, &string0)) {
        printf("[MAPACCESS] FAILED sample decode\n");
        return;
    }

    probeState.done = 1;
    printf("[MAPACCESS] READY decodedFNV=%08x elapsed=%ums nodes=%u lines=%u sprites=%u events=%u byteCodes=%u strings=%u blockCells=%u planeCells=%u\n",
           (unsigned int)decodedFNV,
           (unsigned int)elapsedMs,
           (unsigned int)view->nodeCount,
           (unsigned int)view->lineCount,
           (unsigned int)view->mapSpriteCount,
           (unsigned int)view->eventCount,
           (unsigned int)view->byteCodeCount,
           (unsigned int)view->stringCount,
           (unsigned int)ESP_MAP_BLOCK_CELL_COUNT,
           (unsigned int)(ESP_MAP_PLANE_COUNT * ESP_MAP_PLANE_CELL_COUNT));
    printf("[MAPACCESS] SAMPLE node0=%u,%u-%u,%u args=%08x/%08x line0=%u,%u-%u,%u tex=%u flags=%08x sprite0=%u,%u info=%08x event0=%08x code0=%u/%08x/%08x string0=%u\n",
           (unsigned int)node0.x1,
           (unsigned int)node0.y1,
           (unsigned int)node0.x2,
           (unsigned int)node0.y2,
           (unsigned int)node0.args1,
           (unsigned int)node0.args2,
           (unsigned int)line0.x1,
           (unsigned int)line0.y1,
           (unsigned int)line0.x2,
           (unsigned int)line0.y2,
           (unsigned int)line0.texture,
           (unsigned int)line0.flags,
           (unsigned int)sprite0.x,
           (unsigned int)sprite0.y,
           (unsigned int)sprite0.info,
           (unsigned int)event0,
           (unsigned int)code0.id,
           (unsigned int)code0.arg1,
           (unsigned int)code0.arg2,
           (unsigned int)string0);
    printf("[MAPACCESS] BLOCK flags0=%u flags1=%u flags2=%u flags3=%u resources=%u/%u/%u strings=%u..%u boundsChecks=yes\n",
           (unsigned int)blockCounts[0],
           (unsigned int)blockCounts[1],
           (unsigned int)blockCounts[2],
           (unsigned int)blockCounts[3],
           (unsigned int)textureCount,
           (unsigned int)spriteCount,
           (unsigned int)planeCount,
           (unsigned int)firstString,
           (unsigned int)lastString);
    printf("[MAPACCESS] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)view->arenaFNV1a);
    printf("[MAPACCESS] PARK state=%d page=%d nativeArena=yes immutable=yes overlays=none entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
}

int Esp32Map1AccessProbe_isDone(void) {
    return probeState.done;
}