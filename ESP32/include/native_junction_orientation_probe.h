#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_ORIENTATION_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_ORIENTATION_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionOrientationProbe_reset(void);
void Esp32JunctionOrientationProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionOrientationProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
