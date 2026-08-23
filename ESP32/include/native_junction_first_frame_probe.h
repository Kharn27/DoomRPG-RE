#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_FIRST_FRAME_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_FIRST_FRAME_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionFirstFrameProbe_reset(void);
void Esp32JunctionFirstFrameProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionFirstFrameProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
