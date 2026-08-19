#ifndef DOOMRPG_ESP32_NATIVE_PALETTE_H
#define DOOMRPG_ESP32_NATIVE_PALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

/*
 * DoomRPG-RE's legacy palette loader leaves the renderer palette in the
 * channel order expected by its historical backend.  The ESP32 native
 * framebuffer is canonical RGB565, so normalize the already-resident palette
 * once before native rasterizers consume it.
 *
 * Returns 1 on success, 0 if the palette is unavailable/invalid.
 */
int DoomRPG_prepareNativePalette(struct Render_s* render);

#ifdef __cplusplus
}
#endif

#endif
