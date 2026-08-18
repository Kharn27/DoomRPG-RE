#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_C_BRIDGE_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_C_BRIDGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* Esp32PlatformVideo_framebuffer(void);
size_t Esp32PlatformVideo_framebufferSizeBytes(void);

#ifdef __cplusplus
}
#endif

#endif
