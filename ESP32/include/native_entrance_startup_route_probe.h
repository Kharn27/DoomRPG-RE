#ifndef DOOMRPG_ESP32_NATIVE_ENTRANCE_STARTUP_ROUTE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_ENTRANCE_STARTUP_ROUTE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32EntranceStartupRouteProbe_reset(void);
void Esp32EntranceStartupRouteProbe_service(struct DoomRPG_s* doomRpg);
int Esp32EntranceStartupRouteProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
