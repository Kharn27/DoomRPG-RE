#ifndef DOOMRPG_ESP32_NATIVE_PLANE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_PLANE_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativePlaneRenderStats_s {
    uint32_t rowsRendered;
    uint32_t pixelsRendered;
    uint32_t cacheHits;
    uint32_t cacheMisses;
    uint32_t cacheEvictions;
    uint32_t texelReadBytes;
    uint16_t uniqueLogicalTextures;
    uint8_t rendered;
    uint8_t active;
} EspNativePlaneRenderStats;

void EspNativePlaneRenderer_reset(void);
/* The renderer preserves exact sampling with an adaptive bounded LRU: up to
 * six 2048-byte transient leases are used when available, with one slot as the
 * fail-closed minimum under later gameplay memory pressure. */
int EspNativePlaneRenderer_render(struct Render_s* render);
const EspNativePlaneRenderStats* EspNativePlaneRenderer_view(void);

#ifdef __cplusplus
}
#endif

#endif