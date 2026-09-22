#ifndef DOOMRPG_ESP32_NATIVE_RESIDENT_GAMEPLAY_H
#define DOOMRPG_ESP32_NATIVE_RESIDENT_GAMEPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;
struct Render_s;

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
int EspNativeResidentGameplay_isAutomapActive(void);

/* Repaint the currently-owned Automap after a semantic world/player mutation.
 * Returns 0 when Automap is not active or presentation fails. */
int EspNativeResidentGameplay_redrawAutomap(
    struct Render_s* render,
    const char* reason);

/* Legacy Player_pain leaves ST_AUTOMAP before showing damage. This closes the
 * native Automap to the normal HUD/world presentation when damage commits. */
/* Close Automap before a modal presenter takes framebuffer/input ownership. */
int EspNativeResidentGameplay_exitAutomapForModal(
    struct Render_s* render,
    const char* reason);

int EspNativeResidentGameplay_exitAutomapForDamage(
    struct Render_s* render,
    const char* reason);

#ifdef __cplusplus
}
#endif

#endif
