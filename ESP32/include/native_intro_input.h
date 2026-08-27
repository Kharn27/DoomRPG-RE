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

/* Called only after the intro clock has flushed a completed framebuffer. The
 * final Continue path is not allowed to dispose the intro until at least one
 * full final-text frame has reached the display. */
void Esp32IntroInput_notifyFramePresented(void);

/* Drop ownership of the platform tap callback without touching intro assets. */
void Esp32IntroInput_disarm(void);

int Esp32IntroInput_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
