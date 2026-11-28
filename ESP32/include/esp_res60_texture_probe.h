#ifndef DOOMRPG_ESP32_RES60_TEXTURE_PROBE_H
#define DOOMRPG_ESP32_RES60_TEXTURE_PROBE_H
#include <stddef.h>
#include <stdint.h>
#include "platform_video_config.h"
#ifdef __cplusplus
extern "C" {
#endif
/* P8 is a REAL original-map wall TEXTURE sample, not textured world rendering.
 * Inset covers 48x48 pixels in the upper-right of the 192x96 diagnostic view.
 * Only 2048B of bounded 4bpp payload are ever resident, without PSRAM. */
#define ESP_RES60_TEXTURE_BYTES 2048U
#define ESP_RES60_TEXTURE_INSET_X 144U
#define ESP_RES60_TEXTURE_INSET_Y 24U
#define ESP_RES60_TEXTURE_INSET_SIZE 48U
typedef struct EspRes60TextureProbeReport_s {
    uint16_t logicalId;
    uint16_t catalogCount;
    uint32_t sourceOffsetNibbles;
    uint32_t catalogFNV;
    uint32_t texelFNV;
    uint32_t sampleFNV;
    uint32_t readBytes;
    uint32_t pixels;
    uint8_t cacheHit; /* 1: no catalog build or PAK read this turn */
    uint8_t selectedFromVisible; /* P10: first source ray selected on cold START */
    uint16_t visibleWallColumns; /* P10: current frame, outside P8 inset */
    uint32_t wallColumns; /* P10: projected columns of selected one-tile source */
    uint32_t wallPixels;  /* P10: original wall texel samples written */
    uint32_t wallFNV;     /* P10: witness independent of P8 swatch fingerprint */
#if DOOMRPG_RES60_P38_WALL_ADMISSION
    uint16_t admittedWallIds; /* current full clipped view, not swatch selection */
#endif
#if DOOMRPG_RES60_P11_PROFILE
    /* Current-frame only, not a cache cost claim for P8 warm turns. */
    uint32_t profileCatalogUs, profilePackUs, profileSwatchUs;
    uint32_t profileOverlayUs, profileTotalUs;
#endif
} EspRes60TextureProbeReport;
/* Native packed TEXEL data and original raw 565 palette; caller owns data.
 * Fails BEFORE framebuffer mutation on invalid admission. */
int EspRes60TextureProbe_paint(uint16_t* framebuffer, size_t framebufferBytes,
                               const uint8_t* packed, size_t packedBytes,
                               const uint16_t paletteRaw[16],
                               uint32_t* outFNV);
/* Probe only valid while MAP_INTRO compact runtime is loaded and PAK CLOSED.
 * Builds the tiny native catalog, reads one original 64x64 4bpp map-line wall
 * from wtexels.bin, paints 48x48 diagnostic inset, closes every owner. */
int EspRes60TextureProbe_render(uint16_t* framebuffer, size_t framebufferBytes,
                                EspRes60TextureProbeReport* outReport);
/* One-swatch session cache: original 2048B packed BSS + 32B palette.
 * Explicitly reset on START and successful MENU; no cross-session reuse. */
void EspRes60TextureProbe_reset(void);
#ifdef __cplusplus
}
#endif
#endif
