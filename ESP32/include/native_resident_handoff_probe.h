#ifndef DOOMRPG_ESP32_NATIVE_RESIDENT_HANDOFF_PROBE_H
#define DOOMRPG_ESP32_NATIVE_RESIDENT_HANDOFF_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32ResidentHandoffProbe_reset(void);
void Esp32ResidentHandoffProbe_service(struct DoomRPG_s* doomRpg);
int Esp32ResidentHandoffProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
