#ifndef DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_FINAL_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_FINAL_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1ShowHideFinalProbe_reset(void);
void Esp32Map1ShowHideFinalProbe_service(struct DoomRPG_s* doomRpg);
int Esp32Map1ShowHideFinalProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
