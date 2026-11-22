#ifndef DOOMRPG_ESP32_RES60_WORLD_PROBE_H
#define DOOMRPG_ESP32_RES60_WORLD_PROBE_H

#include <stddef.h>
#include <stdint.h>
#include "esp_res60_world_preview.h"
#if DOOMRPG_RES60_P8_TEXEL
#include "esp_res60_texture_probe.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* P3 is a one-shot, read-only MAP_INTRO geometry preview. The native PAK
 * inventory/compact runtime are transient; no gameplay, legacy Render_t,
 * textures, entity state or map mutation is ever started. */
typedef struct EspRes60WorldProbeReport_s {
    uint32_t mapBytes;
    uint32_t mapCrc32;
    uint32_t arenaBytes;
    uint32_t arenaFNV;
    uint32_t nodeCount;
    uint32_t lineCount;
    uint32_t spawnIndex;
    uint32_t spawnX;
    uint32_t spawnY;
    uint32_t spawnAngle;
    uint32_t cameraX;
    uint32_t cameraY;
    EspRes60PreviewStats raster;
#if DOOMRPG_RES60_P11_PROFILE
    uint32_t inventoryUs, loadUs, p3Us, textureUs, releaseUs;
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    uint8_t inventoryCacheHit;
#endif
#if DOOMRPG_RES60_P8_TEXEL
    EspRes60TextureProbeReport texture;
#endif
} EspRes60WorldProbeReport;

int EspRes60WorldProbe_renderIntro(uint16_t* framebuffer,
                                   size_t framebufferBytes,
                                   EspRes60WorldProbeReport* outReport);
#if DOOMRPG_RES60_P7_NAV
/* Same immutable map + spawn, turned in 90-degree increments; a failed
 * render never commits the caller's orientation. P3 wrapper stays exact. */
int EspRes60WorldProbe_renderIntroQuarterTurns(
    uint16_t* framebuffer, size_t framebufferBytes, uint8_t quarterTurns,
    EspRes60WorldProbeReport* outReport);
#endif

#ifdef __cplusplus
}
#endif
#endif
