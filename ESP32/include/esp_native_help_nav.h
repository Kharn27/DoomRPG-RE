#ifndef DOOMRPG_ESP32_NATIVE_HELP_NAV_H
#define DOOMRPG_ESP32_NATIVE_HELP_NAV_H

/* Pure 160x120 HELP touch routing, shared with the host boundary oracle.
 * The close box wins over left-half pagination, including its border.
 * This never mutates menu state, allocates or reads physical input. */
#define ESP_NATIVE_HELP_WIDTH 160
#define ESP_NATIVE_HELP_HEIGHT 120
#define ESP_NATIVE_HELP_PAGE_LINES 7
#define ESP_NATIVE_HELP_CLOSE_RIGHT 17
#define ESP_NATIVE_HELP_CLOSE_BOTTOM 14

typedef enum EspNativeHelpNavAction_e {
    ESP_NATIVE_HELP_NAV_NONE = 0,
    ESP_NATIVE_HELP_NAV_CLOSE = 1,
    ESP_NATIVE_HELP_NAV_PREVIOUS = 2,
    ESP_NATIVE_HELP_NAV_NEXT = 3
} EspNativeHelpNavAction;

static inline EspNativeHelpNavAction EspNativeHelpNav_hit(int x, int y) {
    if (x < 0 || x >= ESP_NATIVE_HELP_WIDTH ||
        y < 0 || y >= ESP_NATIVE_HELP_HEIGHT)
        return ESP_NATIVE_HELP_NAV_NONE;
    if (x <= ESP_NATIVE_HELP_CLOSE_RIGHT &&
        y <= ESP_NATIVE_HELP_CLOSE_BOTTOM)
        return ESP_NATIVE_HELP_NAV_CLOSE;
    return x < ESP_NATIVE_HELP_WIDTH / 2 ?
        ESP_NATIVE_HELP_NAV_PREVIOUS : ESP_NATIVE_HELP_NAV_NEXT;
}

static inline int EspNativeHelpNav_scroll(int before, int total,
                                           EspNativeHelpNavAction action) {
    const int maxScroll = total > ESP_NATIVE_HELP_PAGE_LINES ?
                          total - ESP_NATIVE_HELP_PAGE_LINES : 0;
    if (before < 0) before = 0;
    if (before > maxScroll) before = maxScroll;
    if (action == ESP_NATIVE_HELP_NAV_PREVIOUS) {
        return before >= ESP_NATIVE_HELP_PAGE_LINES ?
               before - ESP_NATIVE_HELP_PAGE_LINES : 0;
    }
    if (action == ESP_NATIVE_HELP_NAV_NEXT) {
        return before <= maxScroll - ESP_NATIVE_HELP_PAGE_LINES ?
               before + ESP_NATIVE_HELP_PAGE_LINES : maxScroll;
    }
    return before;
}

#endif /* DOOMRPG_ESP32_NATIVE_HELP_NAV_H */
