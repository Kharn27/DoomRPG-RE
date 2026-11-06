/* Real permanent presentation owner: no mocks, no gameplay Random_t. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_player_pain_visual.h"

#define W 160U
#define H 120U
static uint16_t fb[W*H+2U];
static uint16_t original[W*H];
static void makeFrame(void) {
    unsigned x,y;
    fb[0]=0x1357U;fb[W*H+1U]=0x2468U;
    for(y=0U;y<H;++y)for(x=0U;x<W;++x)
        fb[1U+y*W+x]=(uint16_t)((x*97U+y*223U)^0x325eU);
    memcpy(original,&fb[1],sizeof original);
}

int main(void) {
    unsigned i,j,patterns=0U;
    int dx,dy;
    int8_t x0,y0,x1,y1;
    uint8_t step,step1;
    EspNativePlayerPainVisual_reset();
    assert(!EspNativePlayerPainVisual_shakeOffsets(100U,&x0,&y0,&step));
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(100U));
    assert(EspNativePlayerPainVisual_stage(77U,ESP_NATIVE_PLAYER_PAIN_RIGHT,1000U));
    assert(EspNativePlayerPainVisual_shakeOffsets(1000U,&x0,&y0,&step));
    assert(step==0U&&x0>=-2&&x0<=1&&y0>=-2&&y0<=1);
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(1000U));
    assert(EspNativePlayerPainVisual_shakeComposed(0U));
    assert(EspNativePlayerPainVisual_commit(77U));
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(1074U));
    assert(EspNativePlayerPainVisual_shakeRefreshDue(1075U));
    assert(EspNativePlayerPainVisual_shakeOffsets(1075U,&x0,&y0,&step));
    assert(step==1U);
    assert(EspNativePlayerPainVisual_shakeComposed(step));
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(1149U));
    assert(EspNativePlayerPainVisual_shakeRefreshDue(1150U));
    for (i=1000U;i<1500U;++i) {
        assert(EspNativePlayerPainVisual_shakeOffsets(i,&x0,&y0,&step));
        assert(EspNativePlayerPainVisual_shakeOffsets(i,&x1,&y1,&step1));
        assert(x0==x1&&y0==y1&&step==step1);
        assert(x0>=-2&&x0<=1&&y0>=-2&&y0<=1);
        assert(step==(i-1000U)/75U);
        if((i-1000U)%75U==0U)++patterns;
    }
    assert(patterns==7U);
    assert(!EspNativePlayerPainVisual_shakeOffsets(1500U,&x0,&y0,&step));
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(1500U));
    /* An aborted second strike restores the old 500ms camera lease. */
    assert(EspNativePlayerPainVisual_stage(78U,ESP_NATIVE_PLAYER_PAIN_LEFT,1400U));
    assert(EspNativePlayerPainVisual_shakeComposed(0U));
    assert(EspNativePlayerPainVisual_cancel(78U));
    assert(!EspNativePlayerPainVisual_shakeRefreshDue(1500U));
    assert(EspNativePlayerPainVisual_expiryDue(1500U));
    assert(EspNativePlayerPainVisual_ackExpiry(1500U));
    assert(!EspNativePlayerPainVisual_shakeComposed(6U));

    /* Every one of the 16 original [-2,+1]^2 positions, all 19200
     * RGB565 pixels, both HUD bands, and both adjacent memory canaries.
     * Exact golden oracle: destination(x,y)=source(x-dx,y-dy) if in
     * original viewport, otherwise black. */
    for(dy=-2;dy<=1;++dy)for(dx=-2;dx<=1;++dx) {
        makeFrame();
        assert(EspNativePlayerPainVisual_shiftViewport(
            &fb[1],W,H,(int8_t)dx,(int8_t)dy));
        assert(fb[0]==0x1357U&&fb[W*H+1U]==0x2468U);
        for(j=0U;j<W*20U;++j)assert(fb[1U+j]==original[j]);
        for(j=W*100U;j<W*H;++j)assert(fb[1U+j]==original[j]);
        for(j=W*20U;j<W*100U;++j) {
            const int x=(int)(j%W),y=(int)(j/W);
            const int sx=x-dx,sy=y-dy;
            const uint16_t expected=sx>=0&&sx<(int)W&&sy>=20&&sy<100
                ?original[sy*W+sx]:0U;
            assert(fb[j+1U]==expected);
        }
    }
    makeFrame();
    assert(!EspNativePlayerPainVisual_shiftViewport(&fb[1],W-1U,H,0,0));
    assert(!EspNativePlayerPainVisual_shiftViewport(&fb[1],W,H,2,0));
    assert(!EspNativePlayerPainVisual_shiftViewport(&fb[1],W,H,0,-3));
    assert(!EspNativePlayerPainVisual_shiftViewport(NULL,W,H,0,0));
    assert(!EspNativePlayerPainVisual_shakeOffsets(100U,NULL,&y0,&step));
    /* Hud.c source four drawRect coordinates: the lower-right corner
     * (159,99) is NOT painted by its 159-wide lower rectangle. */
    assert(EspNativePlayerPainVisual_stage(79U,ESP_NATIVE_PLAYER_PAIN_FRONT,2000U));
    assert(EspNativePlayerPainVisual_commit(79U));
    for(i=2000U;i<=2499U;i+=499U) {
        makeFrame();
        assert(EspNativePlayerPainVisual_paintBorder(&fb[1],W,H,i));
        assert(fb[0]==0x1357U&&fb[W*H+1U]==0x2468U);
        for(j=0U;j<W*H;++j) {
            const unsigned x=j%W,y=j/W;
            const int side=(y>=20U && y<=98U &&
                            (x<=1U||x>=158U));
            const int topBottom=(x<=158U &&
                                (y==20U||y==21U||y==98U||y==99U));
            const uint16_t expect=side||topBottom?0xb800U:original[j];
            assert(fb[1U+j]==expect);
        }
        assert(fb[1U+99U*W+159U]==original[99U*W+159U]);
    }
    makeFrame();
    assert(!EspNativePlayerPainVisual_paintBorder(&fb[1],W,H,2500U));
    assert(!EspNativePlayerPainVisual_paintBorder(&fb[1],W,H-1U,2000U));
    assert(!EspNativePlayerPainVisual_paintBorder(NULL,W,H,2000U));
    assert(memcmp(&fb[1],original,sizeof original)==0);
    puts("PASS player pain shake + continuous original damage border: 16x19200 exact shifts, 500ms/75ms ticks, exact Hud.c four rectangles, HUD/canary guards, rollback");
    return 0;
}
