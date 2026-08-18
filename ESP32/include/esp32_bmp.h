#ifndef DOOMRPG_ESP32_BMP_H
#define DOOMRPG_ESP32_BMP_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface* Esp32Bmp_LoadRW(SDL_RWops* source, int freeSource);

#ifdef __cplusplus
}
#endif

#endif
