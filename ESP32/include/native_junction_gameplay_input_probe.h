#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_INPUT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_INPUT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGameplayInputProbe_reset(void);
int Esp32JunctionGameplayInputProbe_isActive(void);
void Esp32JunctionGameplayInputProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
