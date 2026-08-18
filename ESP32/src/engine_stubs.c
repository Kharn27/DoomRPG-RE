#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Combat.h"
#include "DoomCanvas.h"
#include "EntityDef.h"
#include "Game.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Render.h"
#include "SDL_Video.h"
#include "Sound.h"
#include "Z_Zone.h"
#include "Z_Zip.h"
#include "engine_metrics.h"
#include "platform_video_config.h"

/* DoomRPG.h defines its original J2ME-style boolean before ESP-IDF brings in
 * the C99 false/true macros. */
#include <esp_heap_caps.h>

/* DoomRPG.c owns the real engine root. The desktop header intentionally does
 * not export it, but the ESP32 bring-up must populate that same root so later
 * increments can continue startup instead of constructing a parallel engine. */
extern DoomRPG_t* doomRpg;

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

Sound_t* Sound_init(Sound_t* sound, DoomRPG_t* doomRpg) {
    if (sound == NULL) sound = SDL_calloc(1, sizeof(Sound_t));
    if (sound != NULL) {
        sound->doomRpg = doomRpg;
        sound->soundEnabled = false;
        sound->volume = 0;
    }
    return sound;
}

void Sound_free(Sound_t* sound, boolean freePtr) { if (freePtr) SDL_free(sound); }
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

/* The diagnostic references this anchor so the linker validates the complete
 * engine call graph without starting it before resources are available. */
uintptr_t DoomRPG_engineLinkAnchor(void) {
    return (uintptr_t)&DoomRPG_Init;
}

void DoomRPG_getEngineMetrics(DoomRpgEngineMetrics* metrics) {
    if (metrics == NULL) return;
    metrics->doomRpg = sizeof(DoomRPG_t);
    metrics->doomCanvas = sizeof(DoomCanvas_t);
    metrics->render = sizeof(Render_t);
    metrics->game = sizeof(Game_t);
    metrics->player = sizeof(Player_t);
    metrics->combat = sizeof(Combat_t);
    metrics->supportObjects = sizeof(Menu_t) + sizeof(MenuSystem_t) +
                              sizeof(Hud_t) + sizeof(Sound_t) +
                              sizeof(EntityDef_t) + sizeof(ParticleSystem_t);
    metrics->totalInitialObjects = metrics->doomRpg + metrics->doomCanvas +
        metrics->render + metrics->game + metrics->player + metrics->combat +
        metrics->supportObjects;
}

static DoomRpgCoreInitReport coreInitReport;
static boolean coreInitAttempted = false;

static uint32_t coreFreeHeap(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t coreLargestBlock(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

const char* DoomRPG_coreStageName(uint8_t stage) {
    static const char* const names[DOOMRPG_CORE_STAGE_COUNT] = {
        "DoomRPG", "DoomCanvas", "Render", "Menu", "MenuSystem", "Hud",
        "Sound", "EntityDef", "Game", "Player", "ParticleSystem", "Combat"
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
    DoomRPG_setDefaultBinds(doomRpg);

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
                     DoomCanvas_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_RENDER, render,
                     Render_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_MENU, menu,
                     Menu_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_MENU_SYSTEM, menuSystem,
                     MenuSystem_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_HUD, hud,
                     Hud_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_SOUND, sound,
                     Sound_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_ENTITY_DEF, entityDef,
                     EntityDef_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_GAME, game,
                     Game_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_PLAYER, player,
                     Player_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_PARTICLE_SYSTEM, particleSystem,
                     ParticleSystem_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_COMBAT, combat,
                     Combat_init(NULL, doomRpg));

#undef INIT_CORE_OBJECT

    coreInitReport.clipWidth = (uint16_t)doomRpg->doomCanvas->clipRect.w;
    coreInitReport.clipHeight = (uint16_t)doomRpg->doomCanvas->clipRect.h;
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
    printf("[CORE] READY objects=%u heap used=%u remaining=%u largest=%u clip=%ux%u\n",
           (unsigned int)coreInitReport.completedStages,
           (unsigned int)coreInitReport.bytesUsed,
           (unsigned int)coreInitReport.heapAfter,
           (unsigned int)coreInitReport.largestBlockAfter,
           coreInitReport.clipWidth, coreInitReport.clipHeight);
    printf("[CORE] Resource startup intentionally NOT executed\n");

    if (report != NULL) *report = coreInitReport;
    return 1;
}
