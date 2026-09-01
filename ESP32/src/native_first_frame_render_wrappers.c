#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_plane_renderer.h"

void __real_Render_initColumnScale(Render_t* render);
boolean __real_Render_cullBoundingBox(Render_t* render, Node_t* node);

/* Compact native world rendering is identified by its permanent architecture
 * boundary, not by whether the historical first-frame owner happens to be
 * ready.  The gameplay viewport route deliberately leaves that historical
 * owner untouched, but it still needs the same native plane injection and
 * tmpLine preservation as the hardware-proven boot route. */
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

static uint32_t elapsedMicros(int64_t start) {
    int64_t elapsed = esp_timer_get_time() - start;
    if (elapsed <= 0) return 0U;
    if ((uint64_t)elapsed > UINT32_MAX) return UINT32_MAX;
    return (uint32_t)elapsed;
}

void __wrap_Render_initColumnScale(Render_t* render) {
    __real_Render_initColumnScale(render);
    if (nativeCompactWorldContext(render)) {
        int64_t start;
        uint32_t micros;
        int ok;

        /* Keep this probe deliberately stack-light. The earlier resident PAK
         * before/after snapshots cost 2 * sizeof(EspAssetPackResidentStats)
         * on this already-deep renderer call chain and the real CYD exposed a
         * loopTask stack canary while regular-door animation added one more
         * render wrapper layer. Storage correlation is already hardware-proven;
         * this probe now owns only scalar timing state and changes no heap/BSS. */
        start = esp_timer_get_time();
        ok = EspNativePlaneRenderer_render(render);
        micros = elapsedMicros(start);

        /* Capture time before printing so this diagnostic line is not charged
         * to the measured plane phase. The existing NATIVEPLANE line remains
         * inside EspNativePlaneRenderer_render() and is therefore part of the
         * current production cost being audited. */
        printf("[PLANEPROFILE] us=%u ok=%u\n",
               (unsigned int)micros,
               (unsigned int)(ok != 0));
    }
}

boolean __wrap_Render_cullBoundingBox(Render_t* render, Node_t* node) {
    boolean result;
    Line_t savedTmpLine;

    if (!nativeCompactWorldContext(render)) {
        return __real_Render_cullBoundingBox(render, node);
    }

    savedTmpLine = render->tmpLine;
    result = __real_Render_cullBoundingBox(render, node);
    render->tmpLine = savedTmpLine;
    return result;
}
