#ifndef DOOMRPG_ESP32_NATIVE_MAP1_DIALOG_OWNER_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAP1_DIALOG_OWNER_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1DialogOwnerProbe_reset(void);
void Esp32Map1DialogOwnerProbe_service(struct DoomRPG_s* doomRpg);
int Esp32Map1DialogOwnerProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
