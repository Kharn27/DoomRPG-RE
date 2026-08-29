#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Permanent native SELECT fallback behind the already-validated tile-event
 * action path. The legacy ordering remains event first, then an 8-tile trace.
 * This owner contains only compact map-local action state; it never imports
 * legacy Entity_t/Player_t/Combat_t ownership.
 */
void EspNativeGameplayActionEngine_reset(void);
int EspNativeGameplayActionEngine_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
