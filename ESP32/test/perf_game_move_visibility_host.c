#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_perf_move_visibility.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_wall_indicator_animation.h"
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
    return EspPerfMoveVisibility_tryReuse(continuation,quiet ? 0U : ESP_PERF_MOVE_BLOCK_FEEDBACK,7,x,32,dst,32,ms);
}
int main(void) {
    memset(&state,0,sizeof(state));
    state.visibleLeaves[0]=1U;
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    assert(!reuse(1,1,160,224,121)); /* cannot re-arm before commit */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    EspPerfMoveVisibility_commitReuse(1);
    assert(reuse(1,1,224,160,121)==DOOMRPG_PERF_MOVE_REUSE);
    EspPerfMoveVisibility_commitReuse(1);
    assert(!reuse(1,1,160,224,122)); /* maximum two reused goals */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    EspPerfMoveVisibility_commitReuse(0);
    assert(!reuse(1,1,224,160,121)); /* owner became busy */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    EspPerfMoveVisibility_invalidate();
    EspPerfMoveVisibility_commitReuse(1);
    assert(!reuse(1,1,224,160,121)); /* rollback invalidates witness */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,120)==DOOMRPG_PERF_MOVE_REUSE);
    EspPerfMoveVisibility_commitReuse(1);
    assert(!reuse(1,1,224,160,350)); /* original deadline not renewed */
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
    assert(!EspPerfMoveVisibility_tryReuse(1,0,7,160,32,224,96,120));
    start(1,100); EspPerfMoveVisibility_noteUnsafe(); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_end(0);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_end(2);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_observe(&state,101);
    EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120)); /* multi-frame */
    /* Green indicator visible, but same original phase: do not reject.
     * A changed phase must still trigger the normal compositor. */
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!EspNativeWallIndicator_changed(ESP_NATIVE_WALL_INDICATOR_GREEN_BIT,100,110));
    assert(EspPerfMoveVisibility_tryReuse(1,
        EspNativeWallIndicator_changed(ESP_NATIVE_WALL_INDICATOR_GREEN_BIT,100,110)
            ? ESP_PERF_MOVE_BLOCK_INDICATOR : 0U,
        7,160,32,224,32,110)==DOOMRPG_PERF_MOVE_REUSE);
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(EspNativeWallIndicator_changed(ESP_NATIVE_WALL_INDICATOR_GREEN_BIT,100,120));
    assert(!EspPerfMoveVisibility_tryReuse(1,
        ESP_PERF_MOVE_BLOCK_INDICATOR | ESP_PERF_MOVE_BLOCK_AMBIENT,
        7,160,32,224,32,120));
    /* Red's doubled phase holds across the first raw cadence boundary. */
    assert(!EspNativeWallIndicator_changed(ESP_NATIVE_WALL_INDICATOR_RED_BIT,300,350));
    assert(EspNativeWallIndicator_changed(ESP_NATIVE_WALL_INDICATOR_RED_BIT,300,700));
    /* P2e: previous move witness predicts a first goal, but the real
     * first-goal path MUST still return false even in fast mode. */
    EspPerfMoveVisibilityContext ctx = {1U,2U,3U,100,200,0,64};
    start(1,100); EspPerfMoveVisibility_end(1);
    EspPerfMoveVisibility_rememberContext(&ctx);
    assert(EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,224,32,120));
    assert(!reuse(0,1,160,224,120));
    start(1,120);
    EspPerfMoveVisibility_firstShadowEnd(1,1U,1U,0,1);
    EspPerfMoveVisibility_end(1);
    EspPerfMoveVisibility_rememberContext(&ctx);
    ctx.angle = 128;
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,224,32,130));
    ctx.angle = 64;
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,ESP_PERF_MOVE_BLOCK_DOOR,7,160,32,224,32,130));
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,256,32,130));
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,224,32,370));
    EspPerfMoveVisibility_invalidate();
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,224,32,130));
    /* Actual staggered phase, not elapsed idle cadence. Fire index 18:
     * sampled clock 100+144=244 -> next phase in exactly 6 ms. */
    start(1,100);
    EspPerfMoveVisibility_noteAmbientPhase(0U,10U,0U,18U,100U);
    EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,105)==DOOMRPG_PERF_MOVE_REUSE);
    start(1,100);
    EspPerfMoveVisibility_noteAmbientPhase(0U,10U,0U,18U,100U);
    EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,106));
    /* Earliest of multiple candidates; subtype4 uses index, not index*8. */
    start(1,100);
    EspPerfMoveVisibility_noteAmbientPhase(0U,1U,4U,145U,100U);
    EspPerfMoveVisibility_noteAmbientPhase(0U,10U,0U,18U,100U);
    EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,105));
    /* Fixed sprites contribute no deadline. */
    start(1,100);
    EspPerfMoveVisibility_noteAmbientPhase(0x80000000UL,10U,0U,18U,100U);
    EspPerfMoveVisibility_end(1);
    assert(reuse(1,1,160,224,106)==DOOMRPG_PERF_MOVE_REUSE);
    /* An unrelated preview or composition invalidates the prior witness. */
    start(1,100); EspPerfMoveVisibility_end(1);
    EspPerfMoveVisibility_frameStarted(1);
    assert(!reuse(1,1,160,224,120));
    start(1,100);
    EspPerfMoveVisibility_frameStarted(1);
    EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120));
    start(1,100); EspPerfMoveVisibility_end(1);
    EspPerfMoveVisibility_rememberContext(&ctx);
    EspPerfMoveVisibility_frameStarted(0);
    assert(!EspPerfMoveVisibility_firstShadowBegin(&ctx,0U,7,160,32,224,32,120));
    /* P2o shadow: phase due alone does not exclude semantic measurement.
     * Actual frame classification must be rejected; start() samples 96->160. */
    state.visibleLeaves[0]=0U;
    for (unsigned scenario=0; scenario<8U; ++scenario) {
        start(1,100); EspPerfMoveVisibility_end(1);
        EspPerfMoveVisibility_rememberContext(&ctx);
        EspPerfMoveVisibility_semanticRemember(11U,2U,33U,44U,1);
        EspPerfMoveVisibility_semanticBegin(&ctx,
            scenario==6U ? ESP_PERF_MOVE_BLOCK_DOOR : ESP_PERF_MOVE_BLOCK_AMBIENT,
            7,160,32,224,32,120,11U,scenario==1U ? 3U : 2U,33U);
        assert(!reuse(0,1,160,224,120)); /* first goal still always renders */
        if (scenario==7U) EspPerfMoveVisibility_invalidate();
        start(1,120);
        assert(EspPerfMoveVisibility_semanticEnd(1,
            scenario==2U ? 45U : 44U, scenario==2U || scenario==3U ? 45U : 44U,
            scenario==4U, 1, scenario==5U ? 34U : 33U, 90000U)==(scenario==0U));
        EspPerfMoveVisibility_end(1);
    }
    state.visibleLeaves[0]=3U;
    start(1,100); EspPerfMoveVisibility_end(1);
    assert(!reuse(1,1,160,224,120)); /* visible leaf */
    return 0;
}
