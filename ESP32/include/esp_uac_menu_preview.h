#ifndef ESP_UAC_MENU_PREVIEW_H
#define ESP_UAC_MENU_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* MENU_MAIN only: generic-120 is the default, classic=0 is fail-closed
 * fallback when SD assets, SD/PAK font or physical scanout are unusable.
 * Optional logo comparison cycles modes 1,2,3; never enters classic. */
int EspUacMenuPreview_mode(void);
/* Allocation-free menu-only clock guard; asks loop to present one selected
 * 110x40 patch at the next dark/light phase, never during gameplay. */
int EspUacMenuPreview_needsBlink(uint32_t nowMs);
/* No-allocation marker set during MENU_MAIN touch activation.
 * The first subsequent video present (after root heap/FNV checks) consumes it. */
void EspUacMenuPreview_armDefault(void);
int EspUacMenuPreview_enterDefault(void);
int EspUacMenuPreview_cycle(void);
void EspUacMenuPreview_reset(void);
int EspUacMenuPreview_selectedIndex(void);
/* Physical-only UAC submenu: no change to original 160x120 render/model. */
enum {
    ESP_UAC_OPTIONS_NONE=0, ESP_UAC_OPTIONS_ROOT=1,
    ESP_UAC_OPTIONS_VIDEO=2, ESP_UAC_OPTIONS_BRIGHTNESS=3,
    ESP_UAC_OPTIONS_ORIENTATION=4, ESP_UAC_OPTIONS_THEME=5,
    ESP_UAC_OPTIONS_SOUND_ON=6, ESP_UAC_OPTIONS_SOUND_OFF=7,
    ESP_UAC_OPTIONS_VOLUME=8
};
void EspUacMenuPreview_armOptions(void);
int EspUacMenuPreview_enterOptions(void);
int EspUacMenuPreview_optionsActive(void);
int EspUacMenuPreview_optionsHit(int16_t physicalX,int16_t physicalY);
void EspUacMenuPreview_optionsPage(int page,int selected,int armed,
                                    uint8_t enabledMask);
#ifdef __cplusplus
}
class TFT_eSPI;
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride);
#endif
#endif
