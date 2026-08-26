#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_RENDER_RESOURCE_CACHE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_RENDER_RESOURCE_CACHE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGameplayRenderResourceCacheProbe_reset(void);
int Esp32JunctionGameplayRenderResourceCacheProbe_isDone(void);
void Esp32JunctionGameplayRenderResourceCacheProbe_service(
    struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
