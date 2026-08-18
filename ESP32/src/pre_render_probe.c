#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "EntityDef.h"
#include "MenuSystem.h"
#include "ParticleSystem.h"
#include "Z_Zip.h"
#include "pre_render_probe.h"

/* Keep ESP-IDF's C99 bool macros after DoomRPG's legacy boolean typedefs. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

static int preRenderAttempted = 0;
static int preRenderReady = 0;

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

static int preflightResources(void) {
    static const char* const required[] = {
        "gibs_24.bmp",
        "p.bmp",
        "q.bmp",
        "j.bmp",
        "entities.db",
    };
    const unsigned int count = sizeof(required) / sizeof(required[0]);
    unsigned int i;
    int allPresent = 1;

    printf("[PRERENDER] Resource preflight (%u files)\n", count);
    for (i = 0; i < count; ++i) {
        const zip_entry_t* entry = findZipEntry(required[i]);
        if (entry == NULL) {
            printf("[PRERENDER] MISSING %s\n", required[i]);
            allPresent = 0;
            continue;
        }

        printf("[PRERENDER] %-14s method=%d c=%d u=%d\n",
               required[i], entry->method, entry->csize, entry->usize);
    }

    if (!allPresent) {
        printf("[PRERENDER] Resource preflight FAILED; startup skipped safely\n");
        return 0;
    }

    printf("[PRERENDER] Resource preflight OK\n");
    return 1;
}

static void printStageResult(const char* name, uint32_t before, uint32_t after) {
    const uint32_t used = before >= after ? before - after : 0;
    printf("[PRERENDER] %-24s used=%u heap8=%u largest8=%u\n",
           name,
           (unsigned int)used,
           (unsigned int)after,
           (unsigned int)largest8Block());
}

int DoomRPG_probePreRenderStartup(int layoutReady) {
    uint32_t heapBefore;
    uint32_t before;
    uint32_t after;
    uint32_t largestBefore;
    int entityResult;

    if (preRenderAttempted) {
        return preRenderReady;
    }
    preRenderAttempted = 1;

    printf("\n=== Doom RPG pre-render startup probe ===\n");

    if (!layoutReady) {
        printf("[PRERENDER] Layout is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->particleSystem == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->entityDef == NULL) {
        printf("[PRERENDER] Core object graph incomplete; probe refused\n");
        return 0;
    }

    if (!preflightResources()) {
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[PRERENDER] Begin: heap8=%u largest8=%u\n",
           (unsigned int)heapBefore, (unsigned int)largestBefore);

    before = heap8Free();
    printf("[PRERENDER] -> ParticleSystem_startup()\n");
    ParticleSystem_startup(doomRpg->particleSystem);
    after = heap8Free();
    printStageResult("ParticleSystem_startup", before, after);

    before = heap8Free();
    printf("[PRERENDER] -> MenuSystem_startup()\n");
    MenuSystem_startup(doomRpg->menuSystem);
    after = heap8Free();
    printStageResult("MenuSystem_startup", before, after);

    before = heap8Free();
    printf("[PRERENDER] -> EntityDef_startup()\n");
    entityResult = EntityDef_startup(doomRpg->entityDef);
    after = heap8Free();
    printStageResult("EntityDef_startup", before, after);

    if (!entityResult || doomRpg->entityDef->list == NULL ||
        doomRpg->entityDef->numDefs <= 0) {
        printf("[PRERENDER] FAILED EntityDef_startup result=%d defs=%d list=%p\n",
               entityResult,
               doomRpg->entityDef->numDefs,
               (void*)doomRpg->entityDef->list);
        return 0;
    }

    printf("[PRERENDER] Entity defs=%d table=%uB\n",
           doomRpg->entityDef->numDefs,
           (unsigned int)(doomRpg->entityDef->numDefs * sizeof(EntityDef_t)));

    preRenderReady = 1;
    printf("[PRERENDER] READY total used=%u heap8=%u largest8=%u\n",
           (unsigned int)(heapBefore >= heap8Free() ? heapBefore - heap8Free() : 0),
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());
    printf("[PRERENDER] Render_startup / Game_loadConfig still NOT executed\n");

    return 1;
}
