#include "platform_video.h"
#include "platform_video_c_bridge.h"

extern "C" void* Esp32PlatformVideo_framebuffer(void) {
    return static_cast<void*>(PlatformVideo_framebuffer());
}

extern "C" size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return PlatformVideo_framebufferSizeBytes();
}
