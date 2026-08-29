#ifndef DOOMRPG_ESP32_LEGACY_CONFIG_MAPPINGS_STARTUP_H
#define DOOMRPG_ESP32_LEGACY_CONFIG_MAPPINGS_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Continue the retained legacy startup after EspRenderStartupBridge_start().
 *
 * Game_loadConfig() is allowed to find no Config file on first boot. This
 * compatibility stage then inspects mappings.bin, verifies that its persistent
 * allocation plan fits the current classic-CYD heap, and executes the retained
 * Render_loadMappings() parser/owner.
 *
 * No BSP/map file is opened by this stage.
 */
int EspLegacyConfigMappingsStartup_start(int renderStartupReady);

#ifdef __cplusplus
}
#endif

#endif
