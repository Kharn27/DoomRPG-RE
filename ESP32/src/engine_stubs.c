#include <SDL.h>

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
