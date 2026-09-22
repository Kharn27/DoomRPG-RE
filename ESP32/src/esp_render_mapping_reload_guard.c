#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_heap_caps.h>

/*
 * ESP32 mappings are immutable and already installed during startup using the
 * framebuffer-backed bounded inflater. Retain them across legacy menu-map
 * loads; Render_loadMappings() recognizes the complete owner and reuses it.
 */
boolean __real_Render_beginLoadMap(Render_t* render, int mapNameID);

boolean __wrap_Render_beginLoadMap(Render_t* render, int mapNameID) {
    int hadMappings = 0;

    if (render != NULL) {
        hadMappings = render->mediaTexelOffsets != NULL ||
                      render->mediaBitShapeOffsets != NULL ||
                      render->mediaTexturesIds != NULL ||
                      render->mediaSpriteIds != NULL;
        if (hadMappings) {
            printf("[MAPPINGS] RETAIN-BEFORE-MAP map=%d textureCnt=%d spriteCnt=%d heap8=%u largest8=%u immutable=yes\n",
                   mapNameID,
                   render->textureCnt,
                   render->spriteCnt,
                   (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                   (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
    }

    return __real_Render_beginLoadMap(render, mapNameID);
}
