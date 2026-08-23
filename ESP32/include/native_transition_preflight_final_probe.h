#ifndef DOOMRPG_ESP32_NATIVE_TRANSITION_PREFLIGHT_FINAL_PROBE_H
#define DOOMRPG_ESP32_NATIVE_TRANSITION_PREFLIGHT_FINAL_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32TransitionPreflightFinalProbe_reset(void);
int Esp32TransitionPreflightFinalProbe_isDone(void);
void Esp32TransitionPreflightFinalProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
