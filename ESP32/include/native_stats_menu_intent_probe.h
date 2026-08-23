#ifndef DOOMRPG_ESP32_NATIVE_STATS_MENU_INTENT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_STATS_MENU_INTENT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32StatsMenuIntentProbe_reset(void);
void Esp32StatsMenuIntentProbe_service(struct DoomRPG_s* doomRpg);
int Esp32StatsMenuIntentProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
