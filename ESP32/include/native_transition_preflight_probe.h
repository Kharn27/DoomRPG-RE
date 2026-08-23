#ifndef DOOMRPG_ESP32_NATIVE_TRANSITION_PREFLIGHT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_TRANSITION_PREFLIGHT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32TransitionPreflightProbe_reset(void);
int Esp32TransitionPreflightProbe_isDone(void);
void Esp32TransitionPreflightProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
