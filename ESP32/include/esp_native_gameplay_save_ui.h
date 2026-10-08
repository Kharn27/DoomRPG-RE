#ifndef ESP_NATIVE_GAMEPLAY_SAVE_UI_H
#define ESP_NATIVE_GAMEPLAY_SAVE_UI_H

#include <stdint.h>

#include "esp_map_save_route.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Authoritative SYS row owned by the native checkpoint UI.
 * 0 = SAVE, 1 = LOAD, 2 = EXIT TO MENU.
 *
 * Touch routing must consult this owner instead of mirroring its own cursor:
 * physical controls can move the status cursor before a direct touch select.
 */
uint8_t EspNativeGameplaySave_statusCursor(void);
int EspNativeGameplaySave_slotSelectorActive(void);
/* Returns 0=not claimed, 1=consumed, 2=dispatch SELECT. */
int EspNativeGameplaySave_touchSlot(int logicalX, int logicalY);

/* One layout contract for painting and touch: 24 logical pixels = 48 physical
 * pixels per row. No title/slot number consumes the reclaimed SYS space. */
enum {
    ESP_NATIVE_SYS_SAVE = 0,
    ESP_NATIVE_SYS_LOAD = 1,
    ESP_NATIVE_SYS_EXIT = 2,
    ESP_NATIVE_SYS_COUNT = 3,
    ESP_NATIVE_SYS_BUTTON_LEFT = 16,
    ESP_NATIVE_SYS_BUTTON_RIGHT = 143,
    ESP_NATIVE_SYS_BUTTON_TOP = 39,
    ESP_NATIVE_SYS_BUTTON_HEIGHT = 24,
    ESP_NATIVE_SYS_BUTTON_GAP = 2
};

/* Shared checkpoint entry points used by both HUB -> SYS and MENU_MAIN.
 * Load rebuilds the native resident world and arms a resumed gameplay session. */
int EspNativeGameplaySave_hasReadableCheckpoint(void);
int EspNativeGameplaySave_loadCheckpoint(void);

/*
 * Durable in-memory legacy EV_SAVEGAME return route.
 *
 * This owner is deliberately outside the map/session reset lifetime: a
 * SAVEGAME immediately followed by CHANGEMAP must survive destruction of the
 * source transition/input owner. The checkpoint format does not yet serialize
 * this route, but the SAVE subsystem owns the live copy so later save work can
 * consume it without retaining a source-map string ref.
 */
int EspNativeGameplaySave_adoptTransitionRoute(
    const EspMapSaveRouteState* route);
const EspMapSaveRouteState* EspNativeGameplaySave_transitionRoute(void);
void EspNativeGameplaySave_clearTransitionRoute(void);

#ifdef __cplusplus
}
#endif

#endif
