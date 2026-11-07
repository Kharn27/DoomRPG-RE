/* Pure real production dog damage reducer, no combat/RNG/heap mocks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_dog_familiar.h"

int main(void) {
    EspNativeGameplayPlayerState p,old,next,want;
    uint8_t after;
    unsigned weapon,hp,damage,checked=0U;
    memset(&p,0,sizeof p);
    p.active=1U;p.param1=0x1420141eU;p.param2=0x11223344U;
    p.weapons=0x0e0fU;p.credits=99U;p.berserkerTics=5U;
    for(weapon=9U;weapon<=11U;++weapon)for(hp=1U;hp<=99U;++hp){
        p.weapon=(uint8_t)weapon;p.ammo[5]=(uint8_t)hp;
        old=p;
        assert(EspNativeDogFamiliar_active(&p));
        for(damage=1U;damage<hp;++damage) {
            memset(&next,0xA5,sizeof next);
            after=0xffU;
            assert(EspNativeDogFamiliar_prepare(&p,damage,&next,&after)==
                   ESP_NATIVE_DOG_FAMILIAR_ABSORBED);
            assert(after==hp-damage);
            want=old;want.ammo[5]=(uint8_t)(hp-damage);
            assert(memcmp(&next,&want,sizeof next)==0);
            assert(memcmp(&p,&old,sizeof p)==0);
            ++checked;
        }
        memset(&next,0xA5,sizeof next);
        assert(EspNativeDogFamiliar_prepare(&p,hp,&next,&after)==
               ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED);
        assert(next.active==0U&&after==0U);
        assert(EspNativeDogFamiliar_prepare(&p,hp+1U,&next,&after)==
               ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED);
        assert(EspNativeDogFamiliar_prepare(&p,UINT32_MAX,&next,&after)==
               ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED);
        assert(memcmp(&p,&old,sizeof p)==0);
    }
    p.weapon=8U;p.ammo[5]=33U;
    assert(!EspNativeDogFamiliar_active(&p));
    assert(EspNativeDogFamiliar_prepare(&p,1U,&next,&after)==
           ESP_NATIVE_DOG_FAMILIAR_NOT_ACTIVE);
    p.weapon=9U;p.ammo[5]=0U;
    assert(!EspNativeDogFamiliar_active(&p));
    assert(EspNativeDogFamiliar_prepare(&p,1U,&next,&after)==
           ESP_NATIVE_DOG_FAMILIAR_NOT_ACTIVE);
    p.weapon=11U;p.ammo[5]=10U;
    assert(EspNativeDogFamiliar_prepare(&p,0U,&next,&after)==
           ESP_NATIVE_DOG_FAMILIAR_INVALID);
    assert(EspNativeDogFamiliar_prepare(NULL,1U,&next,&after)==
           ESP_NATIVE_DOG_FAMILIAR_INVALID);
    assert(EspNativeDogFamiliar_prepare(&p,1U,NULL,&after)==
           ESP_NATIVE_DOG_FAMILIAR_INVALID);
    assert(EspNativeDogFamiliar_prepare(&p,1U,&next,NULL)==
           ESP_NATIVE_DOG_FAMILIAR_INVALID);
    p.active=0U;
    assert(!EspNativeDogFamiliar_active(&p));
    assert(EspNativeDogFamiliar_prepare(&p,1U,&next,&after)==
           ESP_NATIVE_DOG_FAMILIAR_INVALID);
    printf("PASS dog familiar absorption: %u complete HP/damage/weapon cases, exact player memcpy, death fail-closed, no RNG or entity mutation\n",
           checked);
    return 0;
}
