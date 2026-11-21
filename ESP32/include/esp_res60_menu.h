#ifndef DOOMRPG_ESP32_RES60_MENU_H
#define DOOMRPG_ESP32_RES60_MENU_H
#include <stddef.h>
#include <stdint.h>
#include "esp_res60_title_preview.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The original 160x120 industrial skin is drawn into an interior window
 * with stride=192. This is a VIEW into the ONE 192x144 framebuffer,
 * not a second framebuffer or a new menu engine. */
#define ESP_RES60_MENU_VIEW_X 32
#define ESP_RES60_MENU_VIEW_Y 12
#define ESP_RES60_MENU_LOGO_X 12
#define ESP_RES60_MENU_LOGO_Y 35
#define ESP_RES60_MENU_ITEM_COUNT 4

typedef struct EspRes60MenuReport_s {
    EspRes60TitlePreviewReport title;
    uint32_t finalFNV;
} EspRes60MenuReport;

typedef enum EspRes60MenuTap_e {
    ESP_RES60_MENU_TAP_INVALID = 0,
    ESP_RES60_MENU_TAP_NO_CHANGE = 1,
    ESP_RES60_MENU_TAP_SELECTION = 2,
    ESP_RES60_MENU_TAP_START_PREVIEW = 3,
    ESP_RES60_MENU_TAP_UNSUPPORTED = 4
} EspRes60MenuTap;

/* Real PAK c.bmp/j.bmp and native START/LOAD/OPTIONS/HELP card art. */
int EspRes60Menu_render(uint16_t* fb, size_t bytes, int8_t selected,
                        EspRes60MenuReport* report);
/* Repaint changed cards in place, without reopening PAK or allocating. */
int EspRes60Menu_repaintSelection(uint16_t* fb, size_t bytes,
                                 int8_t previous, int8_t next);
/* Physical 320x240 calibrated touch -> 192x144 floor inverse -> skin slots. */
int EspRes60Menu_hitPhysical(int16_t physicalX, int16_t physicalY);
/* Two-tap navigation; START opens P3 geometry only, NOT gameplay. */
EspRes60MenuTap EspRes60Menu_onTap(int16_t x, int16_t y, int8_t* selected);
#ifdef __cplusplus
}
#endif
#endif
