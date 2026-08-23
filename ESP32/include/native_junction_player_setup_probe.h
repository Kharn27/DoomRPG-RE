#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYER_SETUP_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYER_SETUP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPlayerSetupProbe_reset(void);
void Esp32JunctionPlayerSetupProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionPlayerSetupProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
