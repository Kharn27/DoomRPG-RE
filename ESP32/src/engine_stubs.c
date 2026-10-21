#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "esp_native_menu_state.h"
#include "Render.h"
#include "SDL_Video.h"
#include "Sound.h"
#include "Z_Zone.h"
#include "Z_Zip.h"
#include "engine_metrics.h"
#include "esp_native_intro_state.h"
#include "native_story_fit.h"
#include "esp_native_audio_intent.h"
#include "esp_native_menu_storage.h"
#include "platform_video_config.h"
#include "platform_video_c_bridge.h"

/* DoomRPG.h defines its original J2ME-style boolean before ESP-IDF brings in
 * the C99 false/true macros. */
#include <esp_heap_caps.h>

#ifdef DOOMRPG_ESP32
/* Legacy Game_t is a reference-only type; it must never be instantiated.
 * DoomRPG_t keeps an ABI compatibility field, permanently NULL on ESP32. */
#endif

/* The final linked DoomRPG.c globals/functions are now native-owned.
 * Keep the one process root, never create an extra session-owned mirror. */
DoomRPG_t* doomRpg = NULL;

/* Native process-root Canvas teardown. Preserve the desktop release order
 * and its sole CYD freePtr=true call; no Canvas lifecycle ABI survives. */
static void EspNativeEngineRoot_releaseCanvas(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL) return;
    Esp32StoryFit_release(doomCanvas);
    EspNativeIntroState_release(doomCanvas->doomRpg);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgFont);
    SDL_free(doomCanvas);
}

/* Process-root-owned Render destructor. The original native cleanup body and
 * freePtr semantics are retained verbatim apart from the private symbol name.
 * In particular, the shared PlatformVideo framebuffer must be detached before
 * Render_freeRuntime and must never be released by the Render shell.
 * The legacy owner wording in the Serial diagnostic is historical. */
static void EspNativeEngineRoot_releaseRender(Render_t* render, boolean freePtr)
{
    const int sharedFramebuffer = render != NULL &&
        render->framebuffer == (byte*)Esp32PlatformVideo_framebuffer();
    if (sharedFramebuffer) {
        render->framebuffer = NULL;
    }
    printf("[RENDERFREE] ENTRY owner=esp-native-render-startup direct=yes wrap=no sharedFB=%d freePtr=%d\n",
           sharedFramebuffer, (int)freePtr);

    Render_freeRuntime(render);
    SDL_free(render->ceilingColor);
    SDL_free(render->floorColor);
    SDL_free(render->columnScale);
    SDL_free(render->mediaPalettes);

    SDL_free(render->framebuffer);
    render->framebuffer = NULL;

    if (render->piDIB) {
        SDL_DestroyTexture(render->piDIB);
        render->piDIB = NULL;
    }

    if (freePtr) {
        SDL_free(render);
    }
}

/* Original metric contract. Z_FreeMemory() is the single native heap report. */
int DoomRPG_freeMemory(void) { // 0x1EBFC
    return Z_FreeMemory();
}

/* This is the generated ESP32-compatible root teardown contract. World owners
 * are released elsewhere; legacy large members stay NULL. Preserve the exact
 * disposal order of the last linked generated provider on fatal cleanup.
 * Normal HUB->EXIT releases only resident/session owners, not this root. */
void DoomRPG_FreeAppData(DoomRPG_t* doomrpg)
{
    if (doomRpg == NULL)
        return;

    /* Legacy Combat/Particle/Player/EntityDef/Sound/Hud are retired on CYD. */
    doomrpg->combat = NULL;
    doomrpg->particleSystem = NULL;
    doomrpg->player = NULL;
    /* No Game allocation/teardown exists on CYD; native resident and
     * session owners are released by their own lifecycle. */
    doomrpg->game = NULL;
    doomrpg->entityDef = NULL;
    doomrpg->sound = NULL;
    doomrpg->hud = NULL;
    if (doomrpg->menuSystem) {
        EspNativeMenuStorage_free(doomrpg->menuSystem, doomrpg, true);
    }
    doomrpg->menuSystem = NULL;
    if (doomrpg->menu) {
        SDL_free(doomrpg->menu);
    }
    doomrpg->menu = NULL;
    if (doomrpg->render) {
        EspNativeEngineRoot_releaseRender(doomrpg->render, true);
    }
    doomrpg->render = NULL;
    if (doomrpg->doomCanvas) {
        EspNativeEngineRoot_releaseCanvas(doomrpg->doomCanvas);
    }
    doomrpg->doomCanvas = NULL;
    SDL_free(doomrpg);
}

/* Fatal desktop callback retains its message/teardown semantics. The only
 * intentional deviation is skipping the historical ZIP close: ESP32 has no
 * ZIP runtime handle and PAK/SD lifetimes are owned independently. */
void DoomRPG_Error(const char* fmt, ...)
{
    char errMsg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errMsg, sizeof(errMsg), fmt, ap);
    va_end(ap);

    printf("%s", errMsg);

    const SDL_MessageBoxButtonData buttons[] = {
        {0, 0, "Ok"},
    };
    const SDL_MessageBoxColorScheme colorScheme = {
        {
            {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
            {0, 0, 255}, {255, 0, 255}
        }
    };
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_ERROR, NULL, "DoomRPG Error", errMsg,
        SDL_arraysize(buttons), buttons, &colorScheme
    };
    SDL_ShowMessageBox(&messageboxdata, NULL);
    /* Native PAK backing store is not a desktop ZIP and is not closed here. */
    DoomRPG_FreeAppData(doomRpg);
    SDL_CloseAudio();
    SDL_Close();
    exit(0);
}


SDLVideo_t sdlVideo = {
    NULL,
    (SDL_Renderer*)1,
    DOOMRPG_LOGICAL_WIDTH,
    DOOMRPG_LOGICAL_HEIGHT,
    false,
    false,
    true,
    false,
    0,
};

FluidSynth_t fluidSynth = {NULL, NULL, NULL};
SDLController_t sdlController = {NULL, NULL, NULL, 0, 0};

SDLVidModes_t sdlVideoModes[14] = {
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},
};

void SDL_InitVideo(void) {}
void SDL_Close(void) {}
SDLVideo_t* SDL_GetVideo(void) { return &sdlVideo; }

void SDL_InitAudio(void) {}
void SDL_CloseAudio(void) {}
int SDL_GameControllerGetButtonID(void) { return -1; }
char* SDL_GameControllerGetNameButton(int id) {
    static char name[] = "Controller";
    (void)id;
    return name;
}
char* SDL_MouseGetNameButton(int id) {
    static char name[] = "Touch";
    (void)id;
    return name;
}
int SDL_JoystickGetButtonID(void) { return -1; }

void Sound_stopSounds(Sound_t* sound) { (void)sound; }
void Sound_freeSound(Sound_t* sound, int chan) { (void)sound; (void)chan; }
int Sound_getState(Sound_t* sound, int resourceID) { (void)sound; (void)resourceID; return 0; }
int Sound_getFreeChanel(Sound_t* sound) { (void)sound; return -1; }
void Sound_loadSound(Sound_t* sound, int chan, short resourceID) {
    (void)sound; (void)chan; (void)resourceID;
}
void Sound_readySound(Sound_t* sound, int chan) { (void)sound; (void)chan; }
void Sound_playSound(Sound_t* sound, int resourceID, byte flags, int priority) {
    (void)sound; (void)resourceID; (void)flags; (void)priority;
}
void Sound_freeSounds(Sound_t* sound) { (void)sound; }
int Sound_getFromResourceID(int resourceID) { (void)resourceID; return -1; }
void Sound_updateVolume(Sound_t* sound) { (void)sound; }
int Sound_minusVolume(Sound_t* sound, int volume) {
    if (sound == NULL) return 0;
    sound->volume -= volume;
    if (sound->volume < 0) sound->volume = 0;
    return sound->volume;
}
int Sound_addVolume(Sound_t* sound, int volume) {
    if (sound == NULL) return 0;
    sound->volume += volume;
    if (sound->volume > 100) sound->volume = 100;
    return sound->volume;
}

void Z_Init(void) {}
void* SDLCALL Z_Malloc(size_t size) { return malloc(size); }
void* SDLCALL Z_Calloc(size_t count, size_t size) { return calloc(count, size); }
void* SDLCALL Z_Realloc(void* ptr, size_t size) { return realloc(ptr, size); }
void SDLCALL Z_Free(void* ptr) { free(ptr); }
int Z_FreeMemory(void) {
    return (int)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void DoomRPG_getEngineMetrics(DoomRpgEngineMetrics* metrics) {
    if (metrics == NULL) return;
    metrics->doomRpg = sizeof(DoomRPG_t);
    metrics->doomCanvas = sizeof(DoomCanvas_t);
    metrics->render = sizeof(Render_t);
    metrics->game = 0U;  /* Game_t is no longer a resident object. */
    metrics->player = 0U;
    metrics->combat = 0U;
    metrics->supportObjects = sizeof(EspNativeMenuState_t);
    metrics->totalInitialObjects = metrics->doomRpg + metrics->doomCanvas +
        metrics->render + metrics->game + metrics->supportObjects;
}

static DoomRpgCoreInitReport coreInitReport;
static boolean coreInitAttempted = false;
static DoomRpgLayoutReport layoutReport;
static boolean layoutAttempted = false;

static uint32_t coreFreeHeap(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t coreLargestBlock(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

uint32_t DoomRPG_getHeap8Free(void) {
    return coreFreeHeap();
}

uint32_t DoomRPG_getLargest8BitBlock(void) {
    return coreLargestBlock();
}

const char* DoomRPG_coreStageName(uint8_t stage) {
    static const char* const names[DOOMRPG_CORE_STAGE_COUNT] = {
        "DoomRPG", "DoomCanvas", "Render", "MenuSystem",
        "Game", "Player"
    };
    return stage < DOOMRPG_CORE_STAGE_COUNT ? names[stage] : "unknown";
}

static void recordCoreStage(DoomRpgCoreStage stage, uint32_t before,
                            uint32_t after) {
    const uint32_t used = before >= after ? before - after : 0;
    coreInitReport.stageBytes[stage] = used;
    coreInitReport.completedStages = (uint8_t)stage + 1;
    printf("[CORE] %-14s used=%u heap=%u largest=%u\n",
           DoomRPG_coreStageName((uint8_t)stage), (unsigned int)used,
           (unsigned int)after, (unsigned int)coreLargestBlock());
}

static int failCoreStage(DoomRpgCoreStage stage) {
    coreInitReport.failedStage = (uint8_t)stage;
    coreInitReport.heapAfter = coreFreeHeap();
    coreInitReport.largestBlockAfter = coreLargestBlock();
    coreInitReport.bytesUsed = coreInitReport.heapBefore >= coreInitReport.heapAfter
        ? coreInitReport.heapBefore - coreInitReport.heapAfter : 0;
    coreInitReport.ready = 0;
    printf("[CORE] FAILED at %s, heap=%u largest=%u\n",
           DoomRPG_coreStageName((uint8_t)stage),
           (unsigned int)coreInitReport.heapAfter,
           (unsigned int)coreInitReport.largestBlockAfter);
    return 0;
}

/* Canvas allocation, zeroing and image-default initialization are now
 * direct root ownership. Preserve byte-identical historic behavior, except
 * for the internal name and the reported remaining public ABI size. */
_Static_assert(sizeof(DoomCanvas_t) == ESP32_DOOMCANVAS_COMPACT_BYTES,
               "ESP32 DoomCanvas_t layout changed; audit compatibility owners before proceeding");

static DoomCanvas_t* EspNativeEngineRoot_initCanvas(DoomCanvas_t* doomCanvas, DoomRPG_t* doomRpg)
{
    printf("DoomCanvas_init\n");

    if (doomCanvas == NULL) {
        doomCanvas = SDL_malloc(sizeof(DoomCanvas_t));
        if (doomCanvas == NULL) {
            return NULL;
        }
    }
    SDL_memset(doomCanvas, 0, sizeof(DoomCanvas_t));

    doomCanvas->doomRpg = doomRpg;
    doomCanvas->imgFont.imgBitmap = NULL;
    doomCanvas->fontColor = 0xffffffff;
    doomCanvas->renderFloorCeilingTextures = true;

    printf("[DOOMCANVASBRIDGE] INIT exports=0 nativeText=1 desktopTU=no bytes=%u retiredDialogStores=%u retiredZeroRefLayout=%u retiredDormantText=%u, retiredStateLayout=%u retiredGraphMirrors=%u retiredIntroState=%u retiredDeadShell=%u retiredInertControl=%u retiredLargeFont=%u retiredFixedGeometry=%u retiredViewShakeAlias=%u clip=%dx%d\n",
           (unsigned int)sizeof(DoomCanvas_t),
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES,
           DOOMRPG_CANVAS_WIDTH,
           DOOMRPG_CANVAS_HEIGHT);
    return doomCanvas;
}

/* Native Render constructor owned by the process root; old initializer body
 * intentionally unmodified. Other Render owners remain unchanged. */
static Render_t* EspNativeEngineRoot_initRender(Render_t* render, DoomRPG_t* doomRpg)
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

int DoomRPG_initEngineCore(DoomRpgCoreInitReport* report) {
    uint32_t before;
    uint32_t after;

    if (coreInitAttempted) {
        if (report != NULL) *report = coreInitReport;
        return coreInitReport.ready != 0;
    }
    coreInitAttempted = true;

    SDL_memset(&coreInitReport, 0, sizeof(coreInitReport));
    coreInitReport.failedStage = DOOMRPG_CORE_NO_FAILURE;
    coreInitReport.heapBefore = coreFreeHeap();
    coreInitReport.largestBlockBefore = coreLargestBlock();

    printf("[CORE] Begin real Doom RPG object graph: heap=%u largest=%u\n",
           (unsigned int)coreInitReport.heapBefore,
           (unsigned int)coreInitReport.largestBlockBefore);

    before = coreFreeHeap();
    doomRpg = (DoomRPG_t*)SDL_calloc(1, sizeof(DoomRPG_t));
    after = coreFreeHeap();
    recordCoreStage(DOOMRPG_CORE_ROOT, before, after);
    if (doomRpg == NULL) {
        failCoreStage(DOOMRPG_CORE_ROOT);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }

    doomRpg->memoryBeg = DoomRPG_freeMemory();
    doomRpg->imageMemory = 0;
    doomRpg->errorID = 0;
    doomRpg->upTimeMs = 0;
    doomRpg->graphSetCliping = false;
    doomRpg->closeApplet = false;

    /*
     * The ESP32 core root is deliberately calloc'd, unlike the inherited
     * desktop DoomRPG_Init() malloc path. Random_t therefore starts as an
     * all-zero 128-byte table with nextRand=0. DoomRPG_randNextByte() only
     * refills at the end of the table, so without an explicit first seed the
     * first 128 byte draws are forced to zero (crate outcome 0 => trapped,
     * crate blast low byte 0 => minimum blast, identical combat rolls).
     *
     * Materialize the canonical legacy table once when the real root is born.
     * DoomRPG_setRand() also initializes the hidden resetRand/_seed generator;
     * all later byte/word draws and replay-guard cadence remain unchanged.
     */
    DoomRPG_setRand(&doomRpg->random);
    {
        uint32_t randomFNV = 2166136261U;
        uint32_t nonZero = 0U;
        uint32_t i;
        for (i = 0U; i < RANDTABLESIZE; ++i) {
            randomFNV ^= doomRpg->random.randTable[i];
            randomFNV *= 16777619U;
            if (doomRpg->random.randTable[i] != 0U) ++nonZero;
        }
        if (doomRpg->random.nextRand != 0 || nonZero == 0U) {
            printf("[CORERNG] FAILED next=%d nonZero=%u tableFNV=%08x failClosed=yes\n",
                   doomRpg->random.nextRand,
                   (unsigned int)nonZero,
                   (unsigned int)randomFNV);
            failCoreStage(DOOMRPG_CORE_ROOT);
            if (report != NULL) *report = coreInitReport;
            return 0;
        }
        printf("[CORERNG] SEEDED next=0 bytes=%u nonZero=%u tableFNV=%08x source=legacy-DoomRPG_setRand cadence=unchanged\n",
               (unsigned int)RANDTABLESIZE,
               (unsigned int)nonZero,
               (unsigned int)randomFNV);
    }

    /* CYD input is touch-owned: desktop keyboard/controller bindings absent. */
    EspNativeAudioIntent_reset();

#define INIT_CORE_OBJECT(stage, member, expression) \
    do { \
        before = coreFreeHeap(); \
        doomRpg->member = (expression); \
        after = coreFreeHeap(); \
        recordCoreStage((stage), before, after); \
        if (doomRpg->member == NULL) { \
            failCoreStage((stage)); \
            if (report != NULL) *report = coreInitReport; \
            return 0; \
        } \
    } while (0)

    INIT_CORE_OBJECT(DOOMRPG_CORE_CANVAS, doomCanvas,
                     EspNativeEngineRoot_initCanvas(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_RENDER, render,
                     EspNativeEngineRoot_initRender(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_MENU_SYSTEM, menuSystem,
                     EspNativeMenuStorage_init(NULL));
    /* Keep the historical diagnostics slot without an allocation or legacy
     * Game_init call. Reject an unexpected Game shell, even when malloc'd by
     * an accidental new compatibility provider. */
    before = coreFreeHeap();
    if (doomRpg->game != NULL) {
        failCoreStage(DOOMRPG_CORE_GAME);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    recordCoreStage(DOOMRPG_CORE_GAME, before, coreFreeHeap());

#undef INIT_CORE_OBJECT

    /* No Game_t object: the legacy root field is permanently NULL. */
    printf("[CORE] Game object retired gameBytes=0 desktopBytes=36468 totalReclaimed=36468 field=NULL worldOwner=native\n");
    printf("[CORE] Desktop Game.c retired bridge=absent exports=0 nativeGameOwner=yes\n");

    /*
     * Player state is owned by the compact 52-byte native gameplay owner.
     * The inherited Player_t is not constructed and must remain NULL.
     */
    if (doomRpg->player != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired Player pointer=%p expected=NULL\n",
               (void*)doomRpg->player);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Player retired object=NULL owner=native-gameplay-player-state bytes=52\n");

    /*
     * Audio playback remains deferred on classic CYD. Production gameplay
     * publishes bounded EspNativeAudioIntent records while the inherited
     * Sound_t owner is retired. Compatibility Sound_playSound/stopSounds calls
     * are NULL-safe no-ops until a dedicated native audio milestone.
     */
    if (doomRpg->sound != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired Sound pointer=%p expected=NULL\n",
               (void*)doomRpg->sound);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Sound retired object=NULL owner=native-audio-intent playback=deferred\n");

    /*
     * Visible gameplay HUD composition, top-bar feedback, hub restoration and
     * status repainting are all native ESP32 owners. The inherited Hud_t
     * object is retired and must remain NULL.
     */
    if (doomRpg->hud != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired Hud pointer=%p expected=NULL\n",
               (void*)doomRpg->hud);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Hud retired object=NULL owner=native-gameplay-hud+status-feedback\n");

    /*
     * Native map/gameplay resolves entity definition metadata through the
     * compact EspEntityDefTypeCatalog built from /entities.db when a resident
     * map is loaded. The inherited EntityDefManager_t pointer is therefore a
     * retired desktop owner and must remain NULL.
     */
    if (doomRpg->entityDef != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired EntityDef pointer=%p expected=NULL\n",
               (void*)doomRpg->entityDef);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] EntityDef retired object=NULL owner=native-entitydef-catalog\n");

    /*
     * Player/monster attacks, retaliation, weapon presentation and damage math
     * are all owned by bounded native ESP32 subsystems. The inherited Combat_t
     * object is retired and must remain NULL.
     */
    if (doomRpg->combat != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired Combat pointer=%p expected=NULL\n",
               (void*)doomRpg->combat);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Combat retired object=NULL owners=native-combat+weapon+monster-turn\n");

    /*
     * Native gameplay owns its bounded gib overlay in EspNativeGameplayGibFx.
     * The inherited ParticleSystem object is no longer a production owner.
     * DoomRPG_t is calloc'd, so the legacy pointer must remain NULL forever on
     * ESP32 unless a future milestone explicitly introduces a native owner.
     */
    if (doomRpg->particleSystem != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired ParticleSystem pointer=%p expected=NULL\n",
               (void*)doomRpg->particleSystem);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] ParticleSystem retired object=NULL owner=native-gibfx\n");

    /*
     * Pre-game and in-game menu presentation is owned by bounded ESP32
     * models/services. The inherited Menu_t command/factory object is no
     * longer a production owner. Keep the legacy root field NULL.
     */
    if (doomRpg->menu != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_ROOT;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.bytesUsed =
            coreInitReport.heapBefore >= coreInitReport.heapAfter
                ? coreInitReport.heapBefore - coreInitReport.heapAfter
                : 0;
        coreInitReport.ready = 0;
        printf("[CORE] FAILED retired Menu pointer=%p expected=NULL\n",
               (void*)doomRpg->menu);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Menu root retired object=NULL owner=native-menu-models\n");

    coreInitReport.clipWidth = (uint16_t)DOOMRPG_CANVAS_WIDTH;
    coreInitReport.clipHeight = (uint16_t)DOOMRPG_CANVAS_HEIGHT;
    coreInitReport.heapAfter = coreFreeHeap();
    coreInitReport.largestBlockAfter = coreLargestBlock();
    coreInitReport.bytesUsed = coreInitReport.heapBefore >= coreInitReport.heapAfter
        ? coreInitReport.heapBefore - coreInitReport.heapAfter : 0;

    if (coreInitReport.clipWidth != DOOMRPG_LOGICAL_WIDTH ||
        coreInitReport.clipHeight != DOOMRPG_LOGICAL_HEIGHT) {
        coreInitReport.failedStage = DOOMRPG_CORE_CANVAS;
        coreInitReport.ready = 0;
        printf("[CORE] Geometry mismatch: canvas clip=%ux%u expected=%ux%u\n",
               coreInitReport.clipWidth, coreInitReport.clipHeight,
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }

    coreInitReport.ready = 1;
    printf("[CORE] READY stages=%u allocatedObjects=4 heap used=%u remaining=%u largest=%u clip=%ux%u\n",
           (unsigned int)coreInitReport.completedStages,
           (unsigned int)coreInitReport.bytesUsed,
           (unsigned int)coreInitReport.heapAfter,
           (unsigned int)coreInitReport.largestBlockAfter,
           coreInitReport.clipWidth, coreInitReport.clipHeight);
    printf("[CORE] Resource startup intentionally NOT executed\n");

    if (report != NULL) *report = coreInitReport;
    return 1;
}

/* Private native layout Render setup; preserve validated array/free/geometry
 * and odd-height behavior from the retired desktop-named compatibility API. */
static void EspNativeEngineLayout_setupRender(Render_t* render, SDL_Rect* windowRect)
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

/* Bounded CYD layout and font initialization, owned by the native process
 * layout stage instead of a desktop DoomCanvas startup ABI entry. The body
 * preserves the validated Render_setup -> map marker -> font ordering. */
static void EspNativeEngineLayout_startupCanvas(DoomCanvas_t* doomCanvas)
{
    SDL_Rect screenRect;
    DoomRPG_t* doomRpg;

    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) return;

    doomRpg = doomCanvas->doomRpg;
    screenRect.x = DOOMRPG_VIEWPORT_X;
    screenRect.y = DOOMRPG_VIEWPORT_Y;
    screenRect.w = DOOMRPG_VIEWPORT_WIDTH;
    screenRect.h = DOOMRPG_VIEWPORT_HEIGHT;
    EspNativeEngineLayout_setupRender(doomRpg->render, &screenRect);
    doomCanvas->startupMap = 1;
    doomCanvas->skipIntro = false;
    DoomRPG_createImage(doomCanvas->doomRpg, "a.bmp", true, &doomCanvas->imgFont);
    printf("[DOOMCANVASBRIDGE] STARTUP desktopTU=no display=%dx%d screen=%dx%d@%d,%d startupMap=%d hud=native geometry=fixed-cyd\n",
           DOOMRPG_CANVAS_WIDTH, DOOMRPG_CANVAS_HEIGHT,
           DOOMRPG_VIEWPORT_WIDTH, DOOMRPG_VIEWPORT_HEIGHT,
           DOOMRPG_VIEWPORT_X, DOOMRPG_VIEWPORT_Y,
           doomCanvas->startupMap);
}

int DoomRPG_startEngineLayout(DoomRpgLayoutReport* report) {
    DoomCanvas_t* canvas;
    Render_t* render;
    if (layoutAttempted) {
        if (report != NULL) *report = layoutReport;
        return layoutReport.ready != 0;
    }
    layoutAttempted = true;

    SDL_memset(&layoutReport, 0, sizeof(layoutReport));

    if (!coreInitReport.ready || doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->hud != NULL) {
        printf("[LAYOUT] Core graph is not ready; startup refused\n");
        if (report != NULL) *report = layoutReport;
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    render = doomRpg->render;

    layoutReport.heap8Before = coreFreeHeap();
    layoutReport.largest8Before = coreLargestBlock();

    printf("[LAYOUT] Begin DoomCanvas_startup: heap8=%u largest8=%u\n",
           (unsigned int)layoutReport.heap8Before,
           (unsigned int)layoutReport.largest8Before);
    printf("[LAYOUT] This stage configures fixed native HUD geometry; legacy Hud_t is retired\n");

    EspNativeEngineLayout_startupCanvas(canvas);

    printf("[LAYOUT] HUD owner=native-gameplay-hud legacyObject=%p top=20 bottom=20\n",
           (void*)doomRpg->hud);

    layoutReport.heap8After = coreFreeHeap();
    layoutReport.largest8After = coreLargestBlock();
    layoutReport.bytesUsed = layoutReport.heap8Before >= layoutReport.heap8After
        ? layoutReport.heap8Before - layoutReport.heap8After : 0;

    layoutReport.clipX = (int16_t)DOOMRPG_CANVAS_X;
    layoutReport.clipY = (int16_t)DOOMRPG_CANVAS_Y;
    layoutReport.clipWidth = (uint16_t)DOOMRPG_CANVAS_WIDTH;
    layoutReport.clipHeight = (uint16_t)DOOMRPG_CANVAS_HEIGHT;

    layoutReport.displayX = (int16_t)DOOMRPG_CANVAS_X;
    layoutReport.displayY = (int16_t)DOOMRPG_CANVAS_Y;
    layoutReport.displayWidth = (uint16_t)DOOMRPG_CANVAS_WIDTH;
    layoutReport.displayHeight = (uint16_t)DOOMRPG_CANVAS_HEIGHT;

    layoutReport.screenX = (int16_t)DOOMRPG_VIEWPORT_X;
    layoutReport.screenY = (int16_t)DOOMRPG_VIEWPORT_Y;
    layoutReport.screenWidth = (uint16_t)DOOMRPG_VIEWPORT_WIDTH;
    layoutReport.screenHeight = (uint16_t)DOOMRPG_VIEWPORT_HEIGHT;

    layoutReport.renderWidth = (uint16_t)render->screenWidth;
    layoutReport.renderHeight = (uint16_t)render->screenHeight;
    layoutReport.statusTopBarHeight = 20U;
    layoutReport.statusBarHeight = 20U;
    layoutReport.renderArrayPayloadBytes =
        (uint32_t)render->screenWidth *
        (uint32_t)(sizeof(short) + sizeof(short) + sizeof(int));

    printf("[LAYOUT] clip    x=%d y=%d w=%u h=%u\n",
           layoutReport.clipX, layoutReport.clipY,
           layoutReport.clipWidth, layoutReport.clipHeight);
    printf("[LAYOUT] display x=%d y=%d w=%u h=%u\n",
           layoutReport.displayX, layoutReport.displayY,
           layoutReport.displayWidth, layoutReport.displayHeight);
    printf("[LAYOUT] screen  x=%d y=%d w=%u h=%u\n",
           layoutReport.screenX, layoutReport.screenY,
           layoutReport.screenWidth, layoutReport.screenHeight);
    printf("[LAYOUT] HUD top=%u bottom=%u Render=%ux%u arrays=%uB\n",
           layoutReport.statusTopBarHeight, layoutReport.statusBarHeight,
           layoutReport.renderWidth, layoutReport.renderHeight,
           (unsigned int)layoutReport.renderArrayPayloadBytes);
    printf("[LAYOUT] heap8 used=%u remaining=%u largest=%u\n",
           (unsigned int)layoutReport.bytesUsed,
           (unsigned int)layoutReport.heap8After,
           (unsigned int)layoutReport.largest8After);

    if (layoutReport.clipWidth != DOOMRPG_LOGICAL_WIDTH ||
        layoutReport.clipHeight != DOOMRPG_LOGICAL_HEIGHT ||
        layoutReport.displayWidth == 0 || layoutReport.displayHeight == 0 ||
        layoutReport.displayWidth > layoutReport.clipWidth ||
        layoutReport.displayHeight > layoutReport.clipHeight ||
        layoutReport.screenWidth == 0 || layoutReport.screenHeight == 0 ||
        layoutReport.screenX < layoutReport.displayX ||
        layoutReport.screenY < layoutReport.displayY ||
        layoutReport.screenX + layoutReport.screenWidth >
            layoutReport.displayX + layoutReport.displayWidth ||
        layoutReport.screenY + layoutReport.screenHeight >
            layoutReport.displayY + layoutReport.displayHeight ||
        layoutReport.renderWidth != layoutReport.screenWidth ||
        layoutReport.renderHeight != layoutReport.screenHeight ||
        doomRpg->hud != NULL ||
        render->floorColor == NULL || render->ceilingColor == NULL ||
        render->columnScale == NULL) {
        printf("[LAYOUT] FAILED geometry or Render_setup validation\n");
        layoutReport.ready = 0;
        if (report != NULL) *report = layoutReport;
        return 0;
    }

    layoutReport.ready = 1;
    printf("[LAYOUT] READY real engine layout fits inside 160x120\n");
    printf("[LAYOUT] Hud_t retired; native HUD reads bounded PAK assets on demand\n");
    printf("[LAYOUT] Native EntityDef catalog deferred to resident-map load; Render_startup still NOT executed\n");

    if (report != NULL) *report = layoutReport;
    return 1;
}
