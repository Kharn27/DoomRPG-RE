#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Game.h"

/*
 * Permanent ESP32 compatibility bridge for the three Game_* roots that still
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

void Game_unloadMapData(Game_t* game)
{
    /*
     * Native resident/session owners perform the real teardown. The minimal
     * ESP32 Game_t and compact DoomCanvas_t carry no world/entity mirrors.
     */
    (void)game;
}
