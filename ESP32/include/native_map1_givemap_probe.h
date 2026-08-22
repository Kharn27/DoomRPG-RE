#ifndef DOOMRPG_ESP32_NATIVE_MAP1_GIVEMAP_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAP1_GIVEMAP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1GiveMapProbe_reset(void);
void Esp32Map1GiveMapProbe_service(struct DoomRPG_s* doomRpg);
int Esp32Map1GiveMapProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
