#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "doomrpg_log.h"
#include "esp_map_runtime.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_plane_renderer.h"

/* Compact native world rendering is identified by its permanent architecture
 * boundary, not by whether the historical first-frame owner happens to be
 * ready.  The gameplay viewport route deliberately leaves that historical
 * owner untouched, but it still needs the same explicit native plane
 * injection as the hardware-proven boot route. BSP culling now preserves
 * tmpLine at its own native call sites. */
static int nativeCompactWorldContext(const Render_t* render) {
    return render != NULL &&
           EspMapRuntime_isLoaded() &&
           EspNativeGraphicsCatalog_isReady() &&
           EspAssetPack_isOpen() &&
           render->framebuffer != NULL && render->columnScale != NULL &&
           render->screenWidth == 160 && render->screenHeight == 80 &&
           render->screenLeft == 0 && render->screenTop == 0 &&
           render->screenRight == 160 && render->screenBottom == 80 &&
           render->lines == NULL && render->nodes == NULL &&
           render->mapSprites == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL;
}

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
static uint32_t elapsedMicros(int64_t start) {
    int64_t elapsed = esp_timer_get_time() - start;
    if (elapsed <= 0) return 0U;
    if ((uint64_t)elapsed > UINT32_MAX) return UINT32_MAX;
    return (uint32_t)elapsed;
}
#endif

int EspNativeWorldPlane_renderAtColumnReset(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    int ok;

    if (!nativeCompactWorldContext(render)) return 0;

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    int64_t start;
    uint32_t micros;

    /* Keep the old profile point and its stack-light scalar-only footprint.
     * Previously this ran as a hidden side effect of an ld --wrap call.
     * It is now an explicit render-stage dependency. */
    start = esp_timer_get_time();
    ok = EspNativePlaneRenderer_render(render);
    micros = elapsedMicros(start);
    DRPG_LOGT("[PLANEPROFILE] us=%u ok=%u\n",
              (unsigned int)micros, (unsigned int)(ok != 0));
#else
    ok = EspNativePlaneRenderer_render(render);
#endif
    return ok;
}

