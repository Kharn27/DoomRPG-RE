#include <SDL.h>
#include <stdio.h>
#include "DoomRPG.h"
#include "Render.h"

/* Production compatibility-only: native world/planes own live rendering.
 * These four legacy raster entrypoints remain deliberate bounded no-ops
 * or explicit rejects; their separate retirement needs its own milestone.
 * Desktop Render.c is not compiled into ESP32 firmware.
 */
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_BRINGUP_PROBES)
void Render_render(Render_t* r, int x, int y, int z, unsigned int angle) {
    (void)r; (void)x; (void)y; (void)z; (void)angle;
    printf("[LEGACYRENDER] REJECT Render_render: native world renderer required\n");
}
void Render_renderFloorAndCeilingBG(Render_t* r) {
    (void)r;
    printf("[LEGACYRENDER] REJECT Render_renderFloorAndCeilingBG: native planes required\n");
}
void Render_drawplane(Render_t* r, int x, int y, PlaneTextureRef_t* p, int n) {
    (void)r; (void)x; (void)y; (void)p; (void)n;
}
void Render_spanPlane(Render_t* r, int x, int y, PlaneTextureRef_t* p,
                      int a, int b, int c, int d, int n) {
    (void)r; (void)x; (void)y; (void)p;
    (void)a; (void)b; (void)c; (void)d; (void)n;
}
#endif
