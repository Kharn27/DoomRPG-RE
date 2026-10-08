#include <SDL.h>
#include <stdio.h>
#include "DoomRPG.h"
#include "Render.h"

/* Production compatibility-only: native world/BSP/planes own all live
 * operations. The desktop definitions remain available to bringup probes.
 */
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_BRINGUP_PROBES)
/* Compatibility-only: production has no Render.tileEvents allocation.
 * Native event lookup is owned by EspMapRuntime and EspMapState.
 * Fail closed even if a legacy caller unexpectedly reaches this ABI.
 */
int Render_findEventIndex(Render_t* render, int tile)
{
    (void)render;
    (void)tile;
    return -1;
}

boolean Render_beginLoadMap(Render_t* r, int id) {
    (void)r; (void)id;
    printf("[LEGACYMAP] REJECT Render_beginLoadMap: native BSP owner required\n");
    return false;
}
boolean Render_beginLoadMapData(Render_t* r) {
    (void)r;
    printf("[LEGACYMAP] REJECT Render_beginLoadMapData: native BSP owner required\n");
    return false;
}
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
void Render_renderBSP(Render_t* r) {
    (void)r;
    printf("[LEGACYBSP] REJECT Render_renderBSP: native visibility required\n");
}
void Render_walkNode(Render_t* r, int i) {
    (void)r; (void)i;
    printf("[LEGACYBSP] REJECT Render_walkNode: native BSP owner required\n");
}
#endif
