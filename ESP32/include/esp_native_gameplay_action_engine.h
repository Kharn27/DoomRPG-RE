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

/*
 * esp_native_gameplay_present_gate.c remains the sole linker --wrap owner for
 * Esp32PlatformVideo_present().  The action implementation historically names
 * its local leaf as a wrapper; rename that one translation-unit symbol through
 * this header so the gate can chain into it without creating a second linker
 * wrapper.  Other users of this header never reference the private token.
 */
int EspNativeGameplayActionEngine_present(void);
#define __wrap_Esp32PlatformVideo_present EspNativeGameplayActionEngine_present

#ifdef __cplusplus
}
#endif

#endif
