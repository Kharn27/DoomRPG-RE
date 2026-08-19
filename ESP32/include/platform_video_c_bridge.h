#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_C_BRIDGE_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_C_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* Esp32PlatformVideo_framebuffer(void);
size_t Esp32PlatformVideo_framebufferSizeBytes(void);

void Esp32PlatformVideo_debugOverlayClear(void);
void Esp32PlatformVideo_debugOverlaySetZone(int index,
                                            int16_t logicalLeft,
                                            int16_t logicalTop,
                                            int16_t logicalRight,
                                            int16_t logicalBottom);
void Esp32PlatformVideo_debugOverlayRefresh(void);
void Esp32PlatformVideo_debugOverlayMarkTouch(int16_t physicalX,
                                              int16_t physicalY,
                                              uint16_t pressure,
                                              uint16_t rawX,
                                              uint16_t rawY);

#ifdef __cplusplus
}
#endif

#endif
