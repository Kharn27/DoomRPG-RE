#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_perf_move_visibility.h"
#include "esp_native_bsp_visibility.h"
static EspNativeBspVisibilityState state;
static uint16_t liveX;
static int fault;
int EspNativeBspVisibility_positionLeaf(uint32_t sprite,uint16_t x,
    uint16_t y,uint32_t* out) {
    (void)y;
    *out=UINT32_MAX;
    if (sprite!=7 || fault) return 0;
    *out=x/128U;
    return 1;
}
int EspNativeBspVisibility_positionVisible(const EspNativeBspVisibilityState* s,
    uint32_t sprite,uint16_t x,uint16_t y,uint32_t* out) {
    return EspNativeBspVisibility_positionLeaf(sprite,x,y,out) &&
        ((s->visibleLeaves[*out>>5]>>(*out&31U))&1U);
}
int EspNativeBspVisibility_mapSpriteVisibleReadOnly(
    const EspNativeBspVisibilityState* s,uint32_t sprite,uint32_t* out) {
    return EspNativeBspVisibility_positionVisible(s,sprite,liveX,32,out);
}
static void start(int quiet,uint32_t ms) {
    liveX=160;
    EspPerfMoveVisibility_begin(7,96,32,160,32,quiet);
    EspPerfMoveVisibility_observe(&state,ms);
}
static int reuse(int continuation,int quiet,uint16_t x,uint16_t dst,uint32_t ms) {
    return EspPerfMoveVisibility_tryReuse(continuation,quiet,7,x,32,dst,32,ms);
}
int main(void) {
    memset(&state,0,sizeof(state));
    state.visibleLeaves[0]=1U;
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    assert(!reuse(1,1,160,224,121)); /* single-use */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(0,1,160,224,120)); /* first goal */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,0,160,224,120)); /* pending owner */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,256,120)); /* cross BSP leaf */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,161,224,120)); /* stale source */
    start(1,100); EspPerfMoveVisibility_end(1); fault=1;
    assert(!reuse(1,1,160,224,120)); fault=0;
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,350)); /* time budget */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,99)); /* clock wrap */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!EspPerfMoveVisibility_tryReuse(1,1,7,160,32,224,96,120));
    start(1,100); EspPerfMoveVisibility_noteUnsafe(); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_end(0);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_end(2);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_observe(&state,101);
    EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120)); /* multi-frame */
    state.visibleLeaves[0]=3U;
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120)); /* visible leaf */
    return 0;
}
