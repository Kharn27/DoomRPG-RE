/* P1j pure host index oracle: descending order, projected changes,
 * capacity/failure/fallback and exhaustive tile-to-bucket coverage. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_perf_spatial_line.h"
static EspMapRuntimeView runtime;
static EspMapLine projected[ESP_PERF_SPATIAL_LINE_CAPACITY];
static int failAt=-1;
static int invalidAt=-1;
static int failOpenAt=-1;
int EspMapLineState_getOpen(uint32_t i,uint8_t* out) {
    if(i>=runtime.lineCount || (int)i==failOpenAt || out==NULL) return 0;
    *out=(uint8_t)(i&1U); return 1;
}
const EspMapRuntimeView* EspMapRuntime_view(void) {return &runtime;}
int EspMapRuntime_getLine(uint32_t i,EspMapLine* out) {
    if(i>=runtime.lineCount || (int)i==failAt || out==NULL) return 0;
    *out=projected[i];return 1;
}
static int tileOf(const EspMapLine* line,uint16_t* out) {
    uint32_t x=line->x1, y=line->y1;
    if((int)line->texture==invalidAt) return 0;
    /* Test geometry projection's nudge with a dynamic public getter. */
    if(line->flags&1U) ++x;
    if(x>=2048U||y>=2048U) return 0;
    *out=(uint16_t)(((y>>6U)*32U)+(x>>6U));
    return 1;
}
int main(void) {
    EspPerfSpatialLineIndex idx={0};
    unsigned i,tile;
    runtime.lineCount=448U;
    for(i=0;i<runtime.lineCount;i++) {
        projected[i].x1=(uint16_t)((i*73U)%2048U);
        projected[i].y1=(uint16_t)((i*211U)%2048U);
        projected[i].texture=(uint16_t)i;
    }
    assert(EspPerfSpatialLine_begin(&idx,tileOf));
    assert(idx.active && idx.lineCount==448U);
    for(tile=0;tile<1024U;tile++) {
        uint16_t cur=EspPerfSpatialLine_first(&idx,(uint16_t)tile);
        for(i=runtime.lineCount;i>0U;) {
            uint16_t candidate;uint16_t projectedTile;
            --i;
            assert(tileOf(&projected[i],&projectedTile));
            if(EspPerfSpatialLine_bucket(projectedTile)!=EspPerfSpatialLine_bucket((uint16_t)tile)) continue;
            candidate=(uint16_t)i;
            assert(cur==candidate);
            cur=idx.next[cur];
        }
        assert(cur==ESP_PERF_SPATIAL_LINE_NONE);
    }
    assert(EspPerfSpatialLine_first(&idx,1024U)==ESP_PERF_SPATIAL_LINE_NONE);
    EspPerfSpatialLine_end(&idx);
    assert(EspPerfSpatialLine_first(&idx,1U)==ESP_PERF_SPATIAL_LINE_NONE);
    /* Dynamic door/geometry projection must trigger a fresh build per goal. */
    projected[10].x1=127U;projected[10].y1=128U;projected[10].flags=1U;
    assert(EspPerfSpatialLine_begin(&idx,tileOf));
    /* Verify actual membership after crossing a bucket boundary. */
    projected[10].x1=255U; projected[10].y1=256U;
    assert(EspPerfSpatialLine_begin(&idx,tileOf));
    for(tile=0;tile<1024U;tile++) {
        uint16_t cur=EspPerfSpatialLine_first(&idx,(uint16_t)tile);
        int found=0;
        while(cur!=ESP_PERF_SPATIAL_LINE_NONE) {
            if(cur==10U) ++found;
            cur=idx.next[cur];
        }
        assert(found==(EspPerfSpatialLine_bucket((uint16_t)tile)==
                       EspPerfSpatialLine_bucket(132U)));
    }
    failOpenAt=447;
    assert(!EspPerfSpatialLine_begin(&idx,tileOf) && !idx.active);
    assert(EspPerfSpatialLine_first(&idx,132U)==ESP_PERF_SPATIAL_LINE_NONE);
    failOpenAt=-1;
    EspPerfSpatialLine_end(&idx);
    failAt=12;
    assert(!EspPerfSpatialLine_begin(&idx,tileOf) && !idx.active);
    failAt=-1;invalidAt=13;
    assert(!EspPerfSpatialLine_begin(&idx,tileOf) && !idx.active);
    invalidAt=-1;runtime.lineCount=513U;
    assert(!EspPerfSpatialLine_begin(&idx,tileOf) && !idx.active);
    runtime.lineCount=512U;
    for(i=448U;i<512U;i++) projected[i]=projected[i%448U];
    assert(EspPerfSpatialLine_begin(&idx,tileOf));
    EspPerfSpatialLine_end(&idx);
    puts("P1j spatial line host PASS descending/buckets/rebuild/capacity/fail-closed");
    return 0;
}
