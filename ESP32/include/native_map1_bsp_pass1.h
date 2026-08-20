#ifndef DOOMRPG_ESP32_NATIVE_MAP1_BSP_PASS1_H
#define DOOMRPG_ESP32_NATIVE_MAP1_BSP_PASS1_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1BspPass1_reset(void);
void Esp32Map1BspPass1_service(struct DoomRPG_s* doomRpg);
int Esp32Map1BspPass1_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
