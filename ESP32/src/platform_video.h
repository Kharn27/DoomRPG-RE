#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_H

#include <stddef.h>
#include <stdint.h>

class TFT_eSPI;

bool PlatformVideo_begin(TFT_eSPI* display);
uint16_t* PlatformVideo_framebuffer();
size_t PlatformVideo_framebufferSizeBytes();
void PlatformVideo_clear(uint16_t color);
bool PlatformVideo_present();
void PlatformVideo_showTestPattern();

#endif
