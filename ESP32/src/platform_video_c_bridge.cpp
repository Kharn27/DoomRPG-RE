#include "platform_video.h"
#include "platform_video_c_bridge.h"

extern "C" void* Esp32PlatformVideo_framebuffer(void) {
    return static_cast<void*>(PlatformVideo_framebuffer());
}

extern "C" size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return PlatformVideo_framebufferSizeBytes();
}

extern "C" int Esp32PlatformVideo_present(void) {
    return PlatformVideo_present() ? 1 : 0;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
extern "C" void Esp32PlatformVideo_debugOverlayClear(void) {
    PlatformVideo_debugOverlayClear();
}

extern "C" void Esp32PlatformVideo_debugOverlaySetZone(int index,
                                                        int16_t logicalLeft,
                                                        int16_t logicalTop,
                                                        int16_t logicalRight,
                                                        int16_t logicalBottom) {
    PlatformVideo_debugOverlaySetZone(index,
                                      logicalLeft,
                                      logicalTop,
                                      logicalRight,
                                      logicalBottom);
}

extern "C" void Esp32PlatformVideo_debugOverlayRefresh(void) {
    PlatformVideo_debugOverlayRefresh();
}

extern "C" void Esp32PlatformVideo_debugOverlayMarkTouch(int16_t physicalX,
                                                           int16_t physicalY,
                                                           uint16_t pressure,
                                                           uint16_t rawX,
                                                           uint16_t rawY) {
    PlatformVideo_debugOverlayMarkTouch(physicalX,
                                        physicalY,
                                        pressure,
                                        rawX,
                                        rawY);
}
#endif
