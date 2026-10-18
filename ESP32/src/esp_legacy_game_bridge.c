#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Game.h"

/*
 * Permanent ESP32 compatibility bridge for the last live Game_init root.
 * Desktop Game.c is not compiled. The two inert Game_activate and
 * Game_unloadMapData exports are intentionally absent: any attempted
 * resurrection of legacy world/activation ownership must fail to link.
 *
 * World/entity/script/gameplay ownership remains native. The one-pointer
 * Game_t process shell is retired in a separate lifecycle milestone.
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

