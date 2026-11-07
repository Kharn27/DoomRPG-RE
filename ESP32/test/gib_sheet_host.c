#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_gib_sheet.h"

int main(void) {
    EspNativeGibSheetRect p;
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
    printf("PASS gib sheet: %u complete 4x6 velocity/reverse frames, exact source and viewport clip, fail closed\n",
           (unsigned)checks);
    return 0;
}
