#include "platform_video_config.h"
#if DOOMRPG_RES60_P7_NAV
#include "esp_res60_nav.h"
#include "esp_res60_layout.h"
#include "esp_main_menu_skin.h"
#include <stdint.h>

/* Compose proven 160x120 industrial skin in a translated bounded VIEW:
 * for top band x16,y0; bottom x16,y24. True framebuffer stride stays 192.
 * No extra buffer, no PAK read, no map mutation or physical LCD drawing. */
#define NAV_VIEW_X 16
#define NAV_W 160
#define NAV_H 120
#define NAV_BUTTON_Y 96
#define NAV_BUTTON_H 22
static const char* const directions[4]={"EAST","NORTH","WEST","SOUTH"};
static const char* const labels[3]={"LEFT","MENU","RIGHT"};
static const int buttonX[3]={4,56,108};
static const int buttonW=48;
EspRes60NavTap EspRes60Nav_hitPhysical(int16_t x, int16_t y) {
    int lx, ly, i;
    if (x < 0 || y < 0 || x >= 320 || y >= 240) return ESP_RES60_NAV_NONE;
    lx = (int)((uint32_t)x * ESP_RES60_WIDTH / 320U) - NAV_VIEW_X;
    ly = (int)((uint32_t)y * ESP_RES60_HEIGHT / 240U) -
         (int)ESP_RES60_WORLD_Y;
    for (i=0;i<3;++i) {
        if (lx >= buttonX[i] && lx < buttonX[i]+buttonW &&
            ly >= NAV_BUTTON_Y && ly < NAV_BUTTON_Y+NAV_BUTTON_H)
            return (EspRes60NavTap)(i+1);
    }
    return ESP_RES60_NAV_NONE;
}
int EspRes60Nav_applyTurn(uint8_t current, EspRes60NavTap tap,
                         uint8_t* next) {
    if (!next || current > 3U ||
        (tap != ESP_RES60_NAV_LEFT &&
         tap != ESP_RES60_NAV_RIGHT &&
         tap != ESP_RES60_NAV_MENU)) return 0;
    *next=(uint8_t)(tap==ESP_RES60_NAV_LEFT ? ((current+1U)&3U) :
                     tap==ESP_RES60_NAV_RIGHT ? ((current+3U)&3U) :
                     current);
    return 1;
}
int EspRes60Nav_resolveAngle(uint8_t spawnAngle, uint8_t turns,
                            uint8_t* angle) {
    if (!angle || (spawnAngle&63U)!=0U || turns>3U) return 0;
    *angle=(uint8_t)(spawnAngle+64U*turns);
    return 1;
}
int EspRes60Nav_paint(uint16_t* fb, size_t bytes,
                      uint8_t spawnAngle, uint8_t turns) {
    int i,y,x;uint8_t angle;
    uint16_t* top;uint16_t* bottom;
    if (!fb || bytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
        !EspRes60Nav_resolveAngle(spawnAngle,turns,&angle)) return 0;
    /* P3 diagnostic red/orange strips are replaced by a small native
     * chassis; all 192x96 MAP_INTRO wall pixels stay byte-identical. */
    for (y=0;y<(int)ESP_RES60_HUD_TOP_ROWS;++y)
        for (x=0;x<(int)ESP_RES60_WIDTH;++x)
            fb[(size_t)y*ESP_RES60_WIDTH+x]=0x0800U;
    for (y=(int)ESP_RES60_HUD_BOTTOM_Y;y<(int)ESP_RES60_HEIGHT;++y)
        for (x=0;x<(int)ESP_RES60_WIDTH;++x)
            fb[(size_t)y*ESP_RES60_WIDTH+x]=0x0800U;
    top=fb+NAV_VIEW_X;
    bottom=fb+(size_t)ESP_RES60_WORLD_Y*ESP_RES60_WIDTH+NAV_VIEW_X;
    if (!EspMainMenuSkin_paintPlate(top,ESP_RES60_WIDTH,NAV_W,NAV_H,
                                    4,1,155,22,ESP_MAIN_MENU_SKIN_IDLE) ||
        !EspMainMenuSkin_paintText(top,ESP_RES60_WIDTH,NAV_W,NAV_H,
                                   4,1,155,22,directions[angle/64U],0,1))
        return 0;
    for (i=0;i<3;++i) {
        int l=buttonX[i], r=l+buttonW-1;
        if (!EspMainMenuSkin_paintPlate(bottom,ESP_RES60_WIDTH,NAV_W,NAV_H,
             l,NAV_BUTTON_Y,r,NAV_BUTTON_Y+NAV_BUTTON_H-1,
             ESP_MAIN_MENU_SKIN_IDLE) ||
            !EspMainMenuSkin_paintText(bottom,ESP_RES60_WIDTH,NAV_W,NAV_H,
             l,NAV_BUTTON_Y,r,NAV_BUTTON_Y+NAV_BUTTON_H-1,
             labels[i],0,1)) return 0;
    }
    return 1;
}
#endif
