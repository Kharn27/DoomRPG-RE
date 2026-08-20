#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"
#include "Z_Zip.h"

#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_structural_load.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"

#include <esp_heap_caps.h>

#define MAP_HEADER_BYTES 33U
#define ESP32_TINFL_STATE_BYTES 10992U
#define MAP_STRUCTURAL_SAFETY_HEADROOM 4096U
#define MAP_NODE_RECORD_BYTES 10U
#define MAP_LINE_RECORD_BYTES 10U
#define MAP_SPRITE_RECORD_BYTES 5U
#define MAP_EVENT_RECORD_BYTES 4U
#define MAP_BYTECODE_RECORD_BYTES 9U
#define MAP_BLOCKMAP_BYTES 256U
#define MAP_PLANE_TEXTURE_BYTES (2U * 1024U)

typedef struct Esp32Map1Plan_s {
    uint32_t nodes;
    uint32_t lines;
    uint32_t mapSprites;
    uint32_t runtimeSprites;
    uint32_t events;
    uint32_t byteCodes;
    uint32_t strings;
    uint32_t stringBytes;
    uint32_t parsedBytes;
    uint32_t trailingBytes;
    uint64_t structuralBytes;
    uint64_t largestAllocation;
} Esp32Map1Plan;

typedef struct Esp32Map1StructuralState_s {
    int armed;
    int attempted;
    int active;
    int boundaryCaptured;
    int done;
    uint32_t boundaryHeap8;
    uint32_t boundaryLargest8;
} Esp32Map1StructuralState;

static Esp32Map1StructuralState map1State;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static const zip_entry_t* findZipEntryForPath(const char* path) {
    const char* name;
    int i;

    if (path == NULL || zipFile.entry == NULL) {
        return NULL;
    }

    name = path[0] == '/' ? path + 1 : path;
    for (i = 0; i < zipFile.entry_count; ++i) {
        const zip_entry_t* entry = &zipFile.entry[i];
        if (entry->name != NULL && SDL_strcasecmp(name, entry->name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static int zipInflatePlanFits(const char* label,
                              const zip_entry_t* entry,
                              uint32_t heap,
                              uint32_t largest) {
    uint64_t transient;

    if (entry == NULL || entry->csize <= 0 || entry->usize <= 0) {
        printf("[MAP1STRUCT] REFUSED %s ZIP entry missing/invalid\n",
               label != NULL ? label : "resource");
        return 0;
    }

    transient = (uint64_t)(uint32_t)entry->csize +
                (uint64_t)(uint32_t)entry->usize +
                (uint64_t)ESP32_TINFL_STATE_BYTES;

    printf("[MAP1STRUCT] ZIP %s c=%d u=%d inflateState=%u transient=%llu heap8=%u largest8=%u\n",
           label != NULL ? label : "resource",
           entry->csize,
           entry->usize,
           (unsigned int)ESP32_TINFL_STATE_BYTES,
           (unsigned long long)transient,
           (unsigned int)heap,
           (unsigned int)largest);

    if ((uint32_t)entry->csize > largest ||
        (uint32_t)entry->usize > largest ||
        ESP32_TINFL_STATE_BYTES > largest ||
        transient > heap) {
        printf("[MAP1STRUCT] REFUSED %s legacy ZIP inflate set does not fit current heap\n",
               label != NULL ? label : "resource");
        return 0;
    }

    return 1;
}

static int readU16(const byte* data,
                   uint32_t size,
                   uint32_t* pos,
                   uint32_t* value) {
    if (data == NULL || pos == NULL || value == NULL || *pos + 2U > size) {
        return 0;
    }

    *value = (uint32_t)data[*pos] | ((uint32_t)data[*pos + 1U] << 8);
    *pos += 2U;
    return 1;
}

static int skipBytes(uint32_t size, uint32_t* pos, uint64_t bytes) {
    if (pos == NULL || bytes > UINT32_MAX ||
        (uint64_t)*pos + bytes > (uint64_t)size) {
        return 0;
    }

    *pos += (uint32_t)bytes;
    return 1;
}

static uint64_t maxU64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

static int parseStructuralPlan(const byte* data,
                               uint32_t size,
                               Esp32Map1Plan* plan) {
    uint32_t pos = MAP_HEADER_BYTES;
    uint32_t i;
    uint32_t stringLength;
    uint64_t bytes;

    if (data == NULL || plan == NULL || size < MAP_HEADER_BYTES) {
        return 0;
    }

    SDL_memset(plan, 0, sizeof(*plan));

    if (!readU16(data, size, &pos, &plan->nodes) ||
        !skipBytes(size, &pos, (uint64_t)plan->nodes * MAP_NODE_RECORD_BYTES) ||
        !readU16(data, size, &pos, &plan->lines) ||
        !skipBytes(size, &pos, (uint64_t)plan->lines * MAP_LINE_RECORD_BYTES) ||
        !readU16(data, size, &pos, &plan->mapSprites) ||
        !skipBytes(size, &pos,
                   (uint64_t)plan->mapSprites * MAP_SPRITE_RECORD_BYTES) ||
        !readU16(data, size, &pos, &plan->events) ||
        !skipBytes(size, &pos, (uint64_t)plan->events * MAP_EVENT_RECORD_BYTES) ||
        !readU16(data, size, &pos, &plan->byteCodes) ||
        !skipBytes(size, &pos,
                   (uint64_t)plan->byteCodes * MAP_BYTECODE_RECORD_BYTES) ||
        !readU16(data, size, &pos, &plan->strings)) {
        return 0;
    }

    for (i = 0; i < plan->strings; ++i) {
        if (!readU16(data, size, &pos, &stringLength) ||
            !skipBytes(size, &pos, stringLength)) {
            return 0;
        }
        plan->stringBytes += stringLength + 1U;
    }

    if (!skipBytes(size, &pos, MAP_BLOCKMAP_BYTES) ||
        !skipBytes(size, &pos, MAP_PLANE_TEXTURE_BYTES)) {
        return 0;
    }

    plan->runtimeSprites =
        plan->mapSprites + MAX_CUSTOM_SPRITES + MAX_DROP_SPRITES;
    plan->parsedBytes = pos;
    plan->trailingBytes = size - pos;

    plan->structuralBytes =
        (uint64_t)256U * sizeof(int) +
        (uint64_t)1024U * sizeof(int) +
        (uint64_t)plan->nodes * sizeof(Node_t) +
        (uint64_t)plan->lines * sizeof(Line_t) +
        (uint64_t)plan->runtimeSprites * sizeof(Sprite_t) +
        (uint64_t)plan->events * sizeof(int) +
        (uint64_t)plan->byteCodes * BYTE_CODE_MAX * sizeof(int) +
        (uint64_t)plan->strings * sizeof(char*) +
        (uint64_t)plan->stringBytes;

    plan->largestAllocation = (uint64_t)1024U * sizeof(int);
    bytes = (uint64_t)plan->nodes * sizeof(Node_t);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);
    bytes = (uint64_t)plan->lines * sizeof(Line_t);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);
    bytes = (uint64_t)plan->runtimeSprites * sizeof(Sprite_t);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);
    bytes = (uint64_t)plan->events * sizeof(int);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);
    bytes = (uint64_t)plan->byteCodes * BYTE_CODE_MAX * sizeof(int);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);
    bytes = (uint64_t)plan->strings * sizeof(char*);
    plan->largestAllocation = maxU64(plan->largestAllocation, bytes);

    return 1;
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

static int runtimeIsClear(const Render_t* render) {
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

static int preBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL) {
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    return Esp32IntroDispose_isDone() &&
           !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO &&
           canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 &&
           canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           runtimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static int structuralBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->render == NULL || doomRpg->game == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return map1State.boundaryCaptured &&
           render->mapNameID == MAP_INTRO &&
           render->ioBuffer == NULL &&
           render->nodesLength > 0 && render->nodes != NULL &&
           render->linesLength > 0 && render->lines != NULL &&
           render->numMapSprites >= 0 &&
           render->numSprites ==
               render->numMapSprites + MAX_CUSTOM_SPRITES + MAX_DROP_SPRITES &&
           render->mapSprites != NULL &&
           render->mapTextureTexels != NULL &&
           render->mapSpriteTexels != NULL &&
           render->mapTextureTexelsCount > 0 &&
           render->mapTextureTexelsCount <= 256 &&
           render->mapSpriteTexelsCount > 0 &&
           render->mapSpriteTexelsCount <= 1024 &&
           render->planeTexturesCnt > 0 && render->planeTexturesCnt <= 24 &&
           render->mediaTexelOffsets != NULL &&
           render->mediaBitShapeOffsets != NULL &&
           render->mediaTexturesIds != NULL &&
           render->mediaSpriteIds != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive() &&
           doomRpg->game->numEntities == 0 &&
           doomRpg->game->numMonsters == 0;
}

static void cleanupFailedLoad(Render_t* render) {
    if (render == NULL) {
        return;
    }

    if (render->ioBuffer != NULL) {
        SDL_free(render->ioBuffer);
        render->ioBuffer = NULL;
    }
    Render_freeRuntime(render);
}

void Esp32Map1StructuralLoad_reset(void) {
    SDL_memset(&map1State, 0, sizeof(map1State));
}

int Esp32Map1StructuralLoad_captureBoundary(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;

    if (!map1State.active) {
        return 0;
    }

    map1State.boundaryCaptured = 1;
    map1State.boundaryHeap8 = heap8Free();
    map1State.boundaryLargest8 = largest8Block();

    printf("[MAP1STRUCT] CAPTURE after BSP structural parse / before bitshapes+texels heap8=%u largest8=%u\n",
           (unsigned int)map1State.boundaryHeap8,
           (unsigned int)map1State.boundaryLargest8);
    if (render != NULL) {
        printf("[MAP1STRUCT] CAPTURE counts nodes=%d lines=%d mapSprites=%d runtimeSprites=%d events=%d strings=%d refs=%d/%d planes=%d\n",
               render->nodesLength,
               render->linesLength,
               render->numMapSprites,
               render->numSprites,
               render->numTileEvents,
               render->mapStringCount,
               render->mapTextureTexelsCount,
               render->mapSpriteTexelsCount,
               render->planeTexturesCnt);
    }

    return 1;
}

void Esp32Map1StructuralLoad_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    Render_t* render;
    const zip_entry_t* mappingsEntry;
    const zip_entry_t* mapEntry;
    const char* mapFile;
    byte* planData = NULL;
    Esp32Map1Plan plan;
    boolean beginResult;
    boolean dataResult;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfterMappings;
    uint32_t largestAfterMappings;
    uint32_t heapWithPlanData;
    uint32_t largestWithPlanData;
    uint32_t heapAfterPlan;
    uint32_t heapAfterBegin;
    uint32_t largestAfterBegin;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint64_t requiredWithRaw;

    if (map1State.done || map1State.attempted || doomRpg == NULL) {
        return;
    }

    if (!Esp32IntroDispose_isDone()) {
        return;
    }

    if (!map1State.armed) {
        map1State.armed = 1;
        printf("[MAP1STRUCT] ARMED post-intro boundary; MAP_INTRO structural load starts on next loop service\n");
        return;
    }

    map1State.attempted = 1;
    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    printf("\n=== Doom RPG ESP32 first gameplay BSP structural load ===\n");

    if (!preBoundaryIsSafe(doomRpg)) {
        printf("[MAP1STRUCT] FAILED precondition state=%d page=%d startupMap=%d menu=%d dispose=%d clock=%d input=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p entities=%d monsters=%d\n",
               canvas != NULL ? canvas->state : -1,
               canvas != NULL ? canvas->storyPage : -1,
               canvas != NULL ? canvas->startupMap : -1,
               doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -1,
               Esp32IntroDispose_isDone(),
               Esp32IntroClock_isActive(),
               Esp32IntroInput_isActive(),
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL,
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1);
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    mapFile = doomRpg->game->mapFiles[canvas->startupMap - 1];
    mappingsEntry = findZipEntryForPath("/mappings.bin");
    mapEntry = findZipEntryForPath(mapFile);

    printf("[MAP1STRUCT] BEGIN state=%d page=%d mapId=%d enum=MAP_INTRO file=%s heap8=%u largest8=%u\n",
           canvas->state,
           canvas->storyPage,
           canvas->startupMap,
           mapFile != NULL ? mapFile : "<null>",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore);
    printf("[MAP1STRUCT] CONTRACT real Render_beginLoadMap + structural Render_beginLoadMapData only; bitshapes/texels/entities/finalize forbidden\n");

    if (mapFile == NULL || mapFile[0] == '\0' || mapEntry == NULL ||
        (uint32_t)mapEntry->usize < MAP_HEADER_BYTES ||
        !zipInflatePlanFits("mappings.bin", mappingsEntry,
                            heapBefore, largestBefore) ||
        !zipInflatePlanFits(mapFile, mapEntry, heapBefore, largestBefore)) {
        return;
    }

    printf("[MAP1STRUCT] PREFLIGHT -> Render_loadMappings() first, then inspect BSP with mappings resident\n");
    if (!Render_loadMappings(render)) {
        printf("[MAP1STRUCT] FAILED Render_loadMappings returned false\n");
        cleanupFailedLoad(render);
        return;
    }

    heapAfterMappings = heap8Free();
    largestAfterMappings = largest8Block();
    printf("[MAP1STRUCT] MAPPINGS READY mappingMemory=%d heap8=%u largest8=%u offsets=%p/%p ids=%p/%p\n",
           render->mappingMemory,
           (unsigned int)heapAfterMappings,
           (unsigned int)largestAfterMappings,
           (void*)render->mediaTexelOffsets,
           (void*)render->mediaBitShapeOffsets,
           (void*)render->mediaTexturesIds,
           (void*)render->mediaSpriteIds);

    if (!zipInflatePlanFits(mapFile, mapEntry,
                            heapAfterMappings, largestAfterMappings)) {
        cleanupFailedLoad(render);
        return;
    }

    planData = DoomRPG_fileOpenRead(doomRpg, mapFile);
    if (planData == NULL) {
        printf("[MAP1STRUCT] FAILED preflight BSP read returned NULL\n");
        cleanupFailedLoad(render);
        return;
    }

    heapWithPlanData = heap8Free();
    largestWithPlanData = largest8Block();

    if (!parseStructuralPlan(planData, (uint32_t)mapEntry->usize, &plan)) {
        printf("[MAP1STRUCT] FAILED unable to parse structural memory plan from %s\n",
               mapFile);
        SDL_free(planData);
        cleanupFailedLoad(render);
        return;
    }

    printf("[MAP1STRUCT] PLAN nodes=%u lines=%u mapSprites=%u runtimeSprites=%u events=%u byteCodes=%u strings=%u stringBytes=%u parsed=%u/%u trailing=%u\n",
           (unsigned int)plan.nodes,
           (unsigned int)plan.lines,
           (unsigned int)plan.mapSprites,
           (unsigned int)plan.runtimeSprites,
           (unsigned int)plan.events,
           (unsigned int)plan.byteCodes,
           (unsigned int)plan.strings,
           (unsigned int)plan.stringBytes,
           (unsigned int)plan.parsedBytes,
           (unsigned int)mapEntry->usize,
           (unsigned int)plan.trailingBytes);
    printf("[MAP1STRUCT] PLAN allocPayload=%llu largestAlloc=%llu rawResidentHeap8=%u rawResidentLargest8=%u safetyHeadroom=%u\n",
           (unsigned long long)plan.structuralBytes,
           (unsigned long long)plan.largestAllocation,
           (unsigned int)heapWithPlanData,
           (unsigned int)largestWithPlanData,
           (unsigned int)MAP_STRUCTURAL_SAFETY_HEADROOM);

    requiredWithRaw = plan.structuralBytes + MAP_STRUCTURAL_SAFETY_HEADROOM;
    if (requiredWithRaw >= heapWithPlanData ||
        plan.largestAllocation > largestWithPlanData) {
        printf("[MAP1STRUCT] REFUSED structural working set does not fit with raw BSP resident need=%llu heap8=%u largestNeed=%llu largest8=%u\n",
               (unsigned long long)requiredWithRaw,
               (unsigned int)heapWithPlanData,
               (unsigned long long)plan.largestAllocation,
               (unsigned int)largestWithPlanData);
        SDL_free(planData);
        cleanupFailedLoad(render);
        return;
    }

    SDL_free(planData);
    planData = NULL;
    heapAfterPlan = heap8Free();
    printf("[MAP1STRUCT] PREFLIGHT PASS released plan BSP heap8=%u largest8=%u\n",
           (unsigned int)heapAfterPlan,
           (unsigned int)largest8Block());

    canvas->loadMapID = canvas->startupMap;
    printf("[MAP1STRUCT] -> Render_beginLoadMap(map=%d) real header/mappings path\n",
           canvas->loadMapID);
    beginResult = Render_beginLoadMap(render, canvas->loadMapID);
    heapAfterBegin = heap8Free();
    largestAfterBegin = largest8Block();

    printf("[MAP1STRUCT] HEADER result=%d mapName='%.16s' mapNameID=%d loadMapID=%d spawn=%d dir=%d cameraSpawn=%d ioPos=%d ioBuffer=%p heap8=%u largest8=%u\n",
           (int)beginResult,
           render->mapName,
           render->mapNameID,
           render->loadMapID,
           render->mapSpawnIndex,
           render->mapSpawnDir,
           render->mapCameraSpawnIndex,
           render->ioBufferPos,
           (void*)render->ioBuffer,
           (unsigned int)heapAfterBegin,
           (unsigned int)largestAfterBegin);

    if (!beginResult || render->ioBuffer == NULL ||
        render->ioBufferPos != (int)MAP_HEADER_BYTES ||
        render->mapNameID != MAP_INTRO ||
        render->shapeData != NULL || render->mediaTexels != NULL) {
        printf("[MAP1STRUCT] FAILED real map header boundary\n");
        cleanupFailedLoad(render);
        return;
    }

    map1State.active = 1;
    map1State.boundaryCaptured = 0;
    printf("[MAP1STRUCT] -> Render_beginLoadMapData(); generated ESP32 hook will stop after structural parse\n");
    dataResult = Render_beginLoadMapData(render);
    map1State.active = 0;

    if (!map1State.boundaryCaptured) {
        printf("[MAP1STRUCT] FAILED structural hook was not reached result=%d ioBuffer=%p shapeData=%p mediaTexels=%p\n",
               (int)dataResult,
               (void*)render->ioBuffer,
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        cleanupFailedLoad(render);
        return;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (dataResult || !structuralBoundaryIsSafe(doomRpg)) {
        printf("[MAP1STRUCT] FAILED postcondition result=%d heap8=%u largest8=%u ioBuffer=%p shapeData=%p mediaTexels=%p entities=%d monsters=%d\n",
               (int)dataResult,
               (unsigned int)heapAfter,
               (unsigned int)largestAfter,
               (void*)render->ioBuffer,
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               doomRpg->game->numEntities,
               doomRpg->game->numMonsters);
        cleanupFailedLoad(render);
        return;
    }

    map1State.done = 1;
    printf("[MAP1STRUCT] READY map=%d file=%s name='%.16s' nodes=%d lines=%d mapSprites=%d runtimeSprites=%d events=%d strings=%d refs=%d/%d planes=%d\n",
           canvas->loadMapID,
           mapFile,
           render->mapName,
           render->nodesLength,
           render->linesLength,
           render->numMapSprites,
           render->numSprites,
           render->numTileEvents,
           render->mapStringCount,
           render->mapTextureTexelsCount,
           render->mapSpriteTexelsCount,
           render->planeTexturesCnt);
    printf("[MAP1STRUCT] RAM heap8=%u->%u used=%d largest8=%u->%u boundaryHeap8=%u boundaryLargest8=%u\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (int)heapBefore - (int)heapAfter,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)map1State.boundaryHeap8,
           (unsigned int)map1State.boundaryLargest8);
    printf("[MAP1STRUCT] PARK state=%d page=%d mapId=%d entities=%d monsters=%d shapeData=%p mediaTexels=%p noBitShapes=yes noTexels=yes noGameEntities=yes noFinalize=yes\n",
           canvas->state,
           canvas->storyPage,
           canvas->loadMapID,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
}

int Esp32Map1StructuralLoad_isDone(void) {
    return map1State.done;
}
