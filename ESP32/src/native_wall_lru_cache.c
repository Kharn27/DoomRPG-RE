#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_wall_lru_cache.h"

#define WALL_PAYLOAD_BYTES 2048U

typedef struct EspNativeWallCacheSlot_s {
    EspNativeWallFrame frame;
    uint32_t lastUse;
    int valid;
} EspNativeWallCacheSlot;

typedef struct EspNativeWallCacheState_s {
    Render_t* render;
    EspNativeWallCacheSlot slots[ESP_NATIVE_WALL_CACHE_SLOTS];
    EspNativeWallCacheStats stats;
    uint32_t clock;
    int active;
} EspNativeWallCacheState;

static EspNativeWallCacheState wallCache;

static void releaseSlot(EspNativeWallCacheSlot* slot) {
    if (slot == NULL || !slot->valid) {
        return;
    }

    EspNativeGraphics_releaseWallFrame(&slot->frame);
    memset(slot, 0, sizeof(*slot));
}

static void refreshResidentStats(void) {
    uint32_t resident = 0;
    uint32_t i;

    for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
        if (wallCache.slots[i].valid) {
            resident++;
        }
    }

    wallCache.stats.residentSlots = resident;
    wallCache.stats.residentBytes = resident * WALL_PAYLOAD_BYTES;

    if (resident > wallCache.stats.peakResidentSlots) {
        wallCache.stats.peakResidentSlots = resident;
    }
    if (wallCache.stats.residentBytes > wallCache.stats.peakResidentBytes) {
        wallCache.stats.peakResidentBytes = wallCache.stats.residentBytes;
    }
}

int EspNativeWallCache_begin(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    uint32_t i;

    if (render == NULL || render->mediaTexelOffsets == NULL ||
        render->mediaPalettes == NULL || render->mediaTexels != NULL) {
        printf("[WALLCACHE] FAILED begin render=%p mediaTexels=%p\n",
               (void*)render,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return 0;
    }

    if (wallCache.active) {
        EspNativeWallCache_end();
    }

    memset(&wallCache, 0, sizeof(wallCache));
    wallCache.render = render;
    wallCache.active = 1;

    for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
        wallCache.slots[i].frame.textureIndex = -1;
    }

    printf("[WALLCACHE] BEGIN slots=%u payloadPerSlot=%uB maxPayload=%uB cold=yes\n",
           (unsigned int)ESP_NATIVE_WALL_CACHE_SLOTS,
           (unsigned int)WALL_PAYLOAD_BYTES,
           (unsigned int)(ESP_NATIVE_WALL_CACHE_SLOTS * WALL_PAYLOAD_BYTES));
    return 1;
}

int EspNativeWallCache_acquire(struct Render_s* renderBase,
                               int textureIndex,
                               const struct EspNativeWallFrame_s** outFrame) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeWallCacheSlot* target = NULL;
    uint32_t oldestUse = UINT32_MAX;
    uint32_t i;

    if (outFrame != NULL) {
        *outFrame = NULL;
    }

    if (!wallCache.active || render == NULL || render != wallCache.render ||
        outFrame == NULL || textureIndex < 0 || render->mediaTexels != NULL) {
        printf("[WALLCACHE] FAILED acquire texture=%d active=%d renderMatch=%d mediaTexels=%p\n",
               textureIndex,
               wallCache.active,
               render != NULL && render == wallCache.render,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return 0;
    }

    wallCache.stats.requests++;
    wallCache.clock++;
    if (wallCache.clock == 0) {
        /* A wrap is unrealistic during this port, but preserve strict LRU
         * ordering if a caller ever keeps the cache alive for 2^32 requests.
         */
        uint32_t stamp = 1;
        for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
            if (wallCache.slots[i].valid) {
                wallCache.slots[i].lastUse = stamp++;
            }
        }
        wallCache.clock = stamp;
    }

    for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
        EspNativeWallCacheSlot* slot = &wallCache.slots[i];
        if (slot->valid && slot->frame.textureIndex == textureIndex) {
            slot->lastUse = wallCache.clock;
            wallCache.stats.hits++;
            *outFrame = &slot->frame;
            printf("[WALLCACHE] HIT texture=%d slot=%u hits=%u misses=%u\n",
                   textureIndex,
                   (unsigned int)i,
                   (unsigned int)wallCache.stats.hits,
                   (unsigned int)wallCache.stats.misses);
            return 1;
        }
    }

    wallCache.stats.misses++;

    for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
        EspNativeWallCacheSlot* slot = &wallCache.slots[i];
        if (!slot->valid) {
            target = slot;
            break;
        }
        if (slot->lastUse < oldestUse) {
            oldestUse = slot->lastUse;
            target = slot;
        }
    }

    if (target == NULL) {
        printf("[WALLCACHE] FAILED no target slot for texture=%d\n", textureIndex);
        return 0;
    }

    if (target->valid) {
        printf("[WALLCACHE] EVICT texture=%d -> texture=%d\n",
               target->frame.textureIndex,
               textureIndex);
        wallCache.stats.evictions++;
        releaseSlot(target);
    }

    if (!EspNativeGraphics_loadWallFrame(render, textureIndex, &target->frame)) {
        printf("[WALLCACHE] FAILED miss load texture=%d\n", textureIndex);
        memset(target, 0, sizeof(*target));
        refreshResidentStats();
        return 0;
    }

    target->valid = 1;
    target->lastUse = wallCache.clock;
    refreshResidentStats();

    *outFrame = &target->frame;
    printf("[WALLCACHE] MISS texture=%d resident=%u/%u hits=%u misses=%u evictions=%u\n",
           textureIndex,
           (unsigned int)wallCache.stats.residentSlots,
           (unsigned int)ESP_NATIVE_WALL_CACHE_SLOTS,
           (unsigned int)wallCache.stats.hits,
           (unsigned int)wallCache.stats.misses,
           (unsigned int)wallCache.stats.evictions);
    return 1;
}

void EspNativeWallCache_getStats(EspNativeWallCacheStats* outStats) {
    if (outStats != NULL) {
        *outStats = wallCache.stats;
    }
}

void EspNativeWallCache_end(void) {
    uint32_t i;

    if (!wallCache.active) {
        return;
    }

    printf("[WALLCACHE] END requests=%u hits=%u misses=%u evictions=%u resident=%u peak=%u residentBytes=%u peakBytes=%u\n",
           (unsigned int)wallCache.stats.requests,
           (unsigned int)wallCache.stats.hits,
           (unsigned int)wallCache.stats.misses,
           (unsigned int)wallCache.stats.evictions,
           (unsigned int)wallCache.stats.residentSlots,
           (unsigned int)wallCache.stats.peakResidentSlots,
           (unsigned int)wallCache.stats.residentBytes,
           (unsigned int)wallCache.stats.peakResidentBytes);

    for (i = 0; i < ESP_NATIVE_WALL_CACHE_SLOTS; ++i) {
        releaseSlot(&wallCache.slots[i]);
    }

    wallCache.render = NULL;
    wallCache.active = 0;
}

int EspNativeWallCache_isActive(void) {
    return wallCache.active;
}
