#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GRAPHICS_CATALOG_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GRAPHICS_CATALOG_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGraphicsCatalogProbe_reset(void);
void Esp32JunctionGraphicsCatalogProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionGraphicsCatalogProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
