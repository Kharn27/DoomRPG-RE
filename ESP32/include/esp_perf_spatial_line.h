#ifndef ESP_PERF_SPATIAL_LINE_H
#define ESP_PERF_SPATIAL_LINE_H
#include <stdint.h>
#include "esp_map_runtime.h"
#include "esp_map_line_state.h"

/* P1j: reversible one-plan index of public projected line geometry.
 * 4x4-tile buckets; ascending insertion yields DESCENDING line order.
 * P3d: pack the exact in-bucket tile into each existing next word.
 * Queries visit only exact-tile candidates, then re-evaluate dynamic open
 * state, type, texture and geometry. The synchronous scope is unchanged.
 * If any projection/geometry cannot be indexed, no index is published. */
#define ESP_PERF_SPATIAL_LINE_CAPACITY 512U
#define ESP_PERF_SPATIAL_LINE_BUCKETS 64U
#define ESP_PERF_SPATIAL_LINE_NONE UINT16_MAX
#define ESP_PERF_SPATIAL_LINE_LINK_MASK 0x03ffU
#define ESP_PERF_SPATIAL_LINE_LINK_END 0x03ffU
#define ESP_PERF_SPATIAL_LINE_LOCAL_SHIFT 10U
#if ESP_PERF_SPATIAL_LINE_CAPACITY > ESP_PERF_SPATIAL_LINE_LINK_END
#error Spatial line links cannot encode this capacity
#endif
#ifndef DOOMRPG_PERF_LINE_FAST
#define DOOMRPG_PERF_LINE_FAST 1
#endif
typedef int (*EspPerfSpatialLineTileFn)(const EspMapLine*, uint16_t*);
typedef struct EspPerfSpatialLineIndex_s {
    uint16_t first[ESP_PERF_SPATIAL_LINE_BUCKETS];
    uint16_t next[ESP_PERF_SPATIAL_LINE_CAPACITY]; /* packed next + local tile */
    uint16_t lineCount;
    uint8_t active;
} EspPerfSpatialLineIndex;
extern EspPerfSpatialLineIndex EspPerfGameP1_spatialLineIndex;
static inline uint8_t EspPerfSpatialLine_bucket(uint16_t tile) {
    return (uint8_t)((((tile >> 5U) >> 2U) * 8U) +
                     ((tile & 31U) >> 2U));
}
static inline uint16_t EspPerfSpatialLine_localTile(uint16_t tile) {
    return (uint16_t)(((tile >> 5U) & 3U) * 4U + (tile & 3U));
}
static inline void EspPerfSpatialLine_end(EspPerfSpatialLineIndex* idx) {
    if(idx != NULL) {idx->active=0U; idx->lineCount=0U;}
}
static inline int EspPerfSpatialLine_begin(EspPerfSpatialLineIndex* idx,
                                            EspPerfSpatialLineTileFn tileFn) {
    const EspMapRuntimeView* runtime;
    uint8_t open;
    uint32_t i;
    if(idx==NULL || tileFn==NULL) return 0;
    EspPerfSpatialLine_end(idx);
    runtime=EspMapRuntime_view();
    if(runtime==NULL || runtime->lineCount==0U ||
       runtime->lineCount>ESP_PERF_SPATIAL_LINE_CAPACITY) return 0;
    /* P3j: getOpen is a pure canonical owner-readiness + index bound
     * check followed by a bit read. Validate the greatest index once;
     * all preceding indices have the same ready owner and smaller index.
     * The bit value is not used by this geometry index. Queries still
     * read each candidate's actual openness through the public getter.
     * No owner mutation is possible in the synchronous build/plan scope. */
    if(!EspMapLineState_getOpen(runtime->lineCount - 1U,&open)) return 0;
    for(i=0U;i<ESP_PERF_SPATIAL_LINE_BUCKETS;++i)
        idx->first[i]=ESP_PERF_SPATIAL_LINE_NONE;
    for(i=0U;i<runtime->lineCount;++i) {
        EspMapLine line;
        uint16_t tile;
        uint8_t bucket;
        /* Public getter: never circumvent projected native ownership. */
        if(!EspMapRuntime_getLine(i,&line) || !tileFn(&line,&tile) ||
           tile>=1024U) {
            EspPerfSpatialLine_end(idx);
            return 0;
        }
        bucket=EspPerfSpatialLine_bucket(tile);
        const uint16_t link = idx->first[bucket] == ESP_PERF_SPATIAL_LINE_NONE
            ? ESP_PERF_SPATIAL_LINE_LINK_END : idx->first[bucket];
        idx->next[i]=(uint16_t)(link |
            (EspPerfSpatialLine_localTile(tile) << ESP_PERF_SPATIAL_LINE_LOCAL_SHIFT));
        idx->first[bucket]=(uint16_t)i;
    }
    idx->lineCount=(uint16_t)runtime->lineCount;
    idx->active=1U;
    return 1;
}
/* Only called with a successfully validated, synchronous one-plan index.
 * Preserve descending order, never decode the end marker as an index. */
static inline uint16_t EspPerfSpatialLine_match(
        const EspPerfSpatialLineIndex* idx,uint16_t i,uint16_t tile) {
    const uint16_t local=EspPerfSpatialLine_localTile(tile);
    while(i < idx->lineCount) {
        const uint16_t packed=idx->next[i];
        if((packed >> ESP_PERF_SPATIAL_LINE_LOCAL_SHIFT)==local) return i;
        i=(uint16_t)(packed & ESP_PERF_SPATIAL_LINE_LINK_MASK);
    }
    return ESP_PERF_SPATIAL_LINE_NONE;
}
static inline uint16_t EspPerfSpatialLine_first(
                         const EspPerfSpatialLineIndex* idx,uint16_t tile) {
    if(idx==NULL || !idx->active || tile>=1024U)
        return ESP_PERF_SPATIAL_LINE_NONE;
    return EspPerfSpatialLine_match(idx,idx->first[EspPerfSpatialLine_bucket(tile)],tile);
}
static inline uint16_t EspPerfSpatialLine_next(
        const EspPerfSpatialLineIndex* idx,uint16_t i,uint16_t tile) {
    if(idx==NULL || !idx->active || i>=idx->lineCount || tile>=1024U)
        return ESP_PERF_SPATIAL_LINE_NONE;
    return EspPerfSpatialLine_match(idx,
        (uint16_t)(idx->next[i] & ESP_PERF_SPATIAL_LINE_LINK_MASK),tile);
}
#endif
