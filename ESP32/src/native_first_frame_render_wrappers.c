#include <SDL.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_plane_renderer.h"

void __real_Render_initColumnScale(Render_t* render);
boolean __real_Render_cullBoundingBox(Render_t* render, Node_t* node);

static int nativeFirstFrameContext(const Render_t* render) {
    return render != NULL &&
           !EspNativeFirstFrame_isReady() &&
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

void __wrap_Render_initColumnScale(Render_t* render) {
    __real_Render_initColumnScale(render);
    if (nativeFirstFrameContext(render)) {
        (void)EspNativePlaneRenderer_render(render);
    }
}

boolean __wrap_Render_cullBoundingBox(Render_t* render, Node_t* node) {
    boolean result;
    Line_t savedTmpLine;

    if (!nativeFirstFrameContext(render)) {
        return __real_Render_cullBoundingBox(render, node);
    }

    savedTmpLine = render->tmpLine;
    result = __real_Render_cullBoundingBox(render, node);
    render->tmpLine = savedTmpLine;
    return result;
}
