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
    {
        /* The world half is deliberately NOT committed yet. Test the
         * original death weapon choice and gSprite kind for all familiar
         * strengths/HP and exact snapshot immutability, including cases
         * where the normal next owned weapon is out of ammunition. */
        EspNativeDogFamiliarDeathPlan plan;
        unsigned type,hp,terminalTests=0U;
        for(type=9U;type<=11U;++type) for(hp=1U;hp<=99U;++hp) {
            memset(&p,0,sizeof p);p.active=1U;
            p.weapon=(uint8_t)type;
            p.weapons=(uint16_t)((1U<<type)|(1U<<0)|(1U<<2));
            p.ammo[1]=0U;p.ammo[5]=(uint8_t)hp;
            p.param1=0x1420141eU;
            old=p;
            assert(EspNativeDogFamiliar_planDeath(&p,hp,&plan));
            want=old;want.weapons&=(uint16_t)~0x0e00U;
            want.ammo[5]=0U;want.weapon=0U;
            assert(memcmp(&plan.playerAfter,&want,sizeof want)==0);
            assert(plan.previousWeapon==type&&
                   plan.corpseGSpriteKind==20U+(type-9U)&&
                   plan.nextWeapon==0U);
            assert(memcmp(&p,&old,sizeof p)==0);
            assert(!EspNativeDogFamiliar_planDeath(&p,hp-1U,&plan));
            assert(!plan.playerAfter.active);
            p.weapons=(uint16_t)((1U<<type)|(1U<<2)|(1U<<3));
            p.ammo[1]=0U;p.ammo[2]=1U;
            assert(EspNativeDogFamiliar_planDeath(&p,hp+120U,&plan));
            assert(plan.nextWeapon==3U&&
                   (plan.playerAfter.weapons&0x0e00U)==0U);
            p.ammo[1]=1U;
            assert(EspNativeDogFamiliar_planDeath(&p,hp,&plan));
            assert(plan.nextWeapon==2U);
            p.ammo[1]=0U;p.ammo[2]=0U;
            assert(!EspNativeDogFamiliar_planDeath(&p,hp,&plan));
            assert(!plan.playerAfter.active);
            ++terminalTests;
        }
        printf("PASS dog death planning: %u full HP/weapon cases, 20..22 corpse intents, next-usable source weapon selection, world unowned/fail-closed\n",
               terminalTests);
    }
    printf("PASS dog familiar absorption: %u complete HP/damage/weapon cases, exact player memcpy, death fail-closed, no RNG or entity mutation\n",
           checked);
    return 0;
}
