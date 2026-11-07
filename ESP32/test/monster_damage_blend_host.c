/* Standalone source-justified tint/expiry oracle. The native monster
 * compositor calls the identical RGB565 conversion tested exhaustively by
 * player_pain_visual_host.c, and checks the independent +100ms lease. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_player_pain_visual.h"
#include "esp_native_gameplay_monster_pain_tint.h"


int main(void) {
    uint16_t palette[16], saved[16];
    uint32_t t, i, hash = 2166136261U;
    for(i=0U;i<16U;i++)palette[i]=(uint16_t)(i*4097U);
    memcpy(saved,palette,sizeof palette);
    assert(EspNativeMonsterPainTint_active(1000U,1250U,157U,157U));
    assert(EspNativeMonsterPainTint_active(1099U,1250U,157U,157U));
    assert(!EspNativeMonsterPainTint_active(1100U,1250U,157U,157U));
    assert(!EspNativeMonsterPainTint_active(1249U,1250U,157U,157U));
    assert(!EspNativeMonsterPainTint_active(1000U,1250U,157U,42U));
    assert(!EspNativeMonsterPainTint_active(1000U,1250U,0xffffU,157U));
    assert(!EspNativeMonsterPainTint_active(1000U,0U,157U,157U));
    assert(EspNativeMonsterPainTint_active(0xfffffff0U,0x000000a0U,157U,157U));

    for(t=1000U;t<1250U;t+=5U) {
        if(EspNativeMonsterPainTint_active(t,1250U,157U,157U)) {
            for(i=0U;i<16U;i++) {
                const uint16_t c=EspNativePlayerPainVisual_weaponColor(palette[i]);
                assert((c&31U)>=(palette[i]&31U));
                assert(((c>>5U)&63U)>=((palette[i]>>5U)&63U));
                assert(((c>>11U)&31U)>=((palette[i]>>11U)&31U));
                hash=(hash ^ c)*16777619U;
            }
        }
    }
    assert(memcmp(palette,saved,sizeof palette)==0);
    printf("PASS monster damageBlend 100ms/250ms, palette saturated, immutable cached colors, hash=%08x\n",(unsigned)hash);
    return 0;
}
