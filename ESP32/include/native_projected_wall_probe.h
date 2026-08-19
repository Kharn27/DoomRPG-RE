#ifndef DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_PROBE_H
#define DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

int DoomRPG_probeProjectedWallGfxrm(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
