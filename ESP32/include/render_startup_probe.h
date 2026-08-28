#ifndef DOOMRPG_ESP32_RENDER_STARTUP_PROBE_H
#define DOOMRPG_ESP32_RENDER_STARTUP_PROBE_H

/*
 * Transitional source-compatibility shim.
 *
 * The retained startup implementation is permanent CYD compatibility code, not
 * a probe. New code must include esp_render_startup_bridge.h and call
 * EspRenderStartupBridge_start(). Existing bootstrap callers are kept source-
 * compatible for this bounded naming pass; the macro also causes the existing
 * implementation definition to emit the permanent generic symbol.
 */
#include "esp_render_startup_bridge.h"

#define DoomRPG_probeRenderStartup EspRenderStartupBridge_start

#endif
