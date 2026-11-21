#ifndef ESP_UAC_MENU_PREVIEW_H
#define ESP_UAC_MENU_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* MENU_MAIN: direct 240 BGRA32 UAC default; procedural menu is fallback.
 * Activate once when MENU_MAIN owns touch; no logo cycling or 120 mode.
 * SD assets remain optional: invalid/missing files leave classic active. */
int EspUacMenuPreview_mode(void);
int EspUacMenuPreview_activate(void);
void EspUacMenuPreview_reset(void);
int EspUacMenuPreview_selectedIndex(void);
#ifdef __cplusplus
}
class TFT_eSPI;
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride);
#endif
#endif
