#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_PLAYING_TRANSITION_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_PLAYING_TRANSITION_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadPlayingTransitionProbe_reset(void);
int Esp32JunctionPostLoadPlayingTransitionProbe_isDone(void);
void Esp32JunctionPostLoadPlayingTransitionProbe_service(
    struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
