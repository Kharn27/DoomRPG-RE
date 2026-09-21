#ifndef ESP_NATIVE_GAMEPLAY_SAVE_UI_H
#define ESP_NATIVE_GAMEPLAY_SAVE_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Authoritative SAVE/LOAD row owned by the native checkpoint UI.
 * 0 = SAVE, 1 = LOAD.
 *
 * Touch routing must consult this owner instead of mirroring its own cursor:
 * physical controls can move the status cursor before a direct touch select.
 */
uint8_t EspNativeGameplaySave_statusCursor(void);

#ifdef __cplusplus
}
#endif

#endif
