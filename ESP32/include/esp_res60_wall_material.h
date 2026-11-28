#ifndef DOOMRPG_ESP32_RES60_WALL_MATERIAL_H
#define DOOMRPG_ESP32_RES60_WALL_MATERIAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* P10 is a diagnostic material overlay for ONE already-resident 64x64
 * 4bpp texture. It deliberately is not the permanent world renderer.
 * Each P3 ray contributes at most one compact 8-byte column descriptor. */
#define ESP_RES60_WALL_MATERIAL_SOURCE_BYTES 2048U
typedef struct EspRes60WallMaterialStats_s {
    uint32_t columns;
    uint32_t pixels;
    uint32_t sampledFNV;
} EspRes60WallMaterialStats;
void EspRes60WallMaterial_reset(void);
int EspRes60WallMaterial_record(uint32_t column, uint16_t logicalTexture,
                               int32_t top, int32_t height, uint8_t sourceU);
void EspRes60WallMaterial_publish(void);
/* Full clipped wall demand, including pixels currently covered by the
 * diagnostic swatch. IDs outside the native 256-ID catalog are explicit,
 * never silently truncated or treated as texture zero. */
typedef struct EspRes60WallCoverage_s {
    uint16_t materials, columns, unsupportedColumns;
    uint16_t pixels, insetPixels;
} EspRes60WallCoverage;
int EspRes60WallMaterial_measure(EspRes60WallCoverage* out);
/* Select among rays actually visible in the frame; no second raycast, heap
 * or catalog dependency. Excludes material fully hidden by P8's swatch. */
int EspRes60WallMaterial_visibleTextureAt(uint32_t column,
                                         uint16_t* outTexture);
uint32_t EspRes60WallMaterial_visibleColumns(uint16_t logicalTexture);
/* Called only after P3 succeeded and P8 sampled a real immutable PAK tile.
 * Flat walls of every other logical ID are left unchanged. The P8 48x48
 * swatch always wins; HUD, sky/floor and outside-frame pixels are untouched. */
int EspRes60WallMaterial_overlay(uint16_t* framebuffer, size_t framebufferBytes,
                                 const uint8_t* packed, size_t packedBytes,
                                 const uint16_t paletteRaw[16],
                                 uint16_t logicalTexture,
                                 EspRes60WallMaterialStats* out);
#ifdef __cplusplus
}
#endif
#endif
