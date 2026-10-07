#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "SDL_Video.h"

#define ESP_LEGACY_CONFIG_VERSION 23

/*
 * Permanent ESP32 compatibility bridge for the four Game_* roots that still
 * survive in the normal classic-CYD ELF. Desktop Game.c is not compiled.
 *
 * World/entity/script/gameplay ownership is native. Adding a new legacy Game_*
 * dependency must fail at link time and receive an explicit migration milestone.
 */

Game_t* Game_init(Game_t* game, DoomRPG_t* doomRpg)
{
    printf("Game_init\n");

    if (game == NULL) {
        game = (Game_t*)SDL_malloc(sizeof(Game_t));
        if (game == NULL) {
            return NULL;
        }
    }

    SDL_memset(game, 0, sizeof(Game_t));
    game->doomRpg = doomRpg;
    return game;
}

void Game_activate(Game_t* game, Entity_t* entity)
{
    /* Native monster activation owns this behavior. Keep stale ABI fail-closed. */
    (void)game;
    (void)entity;
}

void Game_loadConfig(Game_t* game)
{
    SDL_RWops* rw;
    int version;
    byte boolData;
    int intData;

    printf("loadConfig\n");

    rw = SDL_RWFromFile("Config", "r");
    if (rw != NULL) {
        version = File_readInt(rw);
        if (version == ESP_LEGACY_CONFIG_VERSION) {
            boolData = File_readByte(rw);
            if (game != NULL) {
                game->doomRpg->doomCanvas->vibrateEnabled =
                    boolData != 0 ? true : false;
            }

            /* Retired Sound_t volume: consume for stream compatibility. */
            intData = File_readInt(rw);
            (void)intData;

            intData = File_readInt(rw);
            if (game != NULL) {
                DoomCanvas_setAnimFrames(game->doomRpg->doomCanvas, intData);
            }

            /* Retired Player_t totalDeaths: consume for stream compatibility. */
            intData = File_readInt(rw);
            (void)intData;

            boolData = File_readByte(rw);
            sdlVideo.fullScreen = boolData != 0 ? true : false;

            boolData = File_readByte(rw);
            sdlVideo.vSync = boolData != 0 ? true : false;

            boolData = File_readByte(rw);
            sdlVideo.integerScaling = boolData != 0 ? true : false;

            boolData = File_readByte(rw);
            sdlVideo.displaySoftKeys = boolData != 0 ? true : false;

            sdlVideo.resolutionIndex = File_readInt(rw);

            intData = File_readInt(rw);
            if (game != NULL) {
                game->doomRpg->doomCanvas->mouseSensitivity = intData;
            }

            boolData = File_readByte(rw);
            if (game != NULL) {
                game->doomRpg->doomCanvas->mouseYMove =
                    boolData != 0 ? true : false;
            }

            sdlController.deadZoneLeft = File_readInt(rw);
            sdlController.deadZoneRight = File_readInt(rw);

            boolData = File_readByte(rw);
            if (game != NULL) {
                game->doomRpg->doomCanvas->sndPriority =
                    boolData != 0 ? true : false;
            }

            boolData = File_readByte(rw);
            if (game != NULL) {
                game->doomRpg->doomCanvas->renderFloorCeilingTextures =
                    boolData != 0 ? true : false;
            }

            if (game != NULL) {
                for (int i = 0; i < 12; ++i) {
                    for (int j = 0; j < KEYBINDS_MAX; ++j) {
                        keyMapping[i].keyBinds[j] = File_readInt(rw);
                    }
                }
                SDL_memcpy(keyMappingTemp, keyMapping, sizeof(keyMapping));
            }
        }
        else {
            printf("loadConfig: save version mismatch (expected %d found %d)\n",
                   ESP_LEGACY_CONFIG_VERSION, version);
        }
    }
    else {
        printf("loadConfig: (%s)\n", SDL_GetError());
    }

    if (rw != NULL) {
        SDL_RWclose(rw);
    }
}

void Game_unloadMapData(Game_t* game)
{
    if (game == NULL) {
        return;
    }

    /*
     * Native resident/session owners perform the real teardown. The minimal
     * ESP32 Game_t has no world/entity/transient storage left to clear.
     */
    if (game->doomRpg != NULL && game->doomRpg->doomCanvas != NULL) {
        DoomCanvas_t* canvas = game->doomRpg->doomCanvas;
        for (int i = 0; i < 8; ++i) {
            canvas->openDoors[i] = NULL;
        }
        canvas->openDoorsCount = 0;
        canvas->castEntity = NULL;
    }
}
