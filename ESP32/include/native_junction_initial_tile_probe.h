#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_INITIAL_TILE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_INITIAL_TILE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionInitialTileProbe_reset(void);
void Esp32JunctionInitialTileProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionInitialTileProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
