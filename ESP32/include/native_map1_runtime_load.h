#ifndef DOOMRPG_ESP32_NATIVE_MAP1_RUNTIME_LOAD_H
#define DOOMRPG_ESP32_NATIVE_MAP1_RUNTIME_LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1RuntimeLoad_reset(void);
void Esp32Map1RuntimeLoad_service(struct DoomRPG_s* doomRpg);
int Esp32Map1RuntimeLoad_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
