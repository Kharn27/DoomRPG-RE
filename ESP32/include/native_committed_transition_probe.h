#ifndef DOOMRPG_ESP32_NATIVE_COMMITTED_TRANSITION_PROBE_H
#define DOOMRPG_ESP32_NATIVE_COMMITTED_TRANSITION_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32CommittedTransitionProbe_reset(void);
void Esp32CommittedTransitionProbe_service(struct DoomRPG_s* doomRpg);
int Esp32CommittedTransitionProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
