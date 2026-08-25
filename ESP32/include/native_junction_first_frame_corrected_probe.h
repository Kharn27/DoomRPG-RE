#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_FIRST_FRAME_CORRECTED_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_FIRST_FRAME_CORRECTED_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionFirstFrameCorrectedProbe_reset(void);
void Esp32JunctionFirstFrameCorrectedProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionFirstFrameCorrectedProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
