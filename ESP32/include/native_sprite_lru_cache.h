#ifndef DOOMRPG_ESP32_NATIVE_SPRITE_LRU_CACHE_H
#define DOOMRPG_ESP32_NATIVE_SPRITE_LRU_CACHE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct EspNativeSpriteFrame_s;

typedef struct EspNativeSpriteCacheStats_s {
    uint32_t requests;
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
    uint32_t residentSlots;
    uint32_t peakResidentSlots;
    uint32_t residentBytes;
    uint32_t peakResidentBytes;
    uint32_t maxFrameBytes;
} EspNativeSpriteCacheStats;

/* Hardware-derived first policy for the deterministic menu sprite sequence.
 * Three variable-sized frames give 2 hits / 9 misses with a 6,038-byte logical
 * peak. Five slots would recover only one additional hit while raising the
 * peak to 7,437 bytes, so the smaller cache is preferred on the no-PSRAM CYD.
 */
#define ESP_NATIVE_SPRITE_CACHE_SLOTS 3U

int EspNativeSpriteCache_begin(struct Render_s* render);
int EspNativeSpriteCache_acquire(struct Render_s* render,
                                 int spriteIndex,
                                 const struct EspNativeSpriteFrame_s** outFrame);
void EspNativeSpriteCache_getStats(EspNativeSpriteCacheStats* outStats);
void EspNativeSpriteCache_end(void);
int EspNativeSpriteCache_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
