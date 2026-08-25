#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_CENSUS_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_CENSUS_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void Esp32JunctionSpriteCensusProbe_reset(void);
void Esp32JunctionSpriteCensusProbe_service(void);
int Esp32JunctionSpriteCensusProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
