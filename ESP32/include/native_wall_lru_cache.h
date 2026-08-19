#ifndef DOOMRPG_ESP32_NATIVE_WALL_LRU_CACHE_H
#define DOOMRPG_ESP32_NATIVE_WALL_LRU_CACHE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct EspNativeWallFrame_s;

typedef struct EspNativeWallCacheStats_s {
    uint32_t requests;
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
    uint32_t residentSlots;
    uint32_t peakResidentSlots;
    uint32_t residentBytes;
    uint32_t peakResidentBytes;
} EspNativeWallCacheStats;

/*
 * The first cache policy is deliberately fixed to the smallest measured sweet
 * spot from the hardware-validated menu reference frame: three 2,048-byte wall
 * payloads. Metadata is static; payload storage is allocated only on misses.
 */
#define ESP_NATIVE_WALL_CACHE_SLOTS 3U

int EspNativeWallCache_begin(struct Render_s* render);
int EspNativeWallCache_acquire(struct Render_s* render,
                               int textureIndex,
                               const struct EspNativeWallFrame_s** outFrame);
void EspNativeWallCache_getStats(EspNativeWallCacheStats* outStats);
void EspNativeWallCache_end(void);
int EspNativeWallCache_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
