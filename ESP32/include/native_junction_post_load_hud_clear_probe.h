#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_HUD_CLEAR_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_HUD_CLEAR_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadHudClearProbe_reset(void);
void Esp32JunctionPostLoadHudClearProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionPostLoadHudClearProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
