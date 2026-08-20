#ifndef DOOMRPG_ESP32_NATIVE_INTRO_CLOCK_H
#define DOOMRPG_ESP32_NATIVE_INTRO_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP32_INTRO_CLOCK_STEP_MS 50U

/* Arm the bounded ESP32-owned ST_INTRO clock after the validated t=0 frame. */
int Esp32IntroClock_arm(struct DoomRPG_s* doomRpg);

/* Service at most one quantized intro frame from the Arduino loop. */
void Esp32IntroClock_service(void);

int Esp32IntroClock_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
