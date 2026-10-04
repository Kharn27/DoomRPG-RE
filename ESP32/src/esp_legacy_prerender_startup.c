#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "esp_legacy_asset_source.h"
#include "esp_native_menu_storage.h"
#ifdef DOOMRPG_ESP32_DIVIDER_PROBE
#include "esp_native_text_format.h"
#endif
#ifdef DOOMRPG_ESP32_STORY_TEARDOWN_PROBE
#include "DoomCanvas.h"
#include "native_story_fit.h"
#endif
#include "esp_legacy_prerender_startup.h"

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

static int preflightResources(void) {
    static const char* const required[] = {
        "j.bmp",
        "p.bmp",
    };
    const unsigned int count = sizeof(required) / sizeof(required[0]);
    unsigned int i;
    int allPresent = 1;

    printf("[PRERENDER] Resource preflight (%u files)\n", count);
    for (i = 0; i < count; ++i) {
        uint32_t bytes = 0U;
        if (!EspLegacyAssetSource_stat(required[i], &bytes)) {
            printf("[PRERENDER] MISSING %s\n", required[i]);
            allPresent = 0;
            continue;
        }

        printf("[PRERENDER] %-14s bytes=%u backing=pak\n",
               required[i], (unsigned int)bytes);
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

int EspLegacyPrerenderStartup_start(int layoutReady) {
    uint32_t heapBefore;
    uint32_t before;
    uint32_t after;
    uint32_t largestBefore;
    int menuResult;

    if (preRenderAttempted) {
        return preRenderReady;
    }
    preRenderAttempted = 1;

    printf("\n=== Doom RPG pre-render startup probe ===\n");

#ifdef DOOMRPG_ESP32_DIVIDER_PROBE
    {
        static const struct {
            const char* input;
            const unsigned char* expected;
            unsigned int expectedBytes;
        } cases[] = {
            { "Level up!",   (const unsigned char*)"\x80\x80 Level up! \x80\x80", 16U },
            { "Near Death!", (const unsigned char*)"\x80 Near Death! \x80", 16U },
            { "Low Health!", (const unsigned char*)"\x80 Low Health! \x80", 16U },
            { "Armor Gone!", (const unsigned char*)"\x80 Armor Gone! \x80", 16U },
        };
        unsigned int i;
        unsigned char guarded[40];
        char* out = (char*)guarded;
        uint32_t before = heap8Free();

        for (i = 0; i < (unsigned int)(sizeof(cases) / sizeof(cases[0])); ++i) {
            unsigned int g;
            SDL_memset(guarded, 0x5a, sizeof(guarded));
            EspNativeText_buildDivider(out, cases[i].input);
            if (SDL_memcmp(out, cases[i].expected, cases[i].expectedBytes) != 0) {
                printf("[DIVIDERPROBE] FAILED bytes case=%u text=\"%s\"\n",
                       i, cases[i].input);
                return 0;
            }
            for (g = 32U; g < (unsigned int)sizeof(guarded); ++g) {
                if (guarded[g] != 0x5aU) {
                    printf("[DIVIDERPROBE] FAILED guard case=%u offset=%u value=%02x\n",
                           i, g, guarded[g]);
                    return 0;
                }
            }
        }

        if (heap8Free() != before) {
            printf("[DIVIDERPROBE] FAILED heap8=%u->%u\n",
                   (unsigned int)before, (unsigned int)heap8Free());
            return 0;
        }

        printf("[DIVIDERPROBE] PASS cases=4 heap8=%u exact=yes allocation=no owner=caller\n",
               (unsigned int)heap8Free());
    }
#endif

    if (!layoutReady) {
        printf("[PRERENDER] Layout is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) {
        printf("[PRERENDER] Core object graph incomplete; probe refused\n");
        return 0;
    }

    if (!preflightResources()) {
        return 0;
    }

#ifdef DOOMRPG_ESP32_STORY_TEARDOWN_PROBE
    {
        DoomCanvas_t syntheticCanvas;
        uint32_t probeBefore = heap8Free();
        SDL_memset(&syntheticCanvas, 0, sizeof(syntheticCanvas));
        syntheticCanvas.doomRpg = doomRpg;
        printf("[STORYTEARDOWN] BEGIN heap8=%u owner=%d\n",
               (unsigned int)probeBefore, Esp32StoryFit_hasHand());
        if (!Esp32StoryFit_prepare(&syntheticCanvas) || !Esp32StoryFit_hasHand()) {
            printf("[STORYTEARDOWN] FAILED prepare owner=%d\n", Esp32StoryFit_hasHand());
            return 0;
        }
        DoomCanvas_free(&syntheticCanvas, false);
        if (Esp32StoryFit_hasHand()) {
            printf("[STORYTEARDOWN] FAILED release owner=stale\n");
            return 0;
        }
        if (heap8Free() != probeBefore) {
            printf("[STORYTEARDOWN] FAILED heap8=%u->%u delta=%d\n",
                   (unsigned int)probeBefore, (unsigned int)heap8Free(),
                   (int)heap8Free() - (int)probeBefore);
            return 0;
        }
        printf("[STORYTEARDOWN] PASS prepare->DoomCanvas_free->released heap8=%u exact=yes\n",
               (unsigned int)heap8Free());
    }
#endif

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    printf("[PRERENDER] Begin: heap8=%u largest8=%u\n",
           (unsigned int)heapBefore, (unsigned int)largestBefore);

    before = heap8Free();
    printf("[PRERENDER] -> EspNativeMenuStorage_startup()\n");
    menuResult = EspNativeMenuStorage_startup(doomRpg->menuSystem, doomRpg);
    after = heap8Free();
    printStageResult("EspNativeMenuStorage_startup", before, after);
    if (!menuResult) {
        printf("[PRERENDER] FAILED native menu storage assets\n");
        return 0;
    }

    printf("[PRERENDER] EntityDef desktop startup retired; native catalog builds from PAK at resident-map load\n");

    preRenderReady = 1;
    printf("[PRERENDER] READY total used=%u heap8=%u largest8=%u\n",
           (unsigned int)(heapBefore >= heap8Free() ? heapBefore - heap8Free() : 0),
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());
    printf("[PRERENDER] ParticleSystem/EntityDef desktop startup retired; Render_startup / Game_loadConfig still NOT executed\n");

    return 1;
}
