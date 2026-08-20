#ifndef DOOMRPG_ESP32_NATIVE_INTRO_FIRST_FRAME_H
#define DOOMRPG_ESP32_NATIVE_INTRO_FIRST_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Render exactly one deterministic fitted ST_INTRO frame after a validated
 * fresh Start Game transition, then hand off to the bounded ESP32 intro clock.
 * This deliberately does not enter DoomCanvas_run(), process intro input,
 * advance story pages or load a gameplay map.
 */
int DoomRPG_esp32RenderFirstIntroFrame(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
