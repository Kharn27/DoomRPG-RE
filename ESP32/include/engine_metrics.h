#ifndef DOOMRPG_ESP32_ENGINE_METRICS_H
#define DOOMRPG_ESP32_ENGINE_METRICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DoomRpgEngineMetrics_s {
    uint32_t doomRpg;
    uint32_t doomCanvas;
    uint32_t render;
    uint32_t game;
    uint32_t player;
    uint32_t combat;
    uint32_t supportObjects;
    uint32_t totalInitialObjects;
} DoomRpgEngineMetrics;

uintptr_t DoomRPG_engineLinkAnchor(void);
void DoomRPG_getEngineMetrics(DoomRpgEngineMetrics* metrics);

#ifdef __cplusplus
}
#endif

#endif
