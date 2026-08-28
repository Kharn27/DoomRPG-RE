#ifndef DOOMRPG_ESP32_RENDER_STARTUP_BRIDGE_H
#define DOOMRPG_ESP32_RENDER_STARTUP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the retained CYD Render compatibility shell after the preceding
 * engine layout / pre-render startup stage has succeeded.
 *
 * The linker-owned Render_startup() bridge reuses PlatformVideo's permanent
 * 160x120 RGB565 framebuffer instead of allocating the desktop piDIB + second
 * framebuffer pair. It also validates and loads the legacy sintable/palette
 * resources still required by the retained Render shell.
 *
 * Returns non-zero only when the shared framebuffer alias, pitch/clip state,
 * sintable and palettes are all ready. No map loading or gameplay ownership is
 * performed here.
 */
int EspRenderStartupBridge_start(int preRenderReady);

#ifdef __cplusplus
}
#endif

#endif
