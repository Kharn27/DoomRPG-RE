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
_Static_assert(sizeof(Render_t) == 1456U,
               "Render_t must omit legacy BSP/plane/line/span palette state");
#endif


extern DoomRPG_t* doomRpg;

/* Render-shell compatibility slots have two native lifecycle callers:
 * MENU_MAIN new-game START and fatal root teardown. This is the previous
 * Render_freeRuntime body with only the native symbol substituted; map-owned
 * immutable arenas and compact overlays remain independent. */
void EspNativeRenderRuntime_releaseCompatibility(Render_t* render)
{
    int i;

    SDL_free(render->mediaTexels);
    render->mediaTexels = NULL;
    SDL_free(render->shapeData);
    render->shapeData = NULL;
    SDL_free(render->mapSprites);
    render->mapSprites = NULL;
    render->numMapSprites = 0;
    SDL_free(render->nodes);
    render->nodes = NULL;
    SDL_free(render->lines);
    render->lines = NULL;
    render->linesLength = 0;
    SDL_free(render->tileEvents);
    render->tileEvents = NULL;
    SDL_free(render->mapByteCode);
    render->mapByteCode = NULL;
    for (i = 0; i < render->mapStringCount; i++) {
        SDL_free(render->mapStringsIDs[i]);
    }
    SDL_free(render->mapStringsIDs);
    render->mapStringCount = 0;
    render->mapStringsIDs = NULL;
    SDL_free(render->mediaBitShapeOffsets);
    render->mediaBitShapeOffsets = NULL;
    SDL_free(render->mediaTexelOffsets);
    render->mediaTexelOffsets = NULL;
    SDL_free(render->mediaTexturesIds);
    render->mediaTexturesIds = NULL;
    SDL_free(render->mediaSpriteIds);
    render->mediaSpriteIds = NULL;
    SDL_free(render->mapTextureTexels);
    render->mapTextureTexels = NULL;
    SDL_free(render->mapSpriteTexels);
    render->mapSpriteTexels = NULL;
}

/*
 * Permanent ESP32 owner of the layout Render API root.
 * The original source bodies are preserved exactly, except for one-time
 * source-ownership diagnostics; all legacy world/raster logic stays excluded.
 */
/* Native startup-owned palette loader. The original desktop body, including
 * paletteMemory accounting and fatal OOM contract, remains unchanged. */
static void EspNativeRenderStartup_loadPalettes(Render_t* render)
{
	byte* fData;
	int dataPos = 0, i;
	short color;
	int red, green, blue;

	render->paletteMemory = DoomRPG_freeMemory();

	fData = DoomRPG_fileOpenRead(render->doomRpg, "/palettes.bin");

	SDL_free(render->mediaPalettes);

	render->mediaPalettesLength = DoomRPG_intAtNext(fData, &dataPos) / 2;
	render->mediaPalettes = (short*)SDL_malloc(render->mediaPalettesLength * sizeof(short));
	if (render->mediaPalettes == NULL) {
		DoomRPG_Error("Render_loadPalettes: Insufficient memory for allocation");
	}

	//printf("render->mediaPalettesLength %d\n", render->mediaPalettesLength);

	for (i = 0; i < render->mediaPalettesLength; i++)
	{
		color = DoomRPG_shortAtNext(fData, &dataPos);

		blue = (color >> 11) & 0x1f;    // (color << 16) >> 27;
		blue = (blue << 3) | (blue >> 2);

		green = (color >> 5) & 0x3f;    // (color << 21) >> 26;
		green = (green << 2) | (green >> 4);

		red = (color & 0x1f);
		red = (red << 3) | (red >> 2);
		render->mediaPalettes[i] = (short)Render_make565RGB(render, blue, green, red);
	}

	SDL_free(fData);

	render->paletteMemory = DoomRPG_freeMemory() - render->paletteMemory;
	//printf("paletteMemory %d\n", render->paletteMemory);
}

unsigned int Render_make565RGB(Render_t* render, int blue, int green, int red)
{
	return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}

unsigned short Render_RGB888_To_RGB565(Render_t* render, int rgb)
{
	return (unsigned short)Render_make565RGB(render, rgb & 0xff, (rgb >> 8) & 0xff, (rgb >> 16) & 0xff);
}

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

/*
 * Private ESP32 Render startup implementation owned by EspRenderStartupBridge_start.
 * Desktop Render_startup ABI is retired. No duplicate framebuffer or SDL texture.
 */
static int EspNativeRenderStartup_initialize(Render_t* render) {
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

    printf("[RENDERSTART] OWNER api=Render_startup source=esp-native-render-startup direct=yes wrap=no\n");

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
     * storage. The desktop-only piDIB handle is not part of CYD Render_t.
     */
    render->framebuffer = sharedFramebuffer;
    memset(render->framebuffer, 0xff, framebufferBytes);

    printf("[RENDER] Shared framebuffer %dx%d pitch=%d bytes=%u ptr=%p\n",
           width, height, render->pitch,
           (unsigned int)framebufferBytes,
           (void*)render->framebuffer);

    EspNativeRenderStartup_loadPalettes(render);
    if (render->mediaPalettes == NULL || render->mediaPalettesLength <= 0) {
        printf("[RENDER] ERROR palettes not initialized\n");
        return 0;
    }

    printf("[RENDER] palettes loaded: entries=%d bytes=%u\n",
           render->mediaPalettesLength,
           (unsigned int)(render->mediaPalettesLength * sizeof(short)));

    return 1;
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

    /* Native startup owner calls its private initializer directly; no legacy ABI or --wrap. */
    result = EspNativeRenderStartup_initialize(render);
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
        render->pitch != expectedPitch ||
        render->clipRect.w != DOOMRPG_CANVAS_WIDTH ||
        render->clipRect.h != DOOMRPG_CANVAS_HEIGHT ||
        render->mediaPalettes == NULL || render->mediaPalettesLength <= 0) {
        printf("[RENDERSTART] FAILED fb=%p expected=%p pitch=%d/%d clip=%dx%d palettes=%p len=%d\n",
               (void*)render->framebuffer,
               (void*)expectedFramebuffer,
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
