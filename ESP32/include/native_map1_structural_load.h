#ifndef DOOMRPG_ESP32_NATIVE_MAP1_STRUCTURAL_LOAD_H
#define DOOMRPG_ESP32_NATIVE_MAP1_STRUCTURAL_LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;
struct Render_s;

/* Reset the one-shot first-gameplay-BSP structural-load milestone. */
void Esp32Map1StructuralLoad_reset(void);

/* Service the post-intro MAP_INTRO (/intro.bsp) structural load.
 * The first eligible call only arms the boundary; the following loop service
 * performs the measured load so intro disposal remains a distinct checkpoint.
 */
void Esp32Map1StructuralLoad_service(struct DoomRPG_s* doomRpg);

/* The ESP32 Render_loadBitShapes linker gate calls this at the first legacy
 * graphics-loader boundary, immediately after Render_beginLoadMapData() has
 * freed the raw BSP and completed structural parsing. Non-zero means the
 * MAP_INTRO milestone owns this call and the legacy graphics tail must stop.
 */
int Esp32Map1StructuralLoad_captureBoundary(struct Render_s* render);

int Esp32Map1StructuralLoad_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
