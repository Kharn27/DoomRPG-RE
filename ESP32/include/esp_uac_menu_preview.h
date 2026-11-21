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
/* No-allocation marker set during MENU_MAIN touch activation.
 * The first subsequent video present (after root heap/FNV checks) consumes it. */
void EspUacMenuPreview_armDefault(void);
int EspUacMenuPreview_enterDefault(void);
int EspUacMenuPreview_cycle(void);
void EspUacMenuPreview_reset(void);
int EspUacMenuPreview_selectedIndex(void);
#ifdef __cplusplus
}
class TFT_eSPI;
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride);
#endif
#endif
