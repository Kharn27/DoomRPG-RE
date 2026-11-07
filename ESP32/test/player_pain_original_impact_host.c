/* The real permanent player-pain owner, no mock combat RNG/renderer.
 * Source oracle: src/Combat.c Combat_updateProjectile() weapon switch,
 * src/Game.c Game_gsprite_allocAnim() index to logical bitmap mapping,
 * and Game_gsprite_update() source frame cadence of 150ms for 450ms. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_player_pain_visual.h"

static uint16_t framebuffer[160U*120U+2U];
static void initPixels(void) {
    unsigned i;
    for(i=0U;i<160U*120U;++i) framebuffer[i]=0x15e3U;
    framebuffer[160U*120U]=0x55aaU;
    framebuffer[160U*120U+1U]=0xaa55U;
}
static void guards(void) {
    int x,y;
    assert(framebuffer[160U*120U]==0x55aaU);
    assert(framebuffer[160U*120U+1U]==0xaa55U);
    for(y=0;y<20;++y) for(x=0;x<160;++x)
        assert(framebuffer[y*160+x]==0x15e3U);
    for(y=100;y<120;++y) for(x=0;x<160;++x)
        assert(framebuffer[y*160+x]==0x15e3U);
}

int main(void) {
    static const uint16_t sprite[19] = {
       0U,0U,0U,0U,0U,0U,182U,180U,181U,0U,
       0U,0U,188U,189U,190U,184U,185U,186U,0U
    };
    static const uint8_t bits[4] = {0x09U,0x06U,0x00U,0x00U};
    static const uint8_t texels[2] = {0x21U,0x43U};
    static const uint16_t colors[16] = {
        0U,0xf800U,0x001fU,0x07e0U,0xffffU,0U,0U,0U,
        0U,0U,0U,0U,0U,0U,0U,0U
    };
    uint32_t probe=0U;
    uint16_t logical=0U;
    uint8_t frame=0U;
    uint32_t age,wi,sub;
    int painted;
    EspNativePlayerPainVisual_reset();
    for(wi=0U;wi<19U;++wi) for(sub=0U;sub<14U;++sub){
        uint16_t expected=sprite[wi];
        if(wi==12U&&sub==8U) expected=190U;
        if(wi==15U&&sub==10U) expected=187U;
        assert(EspNativePlayerPainVisual_impactSprite(
            (uint8_t)wi,(uint8_t)sub)==expected);
    }
    assert(EspNativePlayerPainVisual_impactSprite(19U,0U)==0U);
    assert(EspNativePlayerPainVisual_impactSprite(255U,255U)==0U);
    assert(!EspNativePlayerPainVisual_impactInfo(
        1000U,&probe,&logical,&frame));
    assert(!EspNativePlayerPainVisual_stageImpact(77U,12U,1U));

    for(wi=12U;wi<=17U;++wi) {
        EspNativePlayerPainVisual_reset();
        assert(EspNativePlayerPainVisual_stage(
            100U+wi,ESP_NATIVE_PLAYER_PAIN_FRONT,1000U));
        assert(!EspNativePlayerPainVisual_stageImpact(
            99U,12U,0U));
        assert(EspNativePlayerPainVisual_stageImpact(
            100U+wi,(uint8_t)wi,wi==12U?8U:0U));
        /* Nothing at all on pre-COMMIT monster-visual frames. The
         * first shared red flash samples frame zero after COMMIT. */
        assert(!EspNativePlayerPainVisual_impactInfo(
            1300U,&probe,&logical,&frame));
        assert(EspNativePlayerPainVisual_commit(100U+wi));
        assert(EspNativePlayerPainVisual_impactInfo(
            1300U,&probe,&logical,&frame));
        assert(probe==100U+wi);
        assert(frame==0U);
        assert(logical==(wi==12U?190U:sprite[wi]));
        /* Long monster attack tail does NOT consume one frame before
         * the successful LCD witness; all 3 frames get a full 150ms. */
        assert(EspNativePlayerPainVisual_impactInfo(
            1818U,&probe,&logical,&frame) && frame==0U);
        assert(EspNativePlayerPainVisual_firstPresented(1818U));
        for(age=0U;age<500U;++age) {
            assert(EspNativePlayerPainVisual_impactInfo(
                1818U+age,&probe,&logical,&frame));
            assert(frame==(age<150U?0U:age<300U?1U:2U));
        }
        assert(!EspNativePlayerPainVisual_impactInfo(
            2318U,&probe,&logical,&frame));
        assert(probe==0U&&logical==0U&&frame==0U);
        assert(EspNativePlayerPainVisual_expiryDue(2318U));
    }
    /* The dog/previous marker returns EXACTLY after canceled frame; a
     * failed damage transaction cannot leak an impact frame or weapon. */
    EspNativePlayerPainVisual_reset();
    assert(EspNativePlayerPainVisual_stage(
        200U,ESP_NATIVE_PLAYER_PAIN_FRONT,2000U));
    assert(EspNativePlayerPainVisual_stageImpact(200U,12U,0U));
    assert(EspNativePlayerPainVisual_commit(200U));
    assert(EspNativePlayerPainVisual_firstPresented(2100U));
    assert(EspNativePlayerPainVisual_stage(
        201U,ESP_NATIVE_PLAYER_PAIN_BACK,2300U));
    assert(EspNativePlayerPainVisual_stageImpact(201U,15U,10U));
    assert(!EspNativePlayerPainVisual_impactInfo(
        2300U,&probe,&logical,&frame));
    assert(EspNativePlayerPainVisual_cancel(201U));
    assert(EspNativePlayerPainVisual_impactInfo(
        2300U,&probe,&logical,&frame));
    assert(probe==200U&&logical==188U&&frame==1U);

    /* 4x4 column-major source mask, 4 ACTIVE pixels; no RGB transparent
     * key, so palette black and every other shade remain valid. */
    initPixels();
    painted=EspNativePlayerPainVisual_paintImpactBitshape(
        framebuffer,160U,120U,bits,4U,texels,2U,
        colors,30,29,4U,4U,1U);
    assert(painted==4);
    assert(framebuffer[57U*160U+78U]==0xf800U); /* col0 row0 */
    assert(framebuffer[60U*160U+78U]==0x001fU); /* col0 row3 */
    assert(framebuffer[58U*160U+79U]==0x07e0U); /* col1 row1 */
    assert(framebuffer[59U*160U+79U]==0xffffU); /* col1 row2 */
    guards();

    /* Invalid metadata cannot write any pixels, even on the bounds. */
    initPixels();
    assert(EspNativePlayerPainVisual_paintImpactBitshape(
        framebuffer,159U,120U,bits,4U,texels,2U,
        colors,30,29,4U,4U,1U)==-1);
    assert(EspNativePlayerPainVisual_paintImpactBitshape(
        framebuffer,160U,120U,bits,3U,texels,2U,
        colors,30,29,4U,4U,1U)==-1);
    assert(EspNativePlayerPainVisual_paintImpactBitshape(
        framebuffer,160U,120U,bits,4U,texels,1U,
        colors,30,29,4U,4U,1U)==-1);
    assert(EspNativePlayerPainVisual_paintImpactBitshape(
        framebuffer,160U,120U,bits,4U,texels,2U,
        colors,62,29,4U,4U,1U)==-1);
    guards();
    puts("PASS original impact source: 266 weapon/subtype mappings, 12/13/14 claws vs 15-17 missiles, 784ms delayed LCD, 3x150ms PAK frames, exact cancel, column-major packed texel mask, 160x80 clipping, zero HUD/guard writes");
    return 0;
}
