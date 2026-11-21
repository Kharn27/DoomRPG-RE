#include "platform_video_config.h"
#if DOOMRPG_RES60_P6_MENU
#include "esp_res60_menu.h"
#include "esp_res60_layout.h"
#include "esp_main_menu_skin.h"
#include "native_main_menu_160x120_layout.h"
#include <string.h>
/* Four existing 54x24 original industrial cards, translated +32,+12.
 * Explicit 160x120 view validates via existing skin; stride remains 192. */
static int validSelection(int8_t i) {
    return i >= -1 && i < ESP_RES60_MENU_ITEM_COUNT;
}
static uint16_t* menuView(uint16_t* fb) {
    return fb + ESP_RES60_MENU_VIEW_Y * ESP_RES60_WIDTH +
           ESP_RES60_MENU_VIEW_X;
}
int EspRes60Menu_render(uint16_t* fb, size_t bytes, int8_t selected,
                        EspRes60MenuReport* report) {
    unsigned item;
    if (report) memset(report, 0, sizeof(*report));
    if (!fb || !report || bytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        !validSelection(selected)) return 0;
    if (!EspRes60TitlePreview_renderAt(fb, bytes,
           ESP_RES60_MENU_LOGO_X, ESP_RES60_MENU_LOGO_Y, &report->title))
        return 0;
    for (item = 0; item < ESP_RES60_MENU_ITEM_COUNT; ++item)
        if (!EspMainMenuSkin_paint(menuView(fb), (int)ESP_RES60_WIDTH,
             160, 120, (int)item,
             selected == (int8_t)item ? ESP_MAIN_MENU_SKIN_ARMED :
             ESP_MAIN_MENU_SKIN_IDLE)) return 0;
    report->finalFNV = EspRes60Layout_fnv(fb, bytes);
    return report->finalFNV != 0U;
}
int EspRes60Menu_repaintSelection(uint16_t* fb, size_t bytes,
                                 int8_t previous, int8_t next) {
    if (!fb || bytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        !validSelection(previous) || !validSelection(next)) return 0;
    if (previous == next) return 1;
    if (previous >= 0 && !EspMainMenuSkin_paint(
          menuView(fb), (int)ESP_RES60_WIDTH, 160, 120, previous,
          ESP_MAIN_MENU_SKIN_IDLE)) return 0;
    if (next >= 0 && !EspMainMenuSkin_paint(
          menuView(fb), (int)ESP_RES60_WIDTH, 160, 120, next,
          ESP_MAIN_MENU_SKIN_ARMED)) return 0;
    return 1;
}
int EspRes60Menu_hitPhysical(int16_t x, int16_t y) {
    int vx, vy, item;
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
    vx = (int)((uint32_t)x * ESP_RES60_WIDTH / 320U) -
         ESP_RES60_MENU_VIEW_X;
    vy = (int)((uint32_t)y * ESP_RES60_HEIGHT / 240U) -
         ESP_RES60_MENU_VIEW_Y;
    for (item = 0; item < ESP_RES60_MENU_ITEM_COUNT; ++item) {
        int l, t, r, b;
        DoomRPG_esp32IndustrialMainCardRect(item, &l, &t, &r, &b);
        if (vx >= l && vx <= r && vy >= t && vy <= b) return item;
    }
    return -1;
}
EspRes60MenuTap EspRes60Menu_onTap(int16_t x, int16_t y,
                                   int8_t* selected) {
    int hit;
    if (!selected || !validSelection(*selected) ||
        x < 0 || x >= 320 || y < 0 || y >= 240)
        return ESP_RES60_MENU_TAP_INVALID;
    hit = EspRes60Menu_hitPhysical(x, y);
    if (hit < 0) {
        if (*selected == -1) return ESP_RES60_MENU_TAP_NO_CHANGE;
        *selected = -1;
        return ESP_RES60_MENU_TAP_SELECTION;
    }
    if (*selected != hit) {
        *selected = (int8_t)hit;
        return ESP_RES60_MENU_TAP_SELECTION;
    }
    return hit == 0 ? ESP_RES60_MENU_TAP_START_PREVIEW :
                      ESP_RES60_MENU_TAP_UNSUPPORTED;
}
#endif
