#ifndef DOOMRPG_ESP32_LEGACY_CONFIG_MAPPINGS_STARTUP_H
#define DOOMRPG_ESP32_LEGACY_CONFIG_MAPPINGS_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Continue the retained legacy startup after EspRenderStartupBridge_start().
 *
 * Native EspNativeConfig_loadAtBoot handles absent/invalid config after SD
 * mount. This compatibility stage never reads desktop Config v23. It inspects
 * mappings.bin, verifies its persistent allocation plan for the classic CYD,
 * and directly executes EspLegacyMappings_load(), the native bounded PAK parser/owner.
 *
 * No BSP/map file is opened by this stage.
 */
int EspLegacyConfigMappingsStartup_start(int renderStartupReady);

struct Render_s;

/* ESP32 replacement for the legacy whole-file mappings loader. It uses the
 * already-owned 160x120 framebuffer as transient inflate/output scratch, then
 * installs the same four persistent mapping arrays. */
int EspLegacyMappings_load(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
