/* Exercise the exact RGB565 helper compiled by the CYD menu renderer. */
#include "esp_uac_font_theme.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    const uint16_t themes[][3] = {
        {0xd225U, 0xf5aaU, 0xff34U},
        {0xc002U, 0xe30cU, 0xfe78U},
        {0x2638U, 0x87e0U, 0xc7b8U}
    };
    for (unsigned t=0;t<3;++t) for (int rows=19;rows<=23;rows+=2) {
        assert(EspUacFontTheme_verticalTone(themes[t][0],themes[t][1],themes[t][2],0,rows)==themes[t][0]);
        assert(EspUacFontTheme_verticalTone(themes[t][0],themes[t][1],themes[t][2],rows-1,rows)==themes[t][2]);
        assert(EspUacFontTheme_verticalTone(themes[t][0],themes[t][1],themes[t][2],rows/2,rows)==themes[t][1]);
        assert(EspUacFontTheme_verticalTone(themes[t][0],themes[t][1],themes[t][2],-1,rows)==0);
        for (int row=0;row<rows;++row) {
            uint16_t tone=EspUacFontTheme_verticalTone(themes[t][0],themes[t][1],themes[t][2],row,rows);
            assert(EspUacFontTheme_tintGray(0,tone)==0);
            assert(EspUacFontTheme_tintGray(0xffffU,tone)==tone);
        }
    }
    for (int row=0;row<23;++row) {
        uint16_t a=EspUacFontTheme_verticalTone(themes[0][0],themes[0][1],themes[0][2],row,23);
        uint16_t b=EspUacFontTheme_verticalTone(themes[1][0],themes[1][1],themes[1][2],row,23);
        uint16_t c=EspUacFontTheme_verticalTone(themes[2][0],themes[2][1],themes[2][2],row,23);
        assert(a!=b && b!=c && a!=c);
    }
    assert(EspUacFontTheme_isGrayRgb565(0xffffU));
    assert(EspUacFontTheme_isGrayRgb565(0x0000U));
    assert(!EspUacFontTheme_isGrayRgb565(0xf800U));
    assert(!EspUacFontTheme_isGrayRgb565(0x001fU));
    assert(EspUacFontTheme_mix565(0xf800U,0x001fU,0U,10U)==0xf800U);
    assert(EspUacFontTheme_mix565(0xf800U,0x001fU,10U,10U)==0x001fU);
    for(unsigned t=0;t<3;++t){
        const uint16_t top=themes[t][0],bottom=themes[t][2];
        assert(EspUacFontTheme_tintColor(0U,top)==0U);
        assert(EspUacFontTheme_tintColor(0xffffU,top)==top);
        assert(EspUacFontTheme_tintColor(0xf800U,bottom)==bottom);
        assert(EspUacFontTheme_tintColor(0x0001U,bottom)!=0U);
        assert(EspUacFontTheme_scale565(top,0U,1U)==0U);
        assert(EspUacFontTheme_scale565(top,1U,1U)==top);
        assert(EspUacFontTheme_scale565(top,2U,5U)!=top);
    }
    puts("UAC RGB565 THEME PASS 3 themes x 3 glyph heights, exact bounds, grayscale, black, gradients");
    return 0;
}
