#include "esp_native_root_state.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Render.h"
#include "esp_legacy_asset_source.h"
#include "platform_video_c_bridge.h"
#include "esp_render_startup_bridge.h"
#include "esp_native_trig.h"
#include "platform_video_config.h"

/* Include ESP-IDF bool macros only after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>
_Static_assert(DOOMRPG_CANVAS_WIDTH == DOOMRPG_LOGICAL_WIDTH &&
               DOOMRPG_CANVAS_HEIGHT == DOOMRPG_LOGICAL_HEIGHT,
               "render startup must use the native fixed-size CYD canvas");
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_BRINGUP_PROBES)
/* Desktop/bringup retain the original legacy BSP flags; production does not. */
_Static_assert(sizeof(Render_t) == 108U,
               "Render_t must omit desktop plane-color arrays, clipRect/skipCull/half-height mirrors, BSP diagnostics, raster state, world and allocations");
#endif


/* Render-shell compatibility slots have two native lifecycle callers:
 * MENU_MAIN new-game START and fatal root teardown. This is the previous
 * Render_freeRuntime body with only the native symbol substituted; map-owned
 * immutable arenas and compact overlays remain independent. */
void EspNativeRenderRuntime_releaseCompatibility(Render_t* render)
{
#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)
    int i;
#endif

    free(render->mediaTexels);
    render->mediaTexels = NULL;
    free(render->shapeData);
    render->shapeData = NULL;
    /* Desktop BSP/sprite/event/string pointer lists no longer exist in
     * production Render_t. Keep their original release on probe builds. */
#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)
    free(render->mapSprites);
    render->mapSprites = NULL;
    free(render->nodes);
    render->nodes = NULL;
    free(render->lines);
    render->lines = NULL;
    render->numMapSprites = 0;
    render->linesLength = 0;
    free(render->tileEvents);
    render->tileEvents = NULL;
    free(render->mapByteCode);
    render->mapByteCode = NULL;
    for (i = 0; i < render->mapStringCount; i++) {
        free(render->mapStringsIDs[i]);
    }
    free(render->mapStringsIDs);
    render->mapStringsIDs = NULL;
    render->mapStringCount = 0;
#endif
    free(render->mediaBitShapeOffsets);
    render->mediaBitShapeOffsets = NULL;
    free(render->mediaTexelOffsets);
    render->mediaTexelOffsets = NULL;
    free(render->mediaTexturesIds);
    render->mediaTexturesIds = NULL;
    free(render->mediaSpriteIds);
    render->mediaSpriteIds = NULL;
}

/*
 * Permanent ESP32 owner of the layout Render API root.
 * The original source bodies are preserved exactly, except for one-time
 * source-ownership diagnostics; all legacy world/raster logic stays excluded.
 */
/* Native PAK palette loader retains RGB565 conversion, allocation and
 * fatal OOM behavior; the unused desktop heap ledger is no longer resident. */
static void EspNativeRenderStartup_loadPalettes(Render_t* render)
{
	byte* fData;
	int dataPos = 0, i;
	short color;
	int red, green, blue;


	fData = EspLegacyAssetSource_readAlloc("palettes.bin", NULL);

	free(render->mediaPalettes);

	render->mediaPalettesLength = DoomRPG_intAtNext(fData, &dataPos) / 2;
	render->mediaPalettes = (short*)malloc(render->mediaPalettesLength * sizeof(short));
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

	free(fData);

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
        if (!EspLegacyAssetSource_stat(name, &bytes) ||
            (i == 0U && bytes < ESP_NATIVE_TRIG_BYTES)) {
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
    int width;
    int height;
    size_t framebufferBytes;
    byte* sharedFramebuffer;

    if (render == NULL || EspNativeRootOwners_live()->doomCanvas == NULL) {
        printf("[RENDER] ERROR invalid Render_startup object graph\n");
        return 0;
    }

    /* Classic CYD geometry is platform-owned and constant, not a mutable
     * SDLVideo_t mirror. The shared framebuffer capacity and pitch remain
     * validated below before writing any pixel. */

    printf("[RENDERSTART] OWNER api=Render_startup source=esp-native-render-startup direct=yes wrap=no\n");

    fData = EspLegacyAssetSource_readAlloc("sintable.bin", NULL);
    if (fData == NULL) {
        printf("[RENDER] ERROR unable to load sintable.bin\n");
        return 0;
    }

    /* Preserve the exact native sine values loaded from PAK. */
    if (!EspNativeTrig_installLe(fData, ESP_NATIVE_TRIG_BYTES)) {
        free(fData);
        printf("[RENDER] ERROR native sine table install failed\n");
        return 0;
    }
    free(fData);
    printf("[RENDER] sintable loaded: %u bytes\n",
           (unsigned int)ESP_NATIVE_TRIG_BYTES);

    width = DOOMRPG_CANVAS_WIDTH;
    height = DOOMRPG_CANVAS_HEIGHT;
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

int EspRenderStartupBridge_start(DoomRPG_t* root, int preRenderReady) {
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

    if (root == NULL || EspNativeRootOwners_live()->render == NULL ||
        EspNativeRootOwners_live()->doomCanvas == NULL) {
        printf("[RENDERSTART] Core object graph incomplete; probe refused\n");
        return 0;
    }

    if (!preflightRenderResources()) {
        return 0;
    }

    render = EspNativeRootOwners_live()->render;
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

    expectedPitch = ((DOOMRPG_CANVAS_WIDTH * (int)sizeof(uint16_t)) + 3) & ~3;

    if (!result || render->framebuffer != expectedFramebuffer ||
        render->pitch != expectedPitch ||
        render->mediaPalettes == NULL || render->mediaPalettesLength <= 0) {
        printf("[RENDERSTART] FAILED fb=%p expected=%p pitch=%d/%d renderer=%dx%d palettes=%p len=%d\n",
               (void*)render->framebuffer,
               (void*)expectedFramebuffer,
               render->pitch, expectedPitch,
               DOOMRPG_CANVAS_WIDTH, DOOMRPG_CANVAS_HEIGHT,
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
