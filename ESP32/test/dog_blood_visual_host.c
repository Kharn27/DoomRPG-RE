#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_player_pain_visual.h"
#include "esp_native_gameplay_dog_blood.h"

#define W 160
#define H 120
static uint16_t fb[W*H+2U];
static uint32_t fnvFrame(void) {
    uint32_t hash = 2166136261U;
    unsigned i;
    for(i=1U;i<=W*H;++i){
        hash=(hash^(uint8_t)fb[i])*16777619U;
        hash=(hash^(uint8_t)(fb[i]>>8U))*16777619U;
    }
    return hash;
}
int main(void) {
    uint32_t seed,age,checksum=0U,firstFNV;
    unsigned idx,t;
    int16_t x,y;
    uint8_t radius;
    EspNativePlayerPainVisual_reset();
    assert(!EspNativePlayerPainVisual_dogInfo(100U,&seed,&age));
    /* No ordinary, critical, lethal or miss path may invent familiar blood. */
    assert(EspNativePlayerPainVisual_stage(77U,ESP_NATIVE_PLAYER_PAIN_RIGHT,100U));
    assert(EspNativePlayerPainVisual_commit(77U));
    assert(EspNativePlayerPainVisual_firstPresented(100U));
    assert(!EspNativePlayerPainVisual_dogInfo(200U,&seed,&age));
    assert(EspNativePlayerPainVisual_stageDog(
        78U,ESP_NATIVE_PLAYER_PAIN_LEFT,200U));
    assert(EspNativePlayerPainVisual_dogInfo(200U,&seed,&age)&&age==0U);
    assert(EspNativePlayerPainVisual_cancel(78U));
    assert(!EspNativePlayerPainVisual_dogInfo(200U,&seed,&age));
    assert(EspNativePlayerPainVisual_stageDog(
        79U,ESP_NATIVE_PLAYER_PAIN_FRONT,400U));
    assert(EspNativePlayerPainVisual_commit(79U));
    assert(EspNativePlayerPainVisual_firstPresented(400U));
    assert(EspNativePlayerPainVisual_dogInfo(499U,&seed,&age)&&age==99U);
    for(idx=0U;idx<ESP_NATIVE_DOG_BLOOD_COUNT;idx++){
        EspNativeDogBloodParticle p,q;
        assert(EspNativeDogBlood_sample(seed,(uint8_t)idx,&p));
        assert(EspNativeDogBlood_sample(seed,(uint8_t)idx,&q));
        assert(memcmp(&p,&q,sizeof p)==0);
        assert(p.startX>=10&&p.startX<=30);
        assert(p.startY>=15&&p.startY<=35);
        assert(p.velX>=-150&&p.velX<=75);
        assert(p.velY>=-100&&p.velY<=-50);
        assert(p.size>=1&&p.size<=4);
        EspNativeDogBlood_project(&p,0U,&x,&y,&radius);
        assert(x>=90&&x<=110&&y>=75&&y<=95);
        assert(radius==p.size/2U);
        EspNativeDogBlood_project(&p,499U,&x,&y,&radius);
        assert(x>=-100 && x<160 && y>=10 && y<170);
    }
    assert(!EspNativeDogBlood_sample(seed,15U,NULL));
    assert(!EspNativeDogBlood_sample(seed,15U,&(EspNativeDogBloodParticle){0}));
    for(t=0U;t<500U;t+=37U){
        uint32_t n;
        fb[0]=0x1234U;fb[W*H+1U]=0x4321U;
        for(idx=1U;idx<=W*H;++idx)fb[idx]=0x2222U;
        n=EspNativeDogBlood_paint(&fb[1],W,H,seed,t,0U);
        assert(n>0U||t>250U);
        assert(fb[0]==0x1234U&&fb[W*H+1U]==0x4321U);
        for(idx=0U;idx<20U*W;++idx)assert(fb[idx+1U]==0x2222U);
        for(idx=100U*W;idx<H*W;++idx)assert(fb[idx+1U]==0x2222U);
        checksum ^= fnvFrame()+t;
        if(t==0U){firstFNV=fnvFrame();assert(firstFNV!=0U);}
    }
    assert(EspNativeDogBlood_paint(&fb[1],W,H,seed,500U,0U)==0U);
    assert(EspNativeDogBlood_paint(&fb[1],W-1U,H,seed,50U,0U)==0U);
    assert(EspNativeDogBlood_paint(NULL,W,H,seed,50U,0U)==0U);
    assert(!EspNativePlayerPainVisual_dogInfo(900U,&seed,&age));
    assert(EspNativePlayerPainVisual_expiryDue(900U));
    assert(EspNativePlayerPainVisual_ackExpiry(900U));
    assert(!EspNativePlayerPainVisual_dogInfo(901U,&seed,&age));
        for(idx=1U;idx<=W*H;++idx)fb[idx]=0x2222U;
        (void)EspNativeDogBlood_paint(
            &fb[1],W,H,0xa8cf35f4U,0U,1U);
        assert(fnvFrame()==0x6f619a65U); /* berserker red +8 */
    assert(checksum!=0U);
    /* Independent fixed 160x120 RGB565 witnesses generated from the source
     * Combat_spawnDogBloodParticles ranges + SDL_RenderDrawFillCircle spans.
     * These catch regressions in trajectories, palette, circle raster, clip
     * and timing, not merely a nonzero pixel count or a self-oracle. */
    {
        const struct { uint16_t ms; uint32_t fnv; } golden[] = {
            {0U, 0x11caece5U}, {37U, 0x9ebc03b5U},
            {99U, 0x48bad805U}, {185U, 0x43722ad5U},
            {370U, 0xdda3bee5U}, {499U, 0x06ebddc5U}
        };
        unsigned k;
        for (k=0U;k<sizeof golden/sizeof golden[0];++k) {
            for(idx=1U;idx<=W*H;++idx)fb[idx]=0x2222U;
            (void)EspNativeDogBlood_paint(
                &fb[1],W,H,0xa8cf35f4U,golden[k].ms,0U);
            assert(fnvFrame()==golden[k].fnv);
        }
    }
    printf("PASS familiar blood: legacy 15 particles, 14 time samples, 6 golden RGB565 frame FNVs, commit/rollback, expiry, bounds checksum=%08x\n",(unsigned)checksum);
    return 0;
}
