#ifndef DOOMRPG_ESP32_NATIVE_INTRO_INPUT_H
#define DOOMRPG_ESP32_NATIVE_INTRO_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Arm the semantic press-edge tap consumer for bounded ST_INTRO progression.
 * PlatformInput requires a stable release before another tap can be emitted.
 */
int Esp32IntroInput_arm(struct DoomRPG_s* doomRpg);

/* Drop ownership of the platform tap callback without touching intro assets. */
void Esp32IntroInput_disarm(void);

int Esp32IntroInput_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
