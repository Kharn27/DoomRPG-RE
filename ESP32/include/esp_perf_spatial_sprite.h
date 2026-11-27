#ifndef ESP_PERF_SPATIAL_SPRITE_H
#define ESP_PERF_SPATIAL_SPRITE_H
#include <stdint.h>
#include "esp_map_sprite_topology.h"

/* P1h: short-lived 4x4-tile buckets. A single shared BSS owner serves both
 * plan stages; the index is rebuilt before EVERY synchronous aiGoalPlan and
 * invalidated immediately afterwards. No heap, no persistent topology copy.
 * P3e: encode the exact local tile in the existing next word. Skip
 * off-tile sprites before projected reads; callers still recheck exact tile,
 * type, mask, attacker, link order and special-plane semantics. */
#define ESP_PERF_SPATIAL_SPRITE_CAPACITY 512U
#define ESP_PERF_SPATIAL_SPRITE_BUCKETS 64U
#define ESP_PERF_SPATIAL_SPRITE_NONE UINT16_MAX
#define ESP_PERF_SPATIAL_SPRITE_LINK_MASK 0x03ffU
#define ESP_PERF_SPATIAL_SPRITE_LINK_END 0x03ffU
#define ESP_PERF_SPATIAL_SPRITE_LOCAL_SHIFT 10U
#if ESP_PERF_SPATIAL_SPRITE_CAPACITY > ESP_PERF_SPATIAL_SPRITE_LINK_END
#error Spatial sprite links cannot encode this capacity
#endif
#ifndef DOOMRPG_PERF_SPATIAL_FAST
#define DOOMRPG_PERF_SPATIAL_FAST 1
#endif
typedef struct EspPerfSpatialSpriteIndex_s {
    uint16_t first[ESP_PERF_SPATIAL_SPRITE_BUCKETS];
    uint16_t next[ESP_PERF_SPATIAL_SPRITE_CAPACITY]; /* packed next + local tile */
    uint16_t spriteCount;
    uint8_t active;
} EspPerfSpatialSpriteIndex;
extern EspPerfSpatialSpriteIndex EspPerfGameP1_spatialSpriteIndex;
static inline uint8_t EspPerfSpatialSprite_bucket(uint16_t tile) {
    return (uint8_t)((((tile >> 5U) >> 2U) * 8U) +
                     ((tile & 31U) >> 2U));
}
static inline uint16_t EspPerfSpatialSprite_localTile(uint16_t tile) {
    return (uint16_t)(((tile >> 5U) & 3U) * 4U + (tile & 3U));
}
static inline void EspPerfSpatialSprite_end(EspPerfSpatialSpriteIndex* idx) {
    if(idx != NULL) { idx->active = 0U; idx->spriteCount = 0U; }
}
static inline int EspPerfSpatialSprite_begin(EspPerfSpatialSpriteIndex* idx) {
    const EspMapSpriteTopologyView* topology;
    uint32_t i;
    if(idx == NULL) return 0;
    EspPerfSpatialSprite_end(idx);
    topology = EspMapSpriteTopology_view();
    if(topology == NULL || topology->spriteCount == 0U ||
       topology->spriteCount > ESP_PERF_SPATIAL_SPRITE_CAPACITY) return 0;
    for(i=0; i<ESP_PERF_SPATIAL_SPRITE_BUCKETS; ++i)
        idx->first[i]=ESP_PERF_SPATIAL_SPRITE_NONE;
    /* Reverse insertion yields the original ascending sprite-index order. */
    for(i=topology->spriteCount; i>0U;) {
        uint8_t type, subtype;
        uint16_t state, order;
        uint16_t spriteIndex=(uint16_t)(--i);
        if(!EspMapSpriteTopology_getEntity(spriteIndex,&type,&subtype,
                                            &state,&order)) {
            EspPerfSpatialSprite_end(idx);
            return 0;
        }
        (void)type; (void)subtype; (void)order;
        if((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED)!=0U) {
            const uint16_t tile=state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK;
            const uint8_t bucket=EspPerfSpatialSprite_bucket(tile);
            const uint16_t link=idx->first[bucket]==ESP_PERF_SPATIAL_SPRITE_NONE
                ? ESP_PERF_SPATIAL_SPRITE_LINK_END : idx->first[bucket];
            idx->next[spriteIndex]=(uint16_t)(link |
                (EspPerfSpatialSprite_localTile(tile)<<ESP_PERF_SPATIAL_SPRITE_LOCAL_SHIFT));
            idx->first[bucket]=spriteIndex;
        }
    }
    idx->spriteCount=(uint16_t)topology->spriteCount;
    idx->active=1U;
    return 1;
}
/* Valid only inside the existing synchronous, immutable one-plan scope.
 * Preserve ascending sprite-index order; the packed end is never an index. */
static inline uint16_t EspPerfSpatialSprite_match(
    const EspPerfSpatialSpriteIndex* idx,uint16_t i,uint16_t tile) {
    const uint16_t local=EspPerfSpatialSprite_localTile(tile);
    while(i<idx->spriteCount) {
        const uint16_t packed=idx->next[i];
        if((packed>>ESP_PERF_SPATIAL_SPRITE_LOCAL_SHIFT)==local) return i;
        i=(uint16_t)(packed & ESP_PERF_SPATIAL_SPRITE_LINK_MASK);
    }
    return ESP_PERF_SPATIAL_SPRITE_NONE;
}
static inline uint16_t EspPerfSpatialSprite_first(
    const EspPerfSpatialSpriteIndex* idx,uint16_t tile) {
    if(idx==NULL || idx->active==0U || tile>=1024U)
        return ESP_PERF_SPATIAL_SPRITE_NONE;
    return EspPerfSpatialSprite_match(idx,idx->first[EspPerfSpatialSprite_bucket(tile)],tile);
}
static inline uint16_t EspPerfSpatialSprite_next(
    const EspPerfSpatialSpriteIndex* idx,uint16_t i,uint16_t tile) {
    if(idx==NULL || idx->active==0U || i>=idx->spriteCount || tile>=1024U)
        return ESP_PERF_SPATIAL_SPRITE_NONE;
    return EspPerfSpatialSprite_match(idx,
        (uint16_t)(idx->next[i] & ESP_PERF_SPATIAL_SPRITE_LINK_MASK),tile);
}
#endif
