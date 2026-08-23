#ifndef DOOMRPG_ESP32_NATIVE_PLAYER_EXIT_STATE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_PLAYER_EXIT_STATE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32PlayerExitStateProbe_reset(void);
void Esp32PlayerExitStateProbe_service(struct DoomRPG_s* doomRpg);
int Esp32PlayerExitStateProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
