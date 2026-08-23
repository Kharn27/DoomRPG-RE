#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_IDLE_TIME_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_IDLE_TIME_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadIdleTimeProbe_reset(void);
int Esp32JunctionPostLoadIdleTimeProbe_isDone(void);
void Esp32JunctionPostLoadIdleTimeProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
