#ifndef DOOMRPG_ESP32_NATIVE_ENTRANCE_SPAWN_CHAIN_PROBE_H
#define DOOMRPG_ESP32_NATIVE_ENTRANCE_SPAWN_CHAIN_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32EntranceSpawnChainProbe_reset(void);
void Esp32EntranceSpawnChainProbe_service(struct DoomRPG_s* doomRpg);
int Esp32EntranceSpawnChainProbe_isReady(void);

#ifdef __cplusplus
}
#endif

#endif
