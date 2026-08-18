#ifndef DOOMRPG_ESP32_CONFIG_MAPPINGS_PROBE_H
#define DOOMRPG_ESP32_CONFIG_MAPPINGS_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Continue the validated engine bring-up after Render_startup().
 *
 * Game_loadConfig() is allowed to find no Config file on a first boot. The
 * probe then inspects mappings.bin, verifies that its persistent allocation
 * plan fits the current ESP32 heap, and executes the real Render_loadMappings().
 *
 * No BSP/map file is opened by this probe.
 */
int DoomRPG_probeConfigAndMappings(int renderStartupReady);

#ifdef __cplusplus
}
#endif

#endif
