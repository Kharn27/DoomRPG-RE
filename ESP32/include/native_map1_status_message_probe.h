#ifndef DOOMRPG_ESP32_NATIVE_MAP1_STATUS_MESSAGE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAP1_STATUS_MESSAGE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1StatusMessageProbe_reset(void);
void Esp32Map1StatusMessageProbe_service(struct DoomRPG_s* doomRpg);
int Esp32Map1StatusMessageProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
