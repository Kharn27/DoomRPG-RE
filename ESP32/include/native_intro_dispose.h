#ifndef DOOMRPG_ESP32_NATIVE_INTRO_DISPOSE_H
#define DOOMRPG_ESP32_NATIVE_INTRO_DISPOSE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Reset one-shot teardown state when a fresh bounded intro is armed. */
void Esp32IntroDispose_reset(void);

/* Service the single bounded teardown after the validated final-intro PARK.
 * This frees intro-only images/texts and deliberately never loads a map.
 */
void Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

int Esp32IntroDispose_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
