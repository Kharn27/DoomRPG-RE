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
_Static_assert(sizeof(Render_t) == 1532U,
               "Render_t must exclude 1024 B BSP flags plus 2048 B legacy plane tiles");
#endif


extern DoomRPG_t* doomRpg;
void __real_Render_free(Render_t* render, boolean freePtr);

/*
 * Permanent ESP32 owner of the two constructor/layout Render API roots.
 * The original source bodies are preserved exactly, except for one-time
 * source-ownership diagnostics; all legacy world/raster logic stays excluded.
 */
Render_t* Render_init(Render_t* render, DoomRPG_t* doomRpg)
{
	printf("Render_init\n");

	if (render == NULL)
	{
		render = SDL_malloc(sizeof(Render_t));
		if (render == NULL) {
			return NULL;
		}
	}
	SDL_memset(render, 0, sizeof(Render_t));

	//resourceAsStream.Init(&renderClass->mapFile, doomRPGClass, 1);
	render->doomRpg = doomRpg;
	render->skipStretch = 0;
	render->unk4 = 0;
	render->skipCull = 0;
	render->skipBSP = 0;
	render->skipLines = 0;
	render->unk5 = 0;
	render->skipSprites = 0;
	render->skipViewNudge = 0;
	render->ioBufferPos = 0;
	render->lines = NULL;
	render->nodes = NULL;
	render->mapSprites = NULL;
	render->mapCameraSpawnIndex = 0;
	render->floorColor = NULL;
	render->ceilingColor = NULL;
	render->ceilingTex = 0;
	render->floorTex = 0;
	render->columnScale = NULL;
	render->animFrameTime = 0;
	render->mapStringsIDs = NULL;
	render->mapStringCount = 0;

	printf("[RENDERCORE] INIT owner=esp-native-bridge bytes=%u\n", (unsigned int)sizeof(*render));
	return render;
}

void Render_setup(Render_t* render, SDL_Rect* windowRect)
{
	boolean memError = false;
	render->screenWidth = windowRect->w;
	render->screenHeight = windowRect->h;
	render->screenX = windowRect->x;
	render->screenY = windowRect->y;
	if ((windowRect->h & 1) != 0) {
		render->screenHeight = windowRect->h - 1;
	}
	render->halfScreenWidth = render->screenWidth / 2;
	render->halfScreenHeight = render->screenHeight / 2;
	render->fracHalfScreenWidth = (render->halfScreenWidth << FRACBITS) - 0x8000;
	render->fracHalfScreenHeight = (render->halfScreenHeight << FRACBITS) - 0x8000;

#if 0
	printf("render->screenWidth %d\n", render->screenWidth);
	printf("render->screenHeight %d\n", render->screenHeight);
	printf("render->screenX %d\n", render->screenX);
	printf("render->screenY %d\n", render->screenY);
	printf("render->halfScreenWidth %d\n", render->halfScreenWidth);
	printf("render->halfScreenHeight %d\n", render->halfScreenHeight);
	printf("render->fracHalfScreenWidth %d\n", render->fracHalfScreenWidth);
	printf("render->fracHalfScreenHeight %d\n", render->fracHalfScreenHeight);
#endif

	SDL_free(render->ceilingColor);
	render->ceilingColor = SDL_malloc(render->screenWidth * sizeof(short));
	if (render->ceilingColor == NULL) { memError = true; }

	SDL_free(render->floorColor);
	render->floorColor = SDL_malloc(render->screenWidth * sizeof(short));
	if (render->floorColor == NULL) { memError = true; }

	SDL_free(render->columnScale);
	render->columnScale = SDL_malloc(render->screenWidth * sizeof(int));
	if (render->columnScale == NULL) { memError = true; }

	if (memError) {
		//DoomRPG_setErrorID(render->doomRpg, 2);
		DoomRPG_Error("Render: Insufficient memory for allocation");
	}
	printf("[RENDERCORE] SETUP owner=esp-native-bridge view=%dx%d@%d,%d arrays=%uB\n",
	       render->screenWidth, render->screenHeight,
	       render->screenX, render->screenY,
	       (unsigned int)(render->screenWidth * (sizeof(short) * 2U + sizeof(int))));
}


/* Original Render palette and RGB565 ABI behavior, now native-owned. */
void Render_loadPalettes(Render_t* render)
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

void Render_setGrayPalettes(Render_t* render)
{
	short* mediaPalettes, color, grayColor;
	#ifndef DOOMRPG_ESP32
	short* mediaPlanes;
	#endif
	int i, j;

	for (i = 0; i < render->mediaPalettesLength; i++) {
		mediaPalettes = render->mediaPalettes;
		color = mediaPalettes[i];
		grayColor = (((color & 0xf800) >> 10) + ((color >> 5) & 0x3f) + ((color & 0x1f) << 1)) / 3; //RGB
		mediaPalettes[i] = ((grayColor >> 1) << 11) | (grayColor << 5) | (grayColor >> 1);
	}

	#ifndef DOOMRPG_ESP32
	for (i = 0; i < render->planeTexturesCnt; i++)
	{
		for (j = 0; j < (64 * 64); j++) {
			mediaPlanes = &render->mediaPlanes[i][j];
			color = mediaPlanes[0];
			grayColor = (((color & 0xf800) >> 10) + ((color >> 5) & 0x3f) + ((color & 0x1f) << 1)) / 3; //RGB
			mediaPlanes[0] = ((grayColor >> 1) << 11) | (grayColor << 5) | (grayColor >> 1);
		}
	}
	#endif

	color = render->floorColor[0];
	grayColor = (((color & 0xf800) >> 10) + ((color >> 5) & 0x3f) + ((color & 0x1f) << 1)) / 3; //RGB
	render->floorColor[0] = ((grayColor >> 1) << 11) | (grayColor << 5) | (grayColor >> 1);

	color = render->ceilingColor[0];
	grayColor = (((color & 0xf800) >> 10) + ((color >> 5) & 0x3f) + ((color & 0x1f) << 1)) / 3; //RGB
	render->ceilingColor[0] = ((grayColor >> 1) << 11) | (grayColor << 5) | (grayColor >> 1);
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
 * Direct permanent ESP32 Render startup root. The original desktop startup
 * (SDL texture + second RGB565 framebuffer) is excluded from ESP32 linking.
 */
int Render_startup(Render_t* render) {
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
