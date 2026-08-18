#ifndef DOOMRPG_ESP32_MENU_BSP_PROBE_H
#define DOOMRPG_ESP32_MENU_BSP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * First BSP contact for the classic no-PSRAM CYD.
 *
 * Requires the already validated config/mappings stage. The probe checks the
 * ZIP metadata and transient inflate budget, reads /menu.bsp for real, parses
 * only the fixed 33-byte map header, then frees the BSP buffer immediately.
 * It deliberately does NOT call Render_beginLoadMap() or
 * Render_beginLoadMapData().
 */
int DoomRPG_probeMenuBspHeader(int configMappingsReady);

#ifdef __cplusplus
}
#endif

#endif
