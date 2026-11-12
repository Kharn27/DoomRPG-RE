#ifndef DOOMRPG_ESP32_SDL_MIXER_H
#define DOOMRPG_ESP32_SDL_MIXER_H

/* On the CYD, Sound.h only needs this opaque pointer type, not SDL's
 * entire compatibility header. Preserve the host/geometry-oracle SDL ABI. */
#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif

typedef struct Mix_Chunk Mix_Chunk;

#endif
