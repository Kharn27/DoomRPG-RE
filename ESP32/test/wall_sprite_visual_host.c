/* The live native inline visual-pose helper is compiled as production C. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "esp_native_sprite_visual_legacy.h"

static void referenceCoords(uint16_t rawX, uint16_t rawY, uint32_t info,
                            int *x, int *y) {
    *x=rawX; *y=rawY;
    if (info & 0x800000UL) {
        if (info & 0x100000UL) ++*y;
        else if (info & 0x80000UL) --*y;
        else if (info & 0x200000UL) ++*x;
        else if (info & 0x400000UL) --*x;
    }
}
static int32_t referenceSort(int x,int y,uint32_t info,uint8_t type,
                             int c,int s,int t) {
    if (info & 0x800000UL) return INT32_MAX;
    int32_t z=(int32_t)(x*c+y*s+t);
    if (info & 0x1000000UL) ++z;
    else if (type==1U) --z;
    else {
        uint32_t id=info&511UL;
        if (id>=184U && id<=190U) z=0;
        else if (id>=180U && id<=191U) z-=2;
    }
    return z;
}
int main(void) {
    unsigned checks=0;
    const uint32_t flags[]={
        0U,0x800000UL|0x100000UL,0x800000UL|0x80000UL,
        0x800000UL|0x200000UL,0x800000UL|0x400000UL,
        0x800000UL|0x180000UL,0x800000UL,0x1000000UL,
        0x800000UL|0x200000UL|0x1000000UL
    };
    for(unsigned f=0;f<sizeof(flags)/sizeof(flags[0]);++f) {
        for(uint32_t id=178;id<=192;++id) {
            for(uint32_t k=0;k<64;++k) {
                uint32_t info=flags[f]|id;
                uint16_t x=(uint16_t)(100+(k*17)%1600);
                uint16_t y=(uint16_t)(100+(k*23)%1600);
                int refX,refY,actX,actY;
                referenceCoords(x,y,info,&refX,&refY);
                EspNativeSpriteVisual_coordinates(x,y,info,&actX,&actY);
                assert(refX==actX && refY==actY);
                int co=(int)(k%17)-8,si=(int)(k%13)-6;
                int trans=(int)(k%31)-15;
                for(uint8_t ty=0;ty<3;++ty) {
                    int32_t ref=referenceSort(refX,refY,info,ty,co,si,trans);
                    int32_t got=EspNativeSpriteVisual_sortDepth(
                        actX,actY,info,ty,co,si,trans);
                    assert(ref==got);
                    ++checks;
                }
            }
        }
    }
    /* A sprite flush with its surface never changes the source coordinates. */
    int x=0,y=0;
    EspNativeSpriteVisual_coordinates(0,0,0x800000UL|0x80000UL,&x,&y);
    assert(x==0 && y==-1);
    EspNativeSpriteVisual_coordinates(0,0,0x800000UL|0x400000UL,&x,&y);
    assert(x==-1 && y==0);
    assert(EspNativeSpriteVisual_sortDepth(320,320,0x800000UL,0,1,2,3)==INT32_MAX);
    assert(EspNativeSpriteVisual_sortDepth(320,320,184,0,1,2,3)==0);
    printf("PASS wall-sprite visual parity cases=%u nudge=4 sort=front+184..190 source=unchanged\\n",checks);
}
