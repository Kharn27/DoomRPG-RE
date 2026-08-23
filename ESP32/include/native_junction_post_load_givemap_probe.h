#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_GIVEMAP_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_GIVEMAP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadGiveMapProbe_reset(void);
void Esp32JunctionPostLoadGiveMapProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionPostLoadGiveMapProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
