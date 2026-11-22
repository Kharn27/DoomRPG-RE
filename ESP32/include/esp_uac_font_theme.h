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
static inline uint16_t EspUacFontTheme_tintGray(uint16_t src,uint16_t tone){
    /* The green (6-bit) channel carries the grayscale ink intensity. */
    const unsigned light=((unsigned)src>>5U)&63U;
    const unsigned r=(((unsigned)tone>>11U)&31U)*light/63U;
    const unsigned g=(((unsigned)tone>>5U)&63U)*light/63U;
    const unsigned b=((unsigned)tone&31U)*light/63U;
    return (uint16_t)((r<<11U)|(g<<5U)|b);
}
#endif
