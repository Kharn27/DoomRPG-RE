#include <SDL.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Game.h"
#include "Render.h"
#include "SDL_Video.h"
#include "Z_Zip.h"

#include "config_mappings_probe.h"

extern DoomRPG_t* doomRpg;

typedef struct MappingPlan_s {
    int texelsCnt;
    int bitShapeCnt;
    int textureCnt;
    int spriteCnt;
    uint32_t texelOffsetBytes;
    uint32_t bitShapeOffsetBytes;
    uint32_t textureIdBytes;
    uint32_t spriteIdBytes;
    uint32_t persistentBytes;
    uint32_t largestAllocation;
    uint32_t heapWithData;
    uint32_t largestWithData;
} MappingPlan_t;

static int configMappingsAttempted = 0;
static int configMappingsReady = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static const zip_entry_t* findZipEntry(const char* name) {
    int i;

    if (name == NULL || zipFile.entry == NULL) {
        return NULL;
    }

    for (i = 0; i < zipFile.entry_count; ++i) {
        const zip_entry_t* entry = &zipFile.entry[i];
        if (entry->name != NULL && SDL_strcasecmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static uint32_t max4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t result = a;
    if (b > result) result = b;
    if (c > result) result = c;
    if (d > result) result = d;
    return result;
}

static int inspectMappings(const zip_entry_t* entry, MappingPlan_t* plan) {
    byte* fData;
    int dataPos = 0;
    int texelPairs;
    int bitShapePairs;
    uint64_t texelOffsetBytes;
    uint64_t bitShapeOffsetBytes;
    uint64_t textureIdBytes;
    uint64_t spriteIdBytes;
    uint64_t persistentBytes;

    if (entry == NULL || plan == NULL || entry->usize < 16) {
        return 0;
    }

    memset(plan, 0, sizeof(*plan));

    fData = DoomRPG_fileOpenRead(doomRpg, "/mappings.bin");
    if (fData == NULL) {
        printf("[MAPPINGS] ERROR unable to read mappings.bin for header inspection\n");
        return 0;
    }

    texelPairs = DoomRPG_intAtNext(fData, &dataPos);
    bitShapePairs = DoomRPG_intAtNext(fData, &dataPos);
    plan->textureCnt = DoomRPG_intAtNext(fData, &dataPos);
    plan->spriteCnt = DoomRPG_intAtNext(fData, &dataPos);

    plan->heapWithData = heap8Free();
    plan->largestWithData = largest8Block();

    if (texelPairs < 0 || bitShapePairs < 0 ||
        plan->textureCnt < 0 || plan->spriteCnt < 0 ||
        texelPairs > 0x3fffffff || bitShapePairs > 0x3fffffff) {
        SDL_free(fData);
        printf("[MAPPINGS] ERROR invalid negative/overflow count in header\n");
        return 0;
    }

    plan->texelsCnt = texelPairs * 2;
    plan->bitShapeCnt = bitShapePairs * 2;

    texelOffsetBytes = (uint64_t)plan->texelsCnt * sizeof(int);
    bitShapeOffsetBytes = (uint64_t)plan->bitShapeCnt * sizeof(int);
    textureIdBytes = (uint64_t)plan->textureCnt * sizeof(short);
    spriteIdBytes = (uint64_t)plan->spriteCnt * sizeof(short);
    persistentBytes = texelOffsetBytes + bitShapeOffsetBytes +
                      textureIdBytes + spriteIdBytes;

    if (texelOffsetBytes > UINT32_MAX || bitShapeOffsetBytes > UINT32_MAX ||
        textureIdBytes > UINT32_MAX || spriteIdBytes > UINT32_MAX ||
        persistentBytes > UINT32_MAX) {
        SDL_free(fData);
        printf("[MAPPINGS] ERROR mapping table sizes overflow 32-bit address space\n");
        return 0;
    }

    plan->texelOffsetBytes = (uint32_t)texelOffsetBytes;
    plan->bitShapeOffsetBytes = (uint32_t)bitShapeOffsetBytes;
    plan->textureIdBytes = (uint32_t)textureIdBytes;
    plan->spriteIdBytes = (uint32_t)spriteIdBytes;
    plan->persistentBytes = (uint32_t)persistentBytes;
    plan->largestAllocation = max4(plan->texelOffsetBytes,
                                   plan->bitShapeOffsetBytes,
                                   plan->textureIdBytes,
                                   plan->spriteIdBytes);

    printf("[MAPPINGS] Header texelOffsets=%d bitShapeOffsets=%d textures=%d sprites=%d\n",
           plan->texelsCnt, plan->bitShapeCnt,
           plan->textureCnt, plan->spriteCnt);
    printf("[MAPPINGS] Plan payload=%uB largestAlloc=%uB whileData heap8=%u largest8=%u\n",
           (unsigned int)plan->persistentBytes,
           (unsigned int)plan->largestAllocation,
           (unsigned int)plan->heapWithData,
           (unsigned int)plan->largestWithData);

    SDL_free(fData);
    return 1;
}

static int configFilePresent(void) {
    SDL_RWops* rw = SDL_RWFromFile("Config", "r");
    if (rw == NULL) {
        return 0;
    }
    SDL_RWclose(rw);
    return 1;
}

int DoomRPG_probeConfigAndMappings(int renderStartupReady) {
    const zip_entry_t* mappingEntry;
    MappingPlan_t plan;
    Render_t* render;
    uint32_t before;
    uint32_t after;
    uint32_t heapBeforeMappings;
    uint32_t heapAfterMappings;
    uint32_t largestBeforeMappings;
    int hasConfig;
    boolean mappingsResult;

    if (configMappingsAttempted) {
        return configMappingsReady;
    }
    configMappingsAttempted = 1;

    printf("\n=== Doom RPG config + mappings startup probe ===\n");

    if (!renderStartupReady) {
        printf("[CONFIGMAP] Render startup is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->game == NULL || doomRpg->render == NULL) {
        printf("[CONFIGMAP] Core object graph incomplete; probe refused\n");
        return 0;
    }

    mappingEntry = findZipEntry("mappings.bin");
    if (mappingEntry == NULL) {
        printf("[MAPPINGS] MISSING mappings.bin; probe skipped safely\n");
        return 0;
    }

    printf("[MAPPINGS] mappings.bin c=%d u=%d\n",
           mappingEntry->csize, mappingEntry->usize);

    hasConfig = configFilePresent();
    printf("[CONFIG] Config file present=%s (missing is valid on first boot)\n",
           hasConfig ? "yes" : "no");

    before = heap8Free();
    printf("[CONFIG] -> Game_loadConfig()\n");
    Game_loadConfig(doomRpg->game);
    after = heap8Free();
    printf("[CONFIG] DONE heap delta=%d heap8=%u largest8=%u\n",
           (int)before - (int)after,
           (unsigned int)after,
           (unsigned int)largest8Block());

    if (!inspectMappings(mappingEntry, &plan)) {
        printf("[MAPPINGS] Header inspection FAILED; Render_loadMappings skipped\n");
        return 0;
    }

    if (plan.persistentBytes > plan.heapWithData ||
        plan.largestAllocation > plan.largestWithData) {
        printf("[MAPPINGS] REFUSED allocation plan does not fit while mappings.bin is resident\n");
        return 0;
    }

    render = doomRpg->render;
    heapBeforeMappings = heap8Free();
    largestBeforeMappings = largest8Block();

    printf("[MAPPINGS] -> Render_loadMappings() heap8=%u largest8=%u\n",
           (unsigned int)heapBeforeMappings,
           (unsigned int)largestBeforeMappings);

    mappingsResult = Render_loadMappings(render);

    heapAfterMappings = heap8Free();
    printf("[MAPPINGS] Render_loadMappings result=%d used=%u heap8=%u largest8=%u\n",
           mappingsResult,
           (unsigned int)(heapBeforeMappings >= heapAfterMappings
                              ? heapBeforeMappings - heapAfterMappings
                              : 0),
           (unsigned int)heapAfterMappings,
           (unsigned int)largest8Block());

    if (!mappingsResult ||
        render->mediaTexelOffsets == NULL ||
        render->mediaBitShapeOffsets == NULL ||
        render->mediaTexturesIds == NULL ||
        render->mediaSpriteIds == NULL ||
        render->textureCnt != plan.textureCnt ||
        render->spriteCnt != plan.spriteCnt) {
        printf("[MAPPINGS] FAILED texel=%p bitShape=%p texIds=%p spriteIds=%p textures=%d/%d sprites=%d/%d\n",
               (void*)render->mediaTexelOffsets,
               (void*)render->mediaBitShapeOffsets,
               (void*)render->mediaTexturesIds,
               (void*)render->mediaSpriteIds,
               render->textureCnt, plan.textureCnt,
               render->spriteCnt, plan.spriteCnt);
        return 0;
    }

    configMappingsReady = 1;
    printf("[CONFIGMAP] READY config path exercised and mappings resident\n");
    printf("[CONFIGMAP] mappingPayload=%u heap8=%u largest8=%u\n",
           (unsigned int)plan.persistentBytes,
           (unsigned int)heapAfterMappings,
           (unsigned int)largest8Block());
    printf("[CONFIGMAP] Render_beginLoadMap / BSP still NOT executed\n");

    return 1;
}
