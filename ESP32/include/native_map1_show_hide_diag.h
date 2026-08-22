#ifndef DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_DIAG_H
#define DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1ShowHideDiag_reset(void);
void Esp32Map1ShowHideDiag_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
