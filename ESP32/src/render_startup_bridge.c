#include <SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Render.h"
#include "SDL_Video.h"
#include "esp_legacy_asset_source.h"
#include "platform_video_c_bridge.h"
#include "esp_render_startup_bridge.h"
#include "platform_video_config.h"

/* Include ESP-IDF bool macros only after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_BRINGUP_PROBES)
/* Desktop/bringup retain the original legacy BSP flags; production does not. */
_Static_assert(sizeof(Render_t) == 1968U,
               "Render_t must exclude 1024 B BSP flags plus 2048 B legacy plane tiles");
#endif


extern DoomRPG_t* doomRpg;
void __real_Render_free(Render_t* render, boolean freePtr);

static int renderStartupAttempted = 0;
static int renderStartupReady = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int preflightRenderResources(void) {
    static const char* const required[] = {
        "sintable.bin",
        "palettes.bin",
    };
    const unsigned int count = sizeof(required) / sizeof(required[0]);
    unsigned int i;
    int allPresent = 1;

    printf("[RENDERSTART] Resource preflight (%u files)\n", count);

    for (i = 0; i < count; ++i) {
        const char* name = required[i];
        uint32_t bytes = 0U;
        if (!EspLegacyAssetSource_stat(name, &bytes)) {
            printf("[RENDERSTART] MISSING %s\n", name);
            allPresent = 0;
            continue;
        }

        printf("[RENDERSTART] %-14s bytes=%u backing=pak\n",
               name, (unsigned int)bytes);
    }

    if (!allPresent) {
        printf("[RENDERSTART] Resource preflight FAILED; startup skipped safely\n");
        return 0;
    }

    printf("[RENDERSTART] Resource preflight OK\n");
    return 1;
}

int __wrap_Render_startup(Render_t* render) {
    byte* fData;
    int i;
    int width;
    int height;
    size_t framebufferBytes;
    byte* sharedFramebuffer;

    if (render == NULL || render->doomRpg == NULL ||
        render->doomRpg->doomCanvas == NULL) {
        printf("[RENDER] ERROR invalid Render_startup object graph\n");
        return 0;
    }

    fData = DoomRPG_fileOpenRead(render->doomRpg, "/sintable.bin");
    if (fData == NULL) {
        printf("[RENDER] ERROR unable to load sintable.bin\n");
        return 0;
    }

    SDL_memmove(render->sinTable, fData, sizeof(render->sinTable));
    SDL_free(fData);
    for (i = 0; i < 256; ++i) {
        render->sinTable[i] = SDL_SwapLE32(render->sinTable[i]);
    }
    printf("[RENDER] sintable loaded: %u bytes\n",
           (unsigned int)sizeof(render->sinTable));

    render->clipRect.x = DOOMRPG_CANVAS_X;
    render->clipRect.y = DOOMRPG_CANVAS_Y;
    render->clipRect.w = DOOMRPG_CANVAS_WIDTH;
    render->clipRect.h = DOOMRPG_CANVAS_HEIGHT;

    width = sdlVideo.rendererW;
    height = sdlVideo.rendererH;
    render->pitch = ((width * (int)sizeof(uint16_t)) + 3) & ~3;
    framebufferBytes = (size_t)render->pitch * (size_t)height;
    sharedFramebuffer = (byte*)Esp32PlatformVideo_framebuffer();

    if (sharedFramebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() < framebufferBytes) {
        printf("[RENDER] ERROR platform framebuffer unavailable: need=%u have=%u\n",
               (unsigned int)framebufferBytes,
               (unsigned int)Esp32PlatformVideo_framebufferSizeBytes());
        return 0;
    }

    /*
     * Desktop Render_startup() owns both a streaming SDL texture and a second
     * RGB565 framebuffer. On the CYD the SDL renderer already draws directly
     * into PlatformVideo's 160x120 RGB565 framebuffer, so Render uses that same
     * storage and piDIB deliberately stays NULL.
     */
    render->piDIB = NULL;
    render->framebuffer = sharedFramebuffer;
    memset(render->framebuffer, 0xff, framebufferBytes);

    printf("[RENDER] Shared framebuffer %dx%d pitch=%d bytes=%u ptr=%p\n",
           width, height, render->pitch,
           (unsigned int)framebufferBytes,
           (void*)render->framebuffer);

    Render_loadPalettes(render);
    if (render->mediaPalettes == NULL || render->mediaPalettesLength <= 0) {
        printf("[RENDER] ERROR palettes not initialized\n");
        return 0;
    }

    printf("[RENDER] palettes loaded: entries=%d bytes=%u\n",
           render->mediaPalettesLength,
           (unsigned int)(render->mediaPalettesLength * sizeof(short)));

    return 1;
}

void __wrap_Render_free(Render_t* render, boolean freePtr) {
    if (render != NULL &&
        render->framebuffer == (byte*)Esp32PlatformVideo_framebuffer()) {
        /* Render does not own PlatformVideo's shared framebuffer. */
        render->framebuffer = NULL;
    }

    __real_Render_free(render, freePtr);
}

int EspRenderStartupBridge_start(int preRenderReady) {
    Render_t* render;
    byte* expectedFramebuffer;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t used;
    int expectedPitch;
    int result;

    if (renderStartupAttempted) {
        return renderStartupReady;
    }
    renderStartupAttempted = 1;

    printf("\n=== Doom RPG shared Render_startup probe ===\n");

    if (!preRenderReady) {
        printf("[RENDERSTART] Pre-render startup is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL) {
        printf("[RENDERSTART] Core object graph incomplete; probe refused\n");
        return 0;
    }

    if (!preflightRenderResources()) {
        return 0;
    }

    render = doomRpg->render;
    expectedFramebuffer = (byte*)Esp32PlatformVideo_framebuffer();
    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[RENDERSTART] Begin: heap8=%u largest8=%u platformFB=%p bytes=%u\n",
           (unsigned int)heapBefore,
           (unsigned int)largestBefore,
           (void*)expectedFramebuffer,
           (unsigned int)Esp32PlatformVideo_framebufferSizeBytes());

    /* Call the original symbol name intentionally: --wrap must redirect it. */
    result = Render_startup(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    used = heapBefore >= heapAfter ? heapBefore - heapAfter : 0;

    printf("[RENDERSTART] Render_startup result=%d used=%u heap8=%u largest8=%u\n",
           result,
           (unsigned int)used,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);

    expectedPitch = ((sdlVideo.rendererW * (int)sizeof(uint16_t)) + 3) & ~3;

    if (!result || render->framebuffer != expectedFramebuffer ||
        render->piDIB != NULL || render->pitch != expectedPitch ||
        render->clipRect.w != DOOMRPG_CANVAS_WIDTH ||
        render->clipRect.h != DOOMRPG_CANVAS_HEIGHT ||
        render->mediaPalettes == NULL || render->mediaPalettesLength <= 0) {
        printf("[RENDERSTART] FAILED fb=%p expected=%p piDIB=%p pitch=%d/%d clip=%dx%d palettes=%p len=%d\n",
               (void*)render->framebuffer,
               (void*)expectedFramebuffer,
               (void*)render->piDIB,
               render->pitch, expectedPitch,
               render->clipRect.w, render->clipRect.h,
               (void*)render->mediaPalettes,
               render->mediaPalettesLength);
        return 0;
    }

    renderStartupReady = 1;
    printf("[RENDERSTART] READY shared framebuffer, sintable and palettes initialized\n");
    printf("[RENDERSTART] fb=%p pitch=%d paletteEntries=%d paletteBytes=%u\n",
           (void*)render->framebuffer,
           render->pitch,
           render->mediaPalettesLength,
           (unsigned int)(render->mediaPalettesLength * sizeof(short)));
    printf("[RENDERSTART] Game_loadConfig / mappings / BSP still NOT executed\n");

    return 1;
}
