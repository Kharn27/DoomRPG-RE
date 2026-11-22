#include "esp_uac_font_theme.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
int main(){
    constexpr uint16_t classic=0xff34U,inferno=0xfe78U,phosphore=0xc7b8U;
    assert(EspUacFontTheme_isGrayRgb565(0x0000U));
    assert(EspUacFontTheme_isGrayRgb565(0xffffU));
    assert(EspUacFontTheme_isGrayRgb565(0x8410U));
    assert(!EspUacFontTheme_isGrayRgb565(0xf800U));
    assert(!EspUacFontTheme_isGrayRgb565(0x07e0U));
    for(unsigned gray=0;gray<=255;++gray){
        const unsigned r=gray*31U/255U,g=gray*63U/255U,b=gray*31U/255U;
        const uint16_t src=(uint16_t)((r<<11U)|(g<<5U)|b);
        assert(EspUacFontTheme_isGrayRgb565(src));
        for(uint16_t tone:{classic,inferno,phosphore}){
            const uint16_t out=EspUacFontTheme_tintGray(src,tone);
            assert(((out>>11U)&31U)<=((tone>>11U)&31U));
            assert(((out>>5U)&63U)<=((tone>>5U)&63U));
            assert((out&31U)<=(tone&31U));
            assert(out==EspUacFontTheme_tintGray(src,tone));
        }
    }
    assert(EspUacFontTheme_tintGray(0U,classic)==0U);
    assert(EspUacFontTheme_tintGray(0xffffU,classic)==classic);
    assert(EspUacFontTheme_tintGray(0xffffU,inferno)==inferno);
    assert(EspUacFontTheme_tintGray(0xffffU,phosphore)==phosphore);
    assert(EspUacFontTheme_tintGray(0x8410U,inferno)!=
           EspUacFontTheme_tintGray(0x8410U,phosphore));
    /* Native letterTone(0/4/8,9) uses dark-to-light order. */
    const uint16_t stops[3][3]={
        {0xd225U,0xf5aaU,0xff34U}, /* CLASSIC: copper -> bronze -> yellow */
        {0xc002U,0xe30cU,0xfe78U}, /* INFERNO: dark red -> ember -> rose */
        {0x2638U,0x87e0U,0xc7b8U}, /* PHOSPHORE: turquoise -> green -> mint */
    };
    for(const auto& ramp:stops){
        for(int h:{19,21,23}){
            assert(EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],0,h)==ramp[0]);
            assert(EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],(h-1)/2,h)==ramp[1]);
            assert(EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],h-1,h)==ramp[2]);
            int changes=0;
            uint16_t before=ramp[0];
            for(int row=0;row<h;++row){
                const uint16_t c=EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],row,h);
                assert(EspUacFontTheme_tintGray(0U,c)==0U);
                assert(EspUacFontTheme_tintGray(0xffffU,c)==c);
                if(c!=before)++changes;
                before=c;
            }
            assert(changes>=h-5); /* genuinely graded, not three flat bands */
            assert(EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],-1,h)==0U);
            assert(EspUacFontTheme_verticalTone(ramp[0],ramp[1],ramp[2],h,h)==0U);
        }
    }
    assert(EspUacFontTheme_verticalTone(0xd225U,0xf5aaU,0xff34U,0,23) !=
           EspUacFontTheme_verticalTone(0x2638U,0x87e0U,0xc7b8U,0,23));
    puts("UAC neutral palette host PASS: 256 grayscale shades, 3 themes, 19/21/23 graded rows, black outlines");
}
