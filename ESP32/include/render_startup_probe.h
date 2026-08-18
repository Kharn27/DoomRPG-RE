#ifndef DOOMRPG_ESP32_RENDER_STARTUP_PROBE_H
#define DOOMRPG_ESP32_RENDER_STARTUP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cross Render_startup() only after the core, layout and pre-render stages have
 * been validated. The ESP32 linker redirects Render_startup() to a CYD bridge
 * that reuses the platform's existing 160x120 RGB565 framebuffer instead of
 * allocating the desktop piDIB + framebuffer pair.
 *
 * Returns non-zero only when sintable.bin and palettes.bin load successfully,
 * the Render framebuffer aliases PlatformVideo_framebuffer(), and the desktop
 * streaming texture remains absent.
 */
int DoomRPG_probeRenderStartup(int preRenderReady);

#ifdef __cplusplus
}
#endif

#endif
