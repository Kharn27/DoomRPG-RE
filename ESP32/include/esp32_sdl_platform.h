#ifndef DOOMRPG_ESP32_SDL_PLATFORM_H
#define DOOMRPG_ESP32_SDL_PLATFORM_H

class TFT_eSPI;

void Esp32Sdl_attachDisplay(TFT_eSPI* display);
void Esp32Sdl_showTestPattern();

#endif
