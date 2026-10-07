#include <SDL.h>
#include <stdio.h>

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
#include "esp_native_audio_intent.h"
#include "esp_native_menu_storage.h"
#include "platform_video_config.h"

/* DoomRPG.h defines its original J2ME-style boolean before ESP-IDF brings in
 * the C99 false/true macros. */
#include <esp_heap_caps.h>

#ifdef DOOMRPG_ESP32
/* The normal classic-CYD build must never silently regain the 35 KB desktop
 * entity stores. These are compile-only sentinels; native owners hold runtime
 * entity/monster/topology state. */
_Static_assert(GAME_LEGACY_ENTITY_CAPACITY == 1,
               "ESP32 Game_t legacy entity storage must stay compact");
_Static_assert(GAME_LEGACY_ENTITY_DB_CAPACITY == 1,
               "ESP32 Game_t legacy entityDb storage must stay compact");
_Static_assert(GAME_LEGACY_MONSTER_CAPACITY == 1,
               "ESP32 Game_t legacy monster storage must stay compact");
_Static_assert(sizeof(Game_t) == 1296,
               "ESP32 Game_t layout changed; re-audit live compatibility fields");
#endif

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
    metrics->game = sizeof(Game_t);
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

    DoomRPG_setDefaultBinds(doomRpg);
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
                     DoomCanvas_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_RENDER, render,
                     Render_init(NULL, doomRpg));
    INIT_CORE_OBJECT(DOOMRPG_CORE_MENU_SYSTEM, menuSystem,
                     EspNativeMenuStorage_init(NULL));
    INIT_CORE_OBJECT(DOOMRPG_CORE_GAME, game,
                     Game_init(NULL, doomRpg));

#undef INIT_CORE_OBJECT

    /*
     * The legacy Game shell is temporarily retained for config/teardown ABI,
     * but its embedded Entity/EntityMonster stores are retired runtime owners.
     * They must remain empty; native resident-map state owns all live entities.
     */
    if (doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->game->activeMonsters != NULL ||
        doomRpg->game->inactiveMonsters != NULL ||
        doomRpg->game->combatMonsters != NULL ||
        doomRpg->game->spawnMonster != NULL) {
        coreInitReport.failedStage = DOOMRPG_CORE_GAME;
        coreInitReport.heapAfter = coreFreeHeap();
        coreInitReport.largestBlockAfter = coreLargestBlock();
        coreInitReport.ready = 0;
        printf("[CORE] FAILED legacy entity runtime not dormant entities=%d monsters=%d active=%p inactive=%p combat=%p spawn=%p\n",
               doomRpg->game->numEntities,
               doomRpg->game->numMonsters,
               (void*)doomRpg->game->activeMonsters,
               (void*)doomRpg->game->inactiveMonsters,
               (void*)doomRpg->game->combatMonsters,
               (void*)doomRpg->game->spawnMonster);
        if (report != NULL) *report = coreInitReport;
        return 0;
    }
    printf("[CORE] Legacy entity runtime retired stores=sentinel capacities=%u/%u/%u gameBytes=%u desktopBytes=36468 reclaimed=35172 entities=0 monsters=0 owner=native-resident-map\n",
           (unsigned int)GAME_LEGACY_ENTITY_CAPACITY,
           (unsigned int)GAME_LEGACY_ENTITY_DB_CAPACITY,
           (unsigned int)GAME_LEGACY_MONSTER_CAPACITY,
           (unsigned int)sizeof(Game_t));

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

    DoomCanvas_startup(canvas);

    printf("[LAYOUT] HUD owner=native-gameplay-hud legacyObject=%p top=20 bottom=20\n",
           (void*)doomRpg->hud);

    layoutReport.heap8After = coreFreeHeap();
    layoutReport.largest8After = coreLargestBlock();
    layoutReport.bytesUsed = layoutReport.heap8Before >= layoutReport.heap8After
        ? layoutReport.heap8Before - layoutReport.heap8After : 0;

    layoutReport.clipX = (int16_t)canvas->clipRect.x;
    layoutReport.clipY = (int16_t)canvas->clipRect.y;
    layoutReport.clipWidth = (uint16_t)canvas->clipRect.w;
    layoutReport.clipHeight = (uint16_t)canvas->clipRect.h;

    layoutReport.displayX = (int16_t)canvas->displayRect.x;
    layoutReport.displayY = (int16_t)canvas->displayRect.y;
    layoutReport.displayWidth = (uint16_t)canvas->displayRect.w;
    layoutReport.displayHeight = (uint16_t)canvas->displayRect.h;

    layoutReport.screenX = (int16_t)canvas->screenRect.x;
    layoutReport.screenY = (int16_t)canvas->screenRect.y;
    layoutReport.screenWidth = (uint16_t)canvas->screenRect.w;
    layoutReport.screenHeight = (uint16_t)canvas->screenRect.h;

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
