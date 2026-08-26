#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_LARGE_RANGE_CACHE_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_LARGE_RANGE_CACHE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGameplayLargeRangeCacheProbe_reset(void);
int Esp32JunctionGameplayLargeRangeCacheProbe_isDone(void);
void Esp32JunctionGameplayLargeRangeCacheProbe_service(
    struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif