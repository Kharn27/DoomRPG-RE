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

/* Bring-up-only diagnostics drawn directly on the physical TFT after the game
 * framebuffer has been presented. These never modify the 160x120 framebuffer,
 * so deterministic framebuffer hashes remain valid.
 */
void PlatformVideo_debugOverlayClear();
void PlatformVideo_debugOverlaySetZone(int index,
                                       int16_t logicalLeft,
                                       int16_t logicalTop,
                                       int16_t logicalRight,
                                       int16_t logicalBottom);
void PlatformVideo_debugOverlayRefresh();
void PlatformVideo_debugOverlayMarkTouch(int16_t physicalX,
                                         int16_t physicalY,
                                         uint16_t pressure,
                                         uint16_t rawX,
                                         uint16_t rawY);

#endif
