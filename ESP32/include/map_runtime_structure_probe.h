#ifndef DOOMRPG_ESP32_MAP_RUNTIME_STRUCTURE_PROBE_H
#define DOOMRPG_ESP32_MAP_RUNTIME_STRUCTURE_PROBE_H

struct Render_s;

#ifdef __cplusplus
extern "C" {
#endif

/* Execute the real menu map structural phase after the BSP plan has passed. */
int DoomRPG_probeMenuMapRuntimeStructures(int menuBspReady);

/* Called only by the ESP32-generated Render.c probe boundary. */
void DoomRPG_markMapRuntimeStructureBoundary(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
