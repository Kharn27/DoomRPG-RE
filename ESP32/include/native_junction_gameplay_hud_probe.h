#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_HUD_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_HUD_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGameplayHudProbe_reset(void);
int Esp32JunctionGameplayHudProbe_isDone(void);
void Esp32JunctionGameplayHudProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
