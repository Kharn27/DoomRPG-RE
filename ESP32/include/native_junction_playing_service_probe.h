#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYING_SERVICE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_PLAYING_SERVICE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPlayingServiceProbe_reset(void);
int Esp32JunctionPlayingServiceProbe_isDone(void);
void Esp32JunctionPlayingServiceProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
