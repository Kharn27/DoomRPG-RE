#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_OVERLAY_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_OVERLAY_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Temporary post-PARK hardware milestone for the first native map-sprite pass.
 * It renders only the family actually observed on the canonical Junction pose:
 * visible, standard billboards, base render mode 0, animation frame 0.
 * Legacy glow companions remain explicitly deferred to their own milestone.
 */
void Esp32JunctionSpriteOverlayProbe_reset(void);
int Esp32JunctionSpriteOverlayProbe_isDone(void);
void Esp32JunctionSpriteOverlayProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
