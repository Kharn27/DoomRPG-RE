#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_HUD_REFRESH_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_HUD_REFRESH_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionHudRefreshProbe_reset(void);
void Esp32JunctionHudRefreshProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionHudRefreshProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
