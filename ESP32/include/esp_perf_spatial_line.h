#ifndef ESP_PERF_SPATIAL_LINE_H
#define ESP_PERF_SPATIAL_LINE_H
#include <stdint.h>
#include "esp_map_runtime.h"

/* P1j: reversible one-plan index of public projected line geometry.
 * 4x4-tile buckets; ascending insertion yields DESCENDING line order.
 * Every query re-evaluates dynamic open state, type, texture, and exact tile.
 * If any projection/geometry cannot be indexed, no index is published. */
#define ESP_PERF_SPATIAL_LINE_CAPACITY 512U
#define ESP_PERF_SPATIAL_LINE_BUCKETS 64U
#define ESP_PERF_SPATIAL_LINE_NONE UINT16_MAX
#ifndef DOOMRPG_PERF_LINE_FAST
#define DOOMRPG_PERF_LINE_FAST 0
#endif
typedef int (*EspPerfSpatialLineTileFn)(const EspMapLine*, uint16_t*);
typedef struct EspPerfSpatialLineIndex_s {
    uint16_t first[ESP_PERF_SPATIAL_LINE_BUCKETS];
    uint16_t next[ESP_PERF_SPATIAL_LINE_CAPACITY];
    uint16_t lineCount;
    uint8_t active;
} EspPerfSpatialLineIndex;
extern EspPerfSpatialLineIndex EspPerfGameP1_spatialLineIndex;
static inline uint8_t EspPerfSpatialLine_bucket(uint16_t tile) {
    return (uint8_t)((((tile >> 5U) >> 2U) * 8U) +
                     ((tile & 31U) >> 2U));
}
static inline void EspPerfSpatialLine_end(EspPerfSpatialLineIndex* idx) {
    if(idx != NULL) {idx->active=0U; idx->lineCount=0U;}
}
static inline int EspPerfSpatialLine_begin(EspPerfSpatialLineIndex* idx,
                                            EspPerfSpatialLineTileFn tileFn) {
    const EspMapRuntimeView* runtime;
    uint32_t i;
    if(idx==NULL || tileFn==NULL) return 0;
    EspPerfSpatialLine_end(idx);
    runtime=EspMapRuntime_view();
    if(runtime==NULL || runtime->lineCount==0U ||
       runtime->lineCount>ESP_PERF_SPATIAL_LINE_CAPACITY) return 0;
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
        idx->next[i]=idx->first[bucket];
        idx->first[bucket]=(uint16_t)i;
    }
    idx->lineCount=(uint16_t)runtime->lineCount;
    idx->active=1U;
    return 1;
}
static inline uint16_t EspPerfSpatialLine_first(
                         const EspPerfSpatialLineIndex* idx,uint16_t tile) {
    if(idx==NULL || !idx->active || tile>=1024U)
        return ESP_PERF_SPATIAL_LINE_NONE;
    return idx->first[EspPerfSpatialLine_bucket(tile)];
}
#endif
