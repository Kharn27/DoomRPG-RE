#ifndef DOOMRPG_ESP32_NATIVE_RESIDENT_GAMEPLAY_H
#define DOOMRPG_ESP32_NATIVE_RESIDENT_GAMEPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Production-oriented resident-map gameplay loop over the permanent native
 * input/dispatch/render owners. The touch callback only queues a semantic
 * intent; all view mutation, collision and rendering happen later from service.
 */
void EspNativeResidentGameplay_reset(void);
/*
 * One-shot admission for a durable checkpoint resume. The normal fresh-map
 * path still requires EspNativeFirstFrame_isReady(). Resume instead requires
 * the already-restored HUD plus a fully primed resident/large cache before
 * installing input. The admission bit is consumed on activation.
 */
int EspNativeResidentGameplay_armCheckpointResume(void);
void EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpg);
int EspNativeResidentGameplay_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
