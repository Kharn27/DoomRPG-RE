#ifndef DOOMRPG_ESP32_NATIVE_INTRO_FIRST_FRAME_H
#define DOOMRPG_ESP32_NATIVE_INTRO_FIRST_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Render exactly one deterministic ST_INTRO frame after a validated fresh
 * Start Game transition. This deliberately does not enter DoomCanvas_run(),
 * does not process input, does not advance story pages and does not load a map.
 * The frame is rendered at intro-local t=0, presented once, instrumented and
 * then left parked for hardware validation.
 */
int DoomRPG_esp32RenderFirstIntroFrame(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
