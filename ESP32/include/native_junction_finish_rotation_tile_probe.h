#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_FINISH_ROTATION_TILE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_FINISH_ROTATION_TILE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionFinishRotationTileProbe_reset(void);
void Esp32JunctionFinishRotationTileProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionFinishRotationTileProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
