/* BT-1/B isolated test-only compatibility boundary.
 * DoomRPG.h uses the C-only enum identifiers "false"/"true"; do not
 * include native root/Render ABI headers from the C++ Arduino entrypoint. */
#if defined(DOOMRPG_ESP32_BT_TEST_PROFILE) && DOOMRPG_ESP32_BT_TEST_PROFILE
#include <stddef.h>
#include "esp_native_root_state.h"
#include "Render.h"
#include "esp_render_startup_bridge.h"
#include "platform_video_c_bridge.h"

/* Only after the native MENU_MAIN has been presented once into ILI9341
 * display GRAM. The real game's permanent frame buffer is never freed. */
int EspBtTest_detachRenderForFreeze(void) {
    EspNativeRootOwners_t *owners = EspNativeRootOwners_live();
    Render_t *render = owners ? owners->render : NULL;
    void *platformFramebuffer = Esp32PlatformVideo_framebuffer();
    if (!render || !platformFramebuffer ||
        (void *)render->framebuffer != platformFramebuffer) {
        return 0;
    }
    /* Shared native compatibility-slot disposer owns mapping arrays, not FB.
     * The next operation (C++ PlatformVideo_btTestReleaseFrozenFramebuffer)
     * is exclusively test-scoped and frees the 38400-byte screen buffer. */
    EspNativeRenderRuntime_releaseCompatibility(render);
    render->framebuffer = NULL;
    return 1;
}
#endif
