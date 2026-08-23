#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYER_VIEW_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYER_VIEW_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPlayerViewProbe_reset(void);
void Esp32JunctionPlayerViewProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionPlayerViewProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
