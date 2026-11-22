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
    puts("UAC neutral palette host PASS: 256 levels, three themes, dark outlines, bounded RGB565");
}
