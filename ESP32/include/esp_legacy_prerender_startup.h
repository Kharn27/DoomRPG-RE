#ifndef DOOMRPG_ESP32_LEGACY_PRERENDER_STARTUP_H
#define DOOMRPG_ESP32_LEGACY_PRERENDER_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the retained legacy startup segment that sits between the CYD
 * DoomCanvas/layout stage and EspRenderStartupBridge_start().
 *
 * This preserves the recovered desktop/J2ME startup order for the compatibility
 * owners still required by the active menu/render shell:
 *
 *   ParticleSystem_startup()
 *   MenuSystem_startup()
 *   EntityDef_startup()
 *
 * The caller supplies whether the preceding layout stage succeeded. The bridge
 * remains inert when that prerequisite is unavailable and returns non-zero only
 * after all three retained owners are ready. It performs no Render startup, map
 * loading, native gameplay mutation, or map-specific ownership.
 */
int EspLegacyPrerenderStartup_start(int layoutReady);

#ifdef __cplusplus
}
#endif

#endif
