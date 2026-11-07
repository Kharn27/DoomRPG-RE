#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_gib_sheet.h"

int main(void) {
    EspNativeGibSheetRect p;
    {
        unsigned c;
        for (c=0U;c<65536U;++c) {
            unsigned r=(c>>11U)&31U,g=(c>>5U)&63U,b=c&31U;
            const uint16_t expected =
                (uint16_t)(((r+8U>31U?31U:r+8U)<<11U) |
                           ((g>>1U)<<5U) | (b>>1U));
            assert(EspNativeGibSheet_berserk565((uint16_t)c)==expected);
        }
    }
    /* Independent fixed-point arithmetic oracle recovered from source
     * ParticleSystem_startup/calculateScales/calculateParticles/render:
     * scaleX=320 scaleY=232, 25ms deterministic display ticks.
     * For each distance the low-16 PRNG cadence is 1+4*index draws. */
    {
        static const struct {
            uint8_t distance;
            uint32_t age;
            int16_t x,y,vx;
            uint8_t row;
        } cases[] = {
            {1U,0U,78,62,-427,0U}, {1U,25U,73,59,-427,0U},
            {1U,75U,63,53,-427,0U}, {1U,175U,42,44,-427,0U},
            {1U,349U,5,40,-427,0U},
            {2U,0U,83,60,51,0U}, {2U,25U,84,60,51,0U},
            {2U,75U,85,58,51,0U}, {2U,175U,88,57,51,0U},
            {2U,349U,92,60,51,0U},
            {3U,0U,78,61,-64,0U}, {3U,25U,77,60,-64,0U},
            {3U,75U,76,59,-64,0U}, {3U,175U,73,56,-64,0U},
            {3U,349U,67,55,-64,0U},
            {4U,0U,78,58,-12,0U}, {4U,25U,78,57,-12,0U},
            {4U,75U,78,57,-12,0U}, {4U,175U,77,56,-12,0U},
            {4U,349U,76,58,-12,0U}
        };
        uint32_t i;
        EspNativeGibKineticPoint q,again;
        for(i=0U;i<sizeof(cases)/sizeof(cases[0]);++i) {
            assert(EspNativeGibSheet_sampleParticle(
                0x51e6a19bU,0U,cases[i].distance,cases[i].age,&q));
            assert(q.centerX==cases[i].x&&q.centerY==cases[i].y&&
                   q.velocityX==cases[i].vx&&q.variant==cases[i].row);
            assert(EspNativeGibSheet_sampleParticle(
                0x51e6a19bU,0U,cases[i].distance,cases[i].age,&again));
            assert(memcmp(&q,&again,sizeof q)==0);
        }
        assert(EspNativeGibSheet_tileDistance(0U)==1U);
        assert(EspNativeGibSheet_tileDistance(4096U)==1U);
        assert(EspNativeGibSheet_tileDistance(4097U)==2U);
        assert(EspNativeGibSheet_tileDistance(16384U)==2U);
        assert(EspNativeGibSheet_tileDistance(16385U)==3U);
        assert(EspNativeGibSheet_tileDistance(36864U)==3U);
        assert(EspNativeGibSheet_tileDistance(36865U)==4U);
        assert(EspNativeGibSheet_tileDistance(65536U)==4U);
        assert(EspNativeGibSheet_tileDistance(65537U)==0U);
        assert(!EspNativeGibSheet_sampleParticle(
            0x51e6a19bU,5U,1U,0U,&q));
        assert(!EspNativeGibSheet_sampleParticle(
            0x51e6a19bU,0U,0U,0U,&q));
        assert(!EspNativeGibSheet_sampleParticle(
            0x51e6a19bU,0U,5U,0U,&q));
        assert(!EspNativeGibSheet_sampleParticle(
            0x51e6a19bU,0U,1U,350U,&q));
        assert(!EspNativeGibSheet_sampleParticle(
            0x51e6a19bU,0U,1U,0U,NULL));
        for(i=0U;i<ESP_NATIVE_GIB_KINETIC_COUNT;++i){
            assert(EspNativeGibSheet_sampleParticle(
                0x51e6a19bU,(uint8_t)i,1U,175U,&q));
            assert(q.variant==(uint8_t)i);
            assert(q.velocityX>=-480&&q.velocityX<=480);
        }
    }
    uint8_t row, frame;
    uint32_t checks=0U;
    for(row=0U;row<6U;++row)for(frame=0U;frame<4U;++frame) {
        const uint32_t ms=(uint32_t)frame*175U+10U;
        int sign;
        for(sign=-1;sign<=1;sign+=2) {
            assert(EspNativeGibSheet_plan(ms,row,80,52,(int16_t)sign,
                                          96,144,&p));
            assert(p.frame==(sign>0?3U-frame:frame));
            assert(p.sourceX==(uint16_t)(24U*p.frame));
            assert(p.sourceY==(uint16_t)(row*24U));
            assert(p.destinationX==68&&p.destinationY==40);
            assert(p.width==24U&&p.height==24U);
            ++checks;
        }
    }
    assert(EspNativeGibSheet_plan(0U,0U,-2,10,-1,96,144,&p));
    assert(p.destinationX==0&&p.destinationY==20);
    assert(p.sourceX==14U&&p.sourceY==22U);
    assert(p.width==10U&&p.height==2U);
    assert(EspNativeGibSheet_plan(0U,5U,158,99,-1,96,144,&p));
    assert(p.destinationX==146&&p.destinationY==87);
    assert(p.width==14U&&p.height==13U);
    assert(p.sourceY==120U);
    /* The original SDL frame flags reverse only on strictly positive velX. */
    assert(EspNativeGibSheet_plan(350U,2U,80,52,0,96,144,&p)&&p.frame==2);
    assert(!EspNativeGibSheet_plan(0U,6U,80,52,0,96,144,&p));
    assert(!EspNativeGibSheet_plan(0U,0U,80,52,0,95,144,&p));
    assert(!EspNativeGibSheet_plan(0U,0U,80,52,0,96,143,&p));
    assert(!EspNativeGibSheet_plan(0U,0U,-99,52,0,96,144,&p));
    assert(!EspNativeGibSheet_plan(0U,0U,80,140,0,96,144,&p));
    assert(!EspNativeGibSheet_plan(0U,0U,80,52,0,96,144,NULL));
    printf("PASS gib sheet: 65536 exact legacy berserk RGB565 colors, %u complete 4x6 velocity/reverse frames, exact source and viewport clip, fail closed\n",
           (unsigned)checks);
    return 0;
}
