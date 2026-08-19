#ifndef DOOMRPG_ESP32_NATIVE_SPRITE_TEXEL_PROBE_H
#define DOOMRPG_ESP32_NATIVE_SPRITE_TEXEL_PROBE_H

struct Render_s;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Validate direct random access to sprite texels from DoomRPG-ESP32.pak.
 *
 * Preconditions:
 * - the real menu resource reference lists are resident
 * - DoomRPG_loadNativeBitShapes() has already validated the source-offset model
 * - mediaBitShapeOffsets still contains source offsets into bitshapes.bin
 *
 * The probe derives exact packed sizes from the source bitshape masks, finds the
 * largest selected sprite payload, reads only that payload from stexels.bin,
 * then frees it and requires exact heap recovery.
 */
int DoomRPG_probeNativeSpriteTexels(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
