#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_FACING_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_FACING_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionFacingProbe_reset(void);
void Esp32JunctionFacingProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionFacingProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
