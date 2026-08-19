#ifndef DOOMRPG_ESP32_NATIVE_BITSHAPE_LOADER_H
#define DOOMRPG_ESP32_NATIVE_BITSHAPE_LOADER_H

struct Render_s;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Validate the ESP32-native on-demand bitshape model for the currently
 * selected map sprites.
 *
 * Unlike the original Render_loadBitShapes(), this does NOT build the expanded
 * render->shapeData table. mediaBitShapeOffsets keeps its original source
 * offsets from mappings.bin, bitshapes.bin remains on SD, and masks are decoded
 * one column at a time with a bounded <=32-byte scratch buffer.
 *
 * This routine also derives the exact packed sprite-texel payload implied by
 * the selected bitshape masks. Renderer integration is intentionally deferred
 * to a later increment; Render_loadTexels() remains blocked.
 */
int DoomRPG_loadNativeBitShapes(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
