#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"
#include "Z_Zip.h"

#include "menu_bsp_probe.h"
#include "menu_bsp_structure_probe.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define MENU_BSP_HEADER_BYTES 33U
#define ESP32_TINFL_STATE_BYTES 10992U

static int menuBspAttempted = 0;
static int menuBspReady = 0;

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

int DoomRPG_probeMenuBspHeader(int configMappingsReady) {
    const zip_entry_t* entry;
    byte* data;
    int dataPos;
    char mapName[MAPNAMESTRLEN + 1];
    int floorR, floorG, floorB;
    int ceilR, ceilG, ceilB;
    int introR, introG, introB;
    int floorTex, ceilingTex;
    int loadMapID;
    int mapSpawnIndex;
    int mapSpawnDir;
    int mapCameraSpawnIndex;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapResident;
    uint32_t largestResident;
    uint32_t heapAfter;
    uint64_t transientPayload;

    if (menuBspAttempted) {
        return menuBspReady;
    }
    menuBspAttempted = 1;

    printf("\n=== Doom RPG menu BSP header probe ===\n");

    if (!configMappingsReady) {
        printf("[MENUBSP] Config/mappings startup is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[MENUBSP] Core object graph incomplete; probe refused\n");
        return 0;
    }

    entry = findZipEntry("menu.bsp");
    if (entry == NULL) {
        printf("[MENUBSP] MISSING menu.bsp; probe skipped safely\n");
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    transientPayload = (uint64_t)(uint32_t)entry->csize +
                       (uint64_t)(uint32_t)entry->usize +
                       (uint64_t)ESP32_TINFL_STATE_BYTES;

    printf("[MENUBSP] menu.bsp c=%d u=%d header=%uB\n",
           entry->csize, entry->usize, (unsigned int)MENU_BSP_HEADER_BYTES);
    printf("[MENUBSP] Begin heap8=%u largest8=%u transientPayload=%lluB\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned long long)transientPayload);

    if (entry->csize <= 0 || entry->usize < (int)MENU_BSP_HEADER_BYTES) {
        printf("[MENUBSP] REFUSED invalid ZIP sizes\n");
        return 0;
    }

    if ((uint32_t)entry->usize > largestBefore) {
        printf("[MENUBSP] REFUSED BSP needs contiguous %uB but largest8=%u\n",
               (unsigned int)entry->usize,
               (unsigned int)largestBefore);
        return 0;
    }

    if ((uint32_t)entry->csize > largestBefore ||
        ESP32_TINFL_STATE_BYTES > largestBefore ||
        transientPayload > heapBefore) {
        printf("[MENUBSP] REFUSED inflate transient set does not fit current heap\n");
        return 0;
    }

    printf("[MENUBSP] -> DoomRPG_fileOpenRead(/menu.bsp)\n");
    data = DoomRPG_fileOpenRead(doomRpg, "/menu.bsp");
    if (data == NULL) {
        printf("[MENUBSP] ERROR loader returned NULL\n");
        return 0;
    }

    heapResident = heap8Free();
    largestResident = largest8Block();
    printf("[MENUBSP] BSP resident ptr=%p heap8=%u largest8=%u used=%u\n",
           (void*)data,
           (unsigned int)heapResident,
           (unsigned int)largestResident,
           (unsigned int)(heapBefore >= heapResident ? heapBefore - heapResident : 0));

    dataPos = 0;
    memcpy(mapName, data + dataPos, MAPNAMESTRLEN);
    mapName[MAPNAMESTRLEN] = '\0';
    dataPos += MAPNAMESTRLEN;

    floorR = DoomRPG_byteAtNext(data, &dataPos);
    floorG = DoomRPG_byteAtNext(data, &dataPos);
    floorB = DoomRPG_byteAtNext(data, &dataPos);

    ceilR = DoomRPG_byteAtNext(data, &dataPos);
    ceilG = DoomRPG_byteAtNext(data, &dataPos);
    ceilB = DoomRPG_byteAtNext(data, &dataPos);

    floorTex = DoomRPG_byteAtNext(data, &dataPos);
    ceilingTex = DoomRPG_byteAtNext(data, &dataPos);

    introR = DoomRPG_byteAtNext(data, &dataPos);
    introG = DoomRPG_byteAtNext(data, &dataPos);
    introB = DoomRPG_byteAtNext(data, &dataPos);

    loadMapID = DoomRPG_byteAtNext(data, &dataPos);
    mapSpawnIndex = DoomRPG_shortAtNext(data, &dataPos);
    mapSpawnDir = DoomRPG_byteAtNext(data, &dataPos);
    mapCameraSpawnIndex = DoomRPG_shortAtNext(data, &dataPos);

    printf("[MENUBSP] Header name='%s' floorRGB=%d,%d,%d ceilRGB=%d,%d,%d\n",
           mapName,
           floorR, floorG, floorB,
           ceilR, ceilG, ceilB);
    printf("[MENUBSP] Header floorTex=%d ceilingTex=%d introRGB=%d,%d,%d\n",
           floorTex, ceilingTex, introR, introG, introB);
    printf("[MENUBSP] Header loadMapID=%d spawn=%d dir=%d cameraSpawn=%d pos=%d/%u\n",
           loadMapID, mapSpawnIndex, mapSpawnDir, mapCameraSpawnIndex,
           dataPos, (unsigned int)MENU_BSP_HEADER_BYTES);

    if ((uint32_t)dataPos != MENU_BSP_HEADER_BYTES) {
        printf("[MENUBSP] FAILED unexpected header position=%d\n", dataPos);
        SDL_free(data);
        return 0;
    }

    SDL_free(data);
    heapAfter = heap8Free();

    printf("[MENUBSP] Released BSP heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largest8Block(),
           (int)heapBefore - (int)heapAfter);

    if (!DoomRPG_probeMenuBspStructure(1)) {
        printf("[MENUBSP] FAILED complete structure plan\n");
        return 0;
    }

    menuBspReady = 1;
    printf("[MENUBSP] READY menu.bsp header + complete structure plan validated\n");
    printf("[MENUBSP] Render_beginLoadMap / Render_beginLoadMapData still NOT executed\n");

    return 1;
}
