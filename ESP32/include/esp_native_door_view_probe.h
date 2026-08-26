#ifndef DOOMRPG_ESP32_NATIVE_DOOR_VIEW_PROBE_H
#define DOOMRPG_ESP32_NATIVE_DOOR_VIEW_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/*
 * Diagnostic-only, allocation-free arrival-door view witness.
 *
 * Runs only after the native world renderer has fully unwound. It reuses the
 * already hardware-proven side-effect-free BSP visibility pass, then reports
 * immutable map lines whose logical texture is the legacy entrance marker 7.
 * It does not mutate player orientation, map state, entities, framebuffer
 * pixels, or renderer ownership.
 */
int EspNativeDoorViewProbe_log(struct Render_s* render,
                               uint32_t viewportFNV);

#ifdef __cplusplus
}
#endif

#endif
