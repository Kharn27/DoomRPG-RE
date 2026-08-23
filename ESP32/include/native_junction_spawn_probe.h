#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPAWN_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPAWN_PROBE_H

#include "esp_player_spawn_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionSpawnProbe_reset(void);
void Esp32JunctionSpawnProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionSpawnProbe_isDone(void);
int Esp32JunctionSpawnProbe_getState(EspPlayerSpawnState* outState);

#ifdef __cplusplus
}
#endif

#endif
