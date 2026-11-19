#ifndef ESP_UAC_MENU_PREVIEW_H
#define ESP_UAC_MENU_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* MENU_MAIN only: classic=0, 120 exact nearest x2=1, physical 240 native=2.
 * Removable SD assets are OPTIONAL: missing or malformed -> classic. */
int EspUacMenuPreview_mode(void);
int EspUacMenuPreview_cycle(void);
void EspUacMenuPreview_reset(void);
int EspUacMenuPreview_selectedIndex(void);
#ifdef __cplusplus
}
class TFT_eSPI;
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride);
#endif
#endif
