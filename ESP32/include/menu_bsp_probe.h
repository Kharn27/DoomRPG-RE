#ifndef DOOMRPG_ESP32_MENU_BSP_PROBE_H
#define DOOMRPG_ESP32_MENU_BSP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Retired ZIP diagnostic, retained only for the bringup boot report.
 * Always returns 0, without loading a map or touching renderer state.
 * Native PAK readers, not this compatibility endpoint, validate current BSPs.
 */
int DoomRPG_probeMenuBspHeader(int configMappingsReady);

#ifdef __cplusplus
}
#endif

#endif
