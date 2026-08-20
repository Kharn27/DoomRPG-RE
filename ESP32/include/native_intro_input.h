#ifndef DOOMRPG_ESP32_NATIVE_INTRO_INPUT_H
#define DOOMRPG_ESP32_NATIVE_INTRO_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Arm the released-tap consumer for the bounded ST_INTRO progression. */
int Esp32IntroInput_arm(struct DoomRPG_s* doomRpg);

/* Drop ownership of the platform tap callback without touching intro assets. */
void Esp32IntroInput_disarm(void);

int Esp32IntroInput_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
