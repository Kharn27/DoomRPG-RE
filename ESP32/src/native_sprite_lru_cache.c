#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_sprite_lru_cache.h"

typedef struct EspNativeSpriteCacheSlot_s {
    EspNativeSpriteFrame frame;
    uint32_t lastUse;
    int valid;
} EspNativeSpriteCacheSlot;

typedef struct EspNativeSpriteCacheState_s {
    Render_t* render;
    EspNativeSpriteCacheSlot slots[ESP_NATIVE_SPRITE_CACHE_SLOTS];
    EspNativeSpriteCacheStats stats;
    uint32_t clock;
    int active;
} EspNativeSpriteCacheState;

static EspNativeSpriteCacheState spriteCache;

static void releaseSlot(EspNativeSpriteCacheSlot* slot) {
    if (slot == NULL || !slot->valid) {
        return;
    }

    EspNativeGraphics_releaseSpriteFrame(&slot->frame);
    memset(slot, 0, sizeof(*slot));
}

static void refreshResidentStats(void) {
    uint32_t resident = 0;
    uint32_t bytes = 0;
    uint32_t i;

    for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
        if (spriteCache.slots[i].valid) {
            resident++;
            bytes += spriteCache.slots[i].frame.storageBytes;
        }
    }

    spriteCache.stats.residentSlots = resident;
    spriteCache.stats.residentBytes = bytes;
    if (resident > spriteCache.stats.peakResidentSlots) {
        spriteCache.stats.peakResidentSlots = resident;
    }
    if (bytes > spriteCache.stats.peakResidentBytes) {
        spriteCache.stats.peakResidentBytes = bytes;
    }
}

int EspNativeSpriteCache_begin(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    uint32_t i;

    if (render == NULL || render->mediaBitShapeOffsets == NULL ||
        render->mediaPalettes == NULL || render->shapeData != NULL ||
        render->mediaTexels != NULL) {
        printf("[SPRITECACHE] FAILED begin render=%p shapeData=%p mediaTexels=%p\n",
               (void*)render,
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return 0;
    }

    if (spriteCache.active) {
        EspNativeSpriteCache_end();
    }

    memset(&spriteCache, 0, sizeof(spriteCache));
    spriteCache.render = render;
    spriteCache.active = 1;
    for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
        spriteCache.slots[i].frame.spriteIndex = -1;
    }

    printf("[SPRITECACHE] BEGIN slots=%u variablePayload=yes cold=yes\n",
           (unsigned int)ESP_NATIVE_SPRITE_CACHE_SLOTS);
    return 1;
}

int EspNativeSpriteCache_acquire(struct Render_s* renderBase,
                                 int spriteIndex,
                                 const struct EspNativeSpriteFrame_s** outFrame) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeSpriteCacheSlot* target = NULL;
    uint32_t oldestUse = UINT32_MAX;
    uint32_t i;

    if (outFrame != NULL) {
        *outFrame = NULL;
    }

    if (!spriteCache.active || render == NULL || render != spriteCache.render ||
        outFrame == NULL || spriteIndex < 0 || render->shapeData != NULL ||
        render->mediaTexels != NULL) {
        printf("[SPRITECACHE] FAILED acquire sprite=%d active=%d renderMatch=%d shapeData=%p mediaTexels=%p\n",
               spriteIndex,
               spriteCache.active,
               render != NULL && render == spriteCache.render,
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return 0;
    }

    spriteCache.stats.requests++;
    spriteCache.clock++;
    if (spriteCache.clock == 0U) {
        uint32_t stamp = 1U;
        for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
            if (spriteCache.slots[i].valid) {
                spriteCache.slots[i].lastUse = stamp++;
            }
        }
        spriteCache.clock = stamp;
    }

    for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
        EspNativeSpriteCacheSlot* slot = &spriteCache.slots[i];
        if (slot->valid && slot->frame.spriteIndex == spriteIndex) {
            slot->lastUse = spriteCache.clock;
            spriteCache.stats.hits++;
            *outFrame = &slot->frame;
            printf("[SPRITECACHE] HIT sprite=%d slot=%u hits=%u misses=%u residentBytes=%u\n",
                   spriteIndex,
                   (unsigned int)i,
                   (unsigned int)spriteCache.stats.hits,
                   (unsigned int)spriteCache.stats.misses,
                   (unsigned int)spriteCache.stats.residentBytes);
            return 1;
        }
    }

    spriteCache.stats.misses++;
    for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
        EspNativeSpriteCacheSlot* slot = &spriteCache.slots[i];
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
        printf("[SPRITECACHE] FAILED no target slot for sprite=%d\n", spriteIndex);
        return 0;
    }

    if (target->valid) {
        printf("[SPRITECACHE] EVICT sprite=%d(%uB) -> sprite=%d\n",
               target->frame.spriteIndex,
               (unsigned int)target->frame.storageBytes,
               spriteIndex);
        spriteCache.stats.evictions++;
        releaseSlot(target);
        refreshResidentStats();
    }

    if (!EspNativeGraphics_loadSpriteFrame(render, spriteIndex, &target->frame)) {
        printf("[SPRITECACHE] FAILED miss load sprite=%d\n", spriteIndex);
        memset(target, 0, sizeof(*target));
        refreshResidentStats();
        return 0;
    }

    target->valid = 1;
    target->lastUse = spriteCache.clock;
    if (target->frame.storageBytes > spriteCache.stats.maxFrameBytes) {
        spriteCache.stats.maxFrameBytes = target->frame.storageBytes;
    }
    refreshResidentStats();

    *outFrame = &target->frame;
    printf("[SPRITECACHE] MISS sprite=%d frame=%uB resident=%u/%u residentBytes=%u peakBytes=%u hits=%u misses=%u evictions=%u\n",
           spriteIndex,
           (unsigned int)target->frame.storageBytes,
           (unsigned int)spriteCache.stats.residentSlots,
           (unsigned int)ESP_NATIVE_SPRITE_CACHE_SLOTS,
           (unsigned int)spriteCache.stats.residentBytes,
           (unsigned int)spriteCache.stats.peakResidentBytes,
           (unsigned int)spriteCache.stats.hits,
           (unsigned int)spriteCache.stats.misses,
           (unsigned int)spriteCache.stats.evictions);
    return 1;
}

void EspNativeSpriteCache_getStats(EspNativeSpriteCacheStats* outStats) {
    if (outStats != NULL) {
        *outStats = spriteCache.stats;
    }
}

void EspNativeSpriteCache_end(void) {
    uint32_t i;

    if (!spriteCache.active) {
        return;
    }

    printf("[SPRITECACHE] END requests=%u hits=%u misses=%u evictions=%u resident=%u peak=%u residentBytes=%u peakBytes=%u maxFrame=%u\n",
           (unsigned int)spriteCache.stats.requests,
           (unsigned int)spriteCache.stats.hits,
           (unsigned int)spriteCache.stats.misses,
           (unsigned int)spriteCache.stats.evictions,
           (unsigned int)spriteCache.stats.residentSlots,
           (unsigned int)spriteCache.stats.peakResidentSlots,
           (unsigned int)spriteCache.stats.residentBytes,
           (unsigned int)spriteCache.stats.peakResidentBytes,
           (unsigned int)spriteCache.stats.maxFrameBytes);

    for (i = 0; i < ESP_NATIVE_SPRITE_CACHE_SLOTS; ++i) {
        releaseSlot(&spriteCache.slots[i]);
    }

    spriteCache.render = NULL;
    spriteCache.active = 0;
}

int EspNativeSpriteCache_isActive(void) {
    return spriteCache.active;
}
