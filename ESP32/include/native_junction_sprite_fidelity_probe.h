#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_FIDELITY_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_FIDELITY_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionSpriteFidelityProbe_reset(void);
void Esp32JunctionSpriteFidelityProbe_preOverlayService(struct DoomRPG_s* doomRpg);
int Esp32JunctionSpriteFidelityProbe_preOverlayDone(void);
void Esp32JunctionSpriteFidelityProbe_postOverlayService(struct DoomRPG_s* doomRpg);
int Esp32JunctionSpriteFidelityProbe_postOverlayDone(void);

#ifdef __cplusplus
}
#endif

#endif
