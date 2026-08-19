#ifndef DOOMRPG_ESP32_NATIVE_BITSHAPE_LOADER_H
#define DOOMRPG_ESP32_NATIVE_BITSHAPE_LOADER_H

struct Render_s;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Rebuild the selected map bitshape metadata directly from the ESP32-native
 * asset pack. The large bitshapes.bin payload remains on SD; only the exact
 * shapeData required by render->mapSpriteTexels is kept resident.
 *
 * On success this intentionally performs the same logical remap as the
 * original Render_loadBitShapes(): mapSpriteTexels is sorted by source shape
 * offset and mediaBitShapeOffsets entries for selected sprites are rewritten
 * to offsets inside render->shapeData.
 */
int DoomRPG_loadNativeBitShapes(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
