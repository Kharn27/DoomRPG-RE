#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_OVERLAY_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_OVERLAY_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Compose the real MENU_MAIN UI over the already hardware-validated native
 * menu.bsp walls+sprites framebuffer. This intentionally exercises only the
 * post-3D MenuSystem_paint() composition semantics; it does not call the
 * legacy Render_render() path or MenuSystem_setMenu().
 */
int DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
