#ifndef ESP_PERF_TRACE_CACHE_H
#define ESP_PERF_TRACE_CACHE_H
#include <stdint.h>
#include <string.h>
/* Scope: one synchronous aiGoalPlan, no world mutations during scope.
 * Full directed coordinate key, attacker, mask, strict-special. No negative
 * result caching: readiness failures must continue through reference path.
 * Bounded cache: full => reference fallback, never eviction or allocation. */
#define ESP_PERF_TRACE_CAPACITY 36U
typedef struct {
    int32_t sx,sy,dx,dy;
    uint16_t attacker,mask;
    uint8_t strict,result;
} EspPerfTraceEntry;
typedef struct {
    EspPerfTraceEntry entries[ESP_PERF_TRACE_CAPACITY];
    uint8_t count,active;
} EspPerfTraceCache;
static inline void EspPerfTraceCache_begin(EspPerfTraceCache* c) {
    c->count=0; c->active=1;
}
static inline void EspPerfTraceCache_end(EspPerfTraceCache* c) {
    c->active=0; c->count=0;
}
static inline int EspPerfTraceCache_lookup(const EspPerfTraceCache* c,
    int32_t sx,int32_t sy,int32_t dx,int32_t dy,uint16_t attacker,
    uint16_t mask,int strict,int* out) {
    unsigned i;
    if(!c->active || !out || (strict!=0 && strict!=1)) return 0;
    for(i=0;i<c->count;i++) {
        const EspPerfTraceEntry* e=&c->entries[i];
        if(e->sx==sx && e->sy==sy && e->dx==dx && e->dy==dy &&
           e->attacker==attacker && e->mask==mask && e->strict==strict) {
            *out=e->result; return 1;
        }
    }
    return 0;
}
static inline void EspPerfTraceCache_store(EspPerfTraceCache* c,
    int32_t sx,int32_t sy,int32_t dx,int32_t dy,uint16_t attacker,
    uint16_t mask,int strict,int result) {
    EspPerfTraceEntry* e;
    if(!c->active || c->count>=ESP_PERF_TRACE_CAPACITY ||
       (strict!=0 && strict!=1) || (result!=0 && result!=1)) return;
    e=&c->entries[c->count++];
    e->sx=sx;e->sy=sy;e->dx=dx;e->dy=dy;e->attacker=attacker;
    e->mask=mask;e->strict=(uint8_t)strict;e->result=(uint8_t)result;
}
#endif
