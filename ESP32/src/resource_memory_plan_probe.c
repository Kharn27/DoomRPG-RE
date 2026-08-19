#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"
#include "Z_Zip.h"

#include "resource_memory_plan_probe.h"

/* Keep ESP-IDF headers after DoomRPG.h: stdbool false/true macros collide
 * with the engine's legacy boolean enum. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define ESP32_TINFL_STATE_BYTES 10992U
#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_OFFSET_ENTRY_COUNT 1300U
#define WALL_TEXEL_PACKED_BYTES ((64U * 64U) / 2U)

static int resourcePlanAttempted = 0;
static int resourcePlanReady = 0;

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

static uint64_t inflateTransientBytes(const zip_entry_t* entry) {
    if (entry == NULL || entry->csize <= 0 || entry->usize <= 0) {
        return 0;
    }

    return (uint64_t)(uint32_t)entry->csize +
           (uint64_t)(uint32_t)entry->usize +
           (uint64_t)ESP32_TINFL_STATE_BYTES;
}

static int wholeFileInflateFits(const zip_entry_t* entry,
                                uint32_t heapFree,
                                uint32_t largestBlock) {
    uint64_t transient;

    if (entry == NULL || entry->csize <= 0 || entry->usize <= 0) {
        return 0;
    }

    transient = inflateTransientBytes(entry);
    if ((uint32_t)entry->csize > largestBlock ||
        (uint32_t)entry->usize > largestBlock ||
        ESP32_TINFL_STATE_BYTES > largestBlock ||
        transient > heapFree) {
        return 0;
    }

    return 1;
}

static int readBitShapeMetrics(const byte* payload,
                               uint32_t payloadSize,
                               int offset,
                               uint32_t* shapeWords,
                               uint32_t* packedSpriteBytes) {
    uint32_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t maskBytes;
    uint32_t pixels;
    uint32_t x;
    uint32_t y;

    if (payload == NULL || shapeWords == NULL || packedSpriteBytes == NULL ||
        offset < 0) {
        return 0;
    }

    base = (uint32_t)offset;
    if (base > payloadSize || payloadSize - base < 12U) {
        return 0;
    }

    *shapeWords = (uint32_t)payload[base + 6U] |
                  ((uint32_t)payload[base + 7U] << 8);

    if (payload[base + 9U] < payload[base + 8U] ||
        payload[base + 11U] < payload[base + 10U]) {
        return 0;
    }

    width = (uint32_t)payload[base + 9U] -
            (uint32_t)payload[base + 8U] + 1U;
    height = (uint32_t)payload[base + 11U] -
             (uint32_t)payload[base + 10U] + 1U;
    pitch = (height + 7U) / 8U;
    maskBytes = width * pitch;

    if (base + 12U > payloadSize || maskBytes > payloadSize - (base + 12U)) {
        return 0;
    }

    /* Render_getSTexelBufferSize() ultimately sums the number of set pixels in
     * every bitshape column, then rounds that pixel count up to an even value
     * because sprite texels are packed 4 bpp (two pixels per byte). */
    pixels = 0;
    for (x = 0; x < width; ++x) {
        const byte* column = payload + base + 12U + (x * pitch);
        for (y = 0; y < height; ++y) {
            if ((column[y >> 3] & (1U << (y & 7U))) != 0) {
                ++pixels;
            }
        }
    }

    pixels = (pixels + 1U) & ~1U;
    *packedSpriteBytes = pixels / 2U;
    return 1;
}

int DoomRPG_probeMenuResourceMemoryPlan(int mapStructuresReady) {
    Render_t* render;
    const zip_entry_t* bitshapesEntry;
    const zip_entry_t* wtexelsEntry;
    const zip_entry_t* stexelsEntry;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint64_t wallTexelBytes;
    uint64_t texelScratchBytes;
    uint64_t mediaTexelsLowerBound;
    byte* bitshapeFile;
    const byte* payload;
    uint32_t payloadSize;
    uint32_t heapResident;
    uint32_t largestResident;
    uint64_t shapeWordsTotal;
    uint64_t shapeDataBytes;
    uint64_t spriteTexelBytes;
    uint64_t mediaTexelsExact;
    uint32_t uniqueShapes;
    int exactBitshapePlan;
    int i;

    if (resourcePlanAttempted) {
        return resourcePlanReady;
    }
    resourcePlanAttempted = 1;

    printf("\n=== Doom RPG menu graphics resource memory plan ===\n");

    if (!mapStructuresReady) {
        printf("[RESOURCEPLAN] Real map structures are not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[RESOURCEPLAN] Render object unavailable; probe refused\n");
        return 0;
    }

    render = doomRpg->render;
    if (render->mapTextureTexels == NULL || render->mapSpriteTexels == NULL ||
        render->mapTextureTexelsCount <= 0 || render->mapSpriteTexelsCount <= 0 ||
        render->mediaBitShapeOffsets == NULL) {
        printf("[RESOURCEPLAN] Runtime resource-reference lists are incomplete\n");
        return 0;
    }

    bitshapesEntry = findZipEntry("bitshapes.bin");
    wtexelsEntry = findZipEntry("wtexels.bin");
    stexelsEntry = findZipEntry("stexels.bin");
    if (bitshapesEntry == NULL || wtexelsEntry == NULL || stexelsEntry == NULL) {
        printf("[RESOURCEPLAN] Missing bitshapes.bin / wtexels.bin / stexels.bin\n");
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[RESOURCEPLAN] Begin heap8=%u largest8=%u refs textures=%d sprites=%d planes=%d\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           render->mapTextureTexelsCount,
           render->mapSpriteTexelsCount,
           render->planeTexturesCnt);
    printf("[RESOURCEPLAN] bitshapes.bin c=%d u=%d inflateTransient=%lluB\n",
           bitshapesEntry->csize, bitshapesEntry->usize,
           (unsigned long long)inflateTransientBytes(bitshapesEntry));
    printf("[RESOURCEPLAN] wtexels.bin   c=%d u=%d inflateTransient=%lluB\n",
           wtexelsEntry->csize, wtexelsEntry->usize,
           (unsigned long long)inflateTransientBytes(wtexelsEntry));
    printf("[RESOURCEPLAN] stexels.bin   c=%d u=%d inflateTransient=%lluB\n",
           stexelsEntry->csize, stexelsEntry->usize,
           (unsigned long long)inflateTransientBytes(stexelsEntry));

    wallTexelBytes = (uint64_t)(uint32_t)render->mapTextureTexelsCount *
                     (uint64_t)WALL_TEXEL_PACKED_BYTES;
    texelScratchBytes = (uint64_t)(uint32_t)render->mapSpriteTexelsCount *
                        (uint64_t)sizeof(int);
    mediaTexelsLowerBound = wallTexelBytes;

    printf("[RESOURCEPLAN] Render_loadTexels wall payload=%lluB (%d x %uB)\n",
           (unsigned long long)wallTexelBytes,
           render->mapTextureTexelsCount,
           (unsigned int)WALL_TEXEL_PACKED_BYTES);
    printf("[RESOURCEPLAN] Render_loadTexels sprite-size scratch=%lluB\n",
           (unsigned long long)texelScratchBytes);
    printf("[RESOURCEPLAN] mediaTexels lowerBound=%lluB before ANY sprite texels\n",
           (unsigned long long)mediaTexelsLowerBound);

    if (mediaTexelsLowerBound > largestBefore) {
        printf("[RESOURCEPLAN] TEXEL WALL proven: lowerBound exceeds largest8 by %lluB\n",
               (unsigned long long)(mediaTexelsLowerBound - largestBefore));
    }
    else {
        printf("[RESOURCEPLAN] WARNING unexpected: wall-only mediaTexels lower bound currently fits\n");
    }

    exactBitshapePlan = 0;
    shapeWordsTotal = 0;
    shapeDataBytes = 0;
    spriteTexelBytes = 0;
    mediaTexelsExact = 0;
    uniqueShapes = 0;
    bitshapeFile = NULL;

    if (!wholeFileInflateFits(bitshapesEntry, heapBefore, largestBefore)) {
        printf("[RESOURCEPLAN] BITSHAPE PREFLIGHT blocked: current whole-file loader cannot safely inflate bitshapes.bin\n");
        printf("[RESOURCEPLAN] Exact shapeData/sprite-texel contribution intentionally not inspected\n");
    }
    else if (bitshapesEntry->usize <= (int)BITSHAPE_FILE_HEADER_BYTES) {
        printf("[RESOURCEPLAN] ERROR invalid bitshapes.bin size\n");
        return 0;
    }
    else {
        printf("[RESOURCEPLAN] -> temporary DoomRPG_fileOpenRead(/bitshapes.bin) for exact plan\n");
        bitshapeFile = DoomRPG_fileOpenRead(doomRpg, "/bitshapes.bin");
        if (bitshapeFile == NULL) {
            printf("[RESOURCEPLAN] ERROR bitshapes loader returned NULL\n");
            return 0;
        }

        heapResident = heap8Free();
        largestResident = largest8Block();
        payload = bitshapeFile + BITSHAPE_FILE_HEADER_BYTES;
        payloadSize = (uint32_t)bitshapesEntry->usize - BITSHAPE_FILE_HEADER_BYTES;

        printf("[RESOURCEPLAN] bitshapes resident heap8=%u largest8=%u used=%u\n",
               (unsigned int)heapResident,
               (unsigned int)largestResident,
               (unsigned int)(heapBefore >= heapResident ? heapBefore - heapResident : 0));

        for (i = 0; i < render->mapSpriteTexelsCount; ++i) {
            int spriteIndex = render->mapSpriteTexels[i];
            int offset;
            uint32_t shapeWords;
            uint32_t packedBytes;
            int seenBefore;
            int j;

            if (spriteIndex < 0 ||
                (uint32_t)spriteIndex >= (BITSHAPE_OFFSET_ENTRY_COUNT / 2U)) {
                printf("[RESOURCEPLAN] ERROR sprite texel index=%d exceeds mapping table\n",
                       spriteIndex);
                SDL_free(bitshapeFile);
                return 0;
            }

            offset = render->mediaBitShapeOffsets[spriteIndex * 2];
            if (!readBitShapeMetrics(payload, payloadSize, offset,
                                     &shapeWords, &packedBytes)) {
                printf("[RESOURCEPLAN] ERROR invalid bitshape spriteIndex=%d offset=%d\n",
                       spriteIndex, offset);
                SDL_free(bitshapeFile);
                return 0;
            }

            /* Render_loadTexels() adds every referenced sprite's packed texel
             * size to i7, even when multiple sprite indexes share a bitshape. */
            spriteTexelBytes += packedBytes;

            seenBefore = 0;
            for (j = 0; j < i; ++j) {
                int previousIndex = render->mapSpriteTexels[j];
                if (previousIndex >= 0 &&
                    (uint32_t)previousIndex < (BITSHAPE_OFFSET_ENTRY_COUNT / 2U) &&
                    render->mediaBitShapeOffsets[previousIndex * 2] == offset) {
                    seenBefore = 1;
                    break;
                }
            }

            if (!seenBefore) {
                shapeWordsTotal += shapeWords;
                ++uniqueShapes;
            }
        }

        shapeDataBytes = shapeWordsTotal * (uint64_t)sizeof(short);
        mediaTexelsExact = wallTexelBytes + spriteTexelBytes;
        exactBitshapePlan = 1;

        printf("[RESOURCEPLAN] Bitshapes unique=%u shapeWords=%llu shapeData=%lluB\n",
               (unsigned int)uniqueShapes,
               (unsigned long long)shapeWordsTotal,
               (unsigned long long)shapeDataBytes);
        printf("[RESOURCEPLAN] Sprite packed texels=%lluB across %d refs\n",
               (unsigned long long)spriteTexelBytes,
               render->mapSpriteTexelsCount);
        printf("[RESOURCEPLAN] Exact mediaTexels allocation=%lluB wall=%lluB sprite=%lluB\n",
               (unsigned long long)mediaTexelsExact,
               (unsigned long long)wallTexelBytes,
               (unsigned long long)spriteTexelBytes);
        printf("[RESOURCEPLAN] shapeData while bitshapes resident fit aggregate=%s contiguous=%s\n",
               shapeDataBytes <= heapResident ? "yes" : "NO",
               shapeDataBytes <= largestResident ? "yes" : "NO");

        SDL_free(bitshapeFile);
        bitshapeFile = NULL;

        printf("[RESOURCEPLAN] Released bitshapes heap8=%u largest8=%u deltaFromStart=%d\n",
               (unsigned int)heap8Free(),
               (unsigned int)largest8Block(),
               (int)heapBefore - (int)heap8Free());
    }

    if (exactBitshapePlan) {
        printf("[RESOURCEPLAN] Current texel allocation fit aggregate=%s contiguous=%s\n",
               mediaTexelsExact + texelScratchBytes <= heapBefore ? "yes" : "NO",
               mediaTexelsExact <= largestBefore ? "yes" : "NO");
    }
    else {
        printf("[RESOURCEPLAN] Current texel allocation fit aggregate=NO contiguous=NO (wall lower bound alone is sufficient)\n");
    }

    printf("[RESOURCEPLAN] NOTE original Render_loadTexels() allocates %lluB sprite-size scratch and does not free it\n",
           (unsigned long long)texelScratchBytes);
    printf("[RESOURCEPLAN] READY resource budget measured; heavy graphics loaders remain blocked\n");
    printf("[RESOURCEPLAN] Render_loadBitShapes / Render_loadTexels still NOT executed\n");

    resourcePlanReady = 1;
    return 1;
}
