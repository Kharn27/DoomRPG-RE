#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_TURN_DISPATCH_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_TURN_DISPATCH_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionTurnDispatchProbe_reset(void);
int Esp32JunctionTurnDispatchProbe_isActive(void);
void Esp32JunctionTurnDispatchProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
