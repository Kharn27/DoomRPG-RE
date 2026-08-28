#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_heap_caps.h>

/*
 * Legacy Render_beginLoadMap() always calls Render_loadMappings().  On desktop
 * that routine opens/decompresses mappings.bin first and only then frees the
 * previous four mapping arrays.  On the no-PSRAM classic CYD this creates an
 * avoidable peak: resident mappings coexist with compressed data, the 8392-byte
 * inflated blob and the ~11 KiB tinfl state.
 *
 * Keep Render.c behavior and file format untouched.  Immediately before the
 * real map-load entry point, release only the four immutable mapping arrays it
 * is about to rebuild anyway.  The real Render_loadMappings() then sees NULL
 * pointers, reloads the exact same data, and remains the sole owner/parser.
 */
boolean __real_Render_beginLoadMap(Render_t* render, int mapNameID);

boolean __wrap_Render_beginLoadMap(Render_t* render, int mapNameID) {
    uint32_t heapBefore = 0U;
    uint32_t largestBefore = 0U;
    int hadMappings = 0;

    if (render != NULL) {
        hadMappings = render->mediaTexelOffsets != NULL ||
                      render->mediaBitShapeOffsets != NULL ||
                      render->mediaTexturesIds != NULL ||
                      render->mediaSpriteIds != NULL;
        if (hadMappings) {
            heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
            largestBefore =
                (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        }

        if (render->mediaTexelOffsets != NULL) {
            SDL_free(render->mediaTexelOffsets);
            render->mediaTexelOffsets = NULL;
        }
        if (render->mediaBitShapeOffsets != NULL) {
            SDL_free(render->mediaBitShapeOffsets);
            render->mediaBitShapeOffsets = NULL;
        }
        if (render->mediaTexturesIds != NULL) {
            SDL_free(render->mediaTexturesIds);
            render->mediaTexturesIds = NULL;
        }
        if (render->mediaSpriteIds != NULL) {
            SDL_free(render->mediaSpriteIds);
            render->mediaSpriteIds = NULL;
        }

        if (hadMappings) {
            printf("[MAPPINGS] RELEASE-BEFORE-MAP map=%d textureCnt=%d spriteCnt=%d heap8=%u->%u largest8=%u->%u reason=bound-inflate-peak\n",
                   mapNameID,
                   render->textureCnt,
                   render->spriteCnt,
                   (unsigned int)heapBefore,
                   (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                   (unsigned int)largestBefore,
                   (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
    }

    return __real_Render_beginLoadMap(render, mapNameID);
}
