#ifndef DOOMRPG_ESP32_UAC_FONT_THEME_H
#define DOOMRPG_ESP32_UAC_FONT_THEME_H
#include <stdint.h>

/* Tiny, pure RGB565 helpers: no heap, palette LUT, SD or frame buffer. */
static inline int EspUacFontTheme_isGrayRgb565(uint16_t c){
    const unsigned r=((unsigned)(c>>11U)&31U)*255U/31U;
    const unsigned g=((unsigned)(c>>5U)&63U)*255U/63U;
    const unsigned b=((unsigned)c&31U)*255U/31U;
    const unsigned rg=r>g?r-g:g-r;
    const unsigned gb=g>b?g-b:b-g;
    const unsigned rb=r>b?r-b:b-r;
    return rg<=12U && gb<=12U && rb<=12U;
}
/* Interpolate RGB565 channels between the three authoritative theme inks,
 * preserving dark-to-light direction and a smooth gradient per physical row.
 * Height is strictly bounded by the caller's 19/21/23-pixel glyph paint. */
static inline uint16_t EspUacFontTheme_mix565(uint16_t a,uint16_t b,
                                               unsigned at,unsigned span){
    if(span==0U||at>span)return 0U;
    const unsigned before=span-at;
    const unsigned ar=(a>>11U)&31U,ag=(a>>5U)&63U,ab=a&31U;
    const unsigned br=(b>>11U)&31U,bg=(b>>5U)&63U,bb=b&31U;
    const unsigned r=(ar*before+br*at+span/2U)/span;
    const unsigned g=(ag*before+bg*at+span/2U)/span;
    const unsigned blue=(ab*before+bb*at+span/2U)/span;
    return (uint16_t)((r<<11U)|(g<<5U)|blue);
}
static inline uint16_t EspUacFontTheme_verticalTone(
    uint16_t top,uint16_t mid,uint16_t bottom,int row,int rows){
    if(rows<2||row<0||row>=rows)return 0U;
    const unsigned span=(unsigned)(rows-1);
    const unsigned twice=(unsigned)row*2U;
    return twice<=span?
        EspUacFontTheme_mix565(top,mid,twice,span):
        EspUacFontTheme_mix565(mid,bottom,twice-span,span);
}
static inline uint16_t EspUacFontTheme_tintGray(uint16_t src,uint16_t tone){
    /* The green (6-bit) channel carries the grayscale ink intensity. */
    const unsigned light=((unsigned)src>>5U)&63U;
    const unsigned r=(((unsigned)tone>>11U)&31U)*light/63U;
    const unsigned g=(((unsigned)tone>>5U)&63U)*light/63U;
    const unsigned b=((unsigned)tone&31U)*light/63U;
    return (uint16_t)((r<<11U)|(g<<5U)|b);
}
#endif
