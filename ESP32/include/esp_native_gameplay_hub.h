#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayHubStatus_e {
    ESP_NATIVE_GAMEPLAY_HUB_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_HUB_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY = 2,
    ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED = 3,
    ESP_NATIVE_GAMEPLAY_HUB_IGNORED = 4,
    ESP_NATIVE_GAMEPLAY_HUB_REDRAWN = 5,
    ESP_NATIVE_GAMEPLAY_HUB_CLOSED = 6,
    ESP_NATIVE_GAMEPLAY_HUB_OK = 7
} EspNativeGameplayHubStatus;

typedef enum EspNativeGameplayHubPage_e {
    ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY = 0,
    ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS = 1,
    ESP_NATIVE_GAMEPLAY_HUB_PAGE_COUNT = 2
} EspNativeGameplayHubPage;

typedef struct EspNativeGameplayHubView_s {
    uint32_t opens;
    uint32_t closes;
    uint32_t paints;
    uint32_t playerFNVAtOpen;
    uint32_t lastPlayerFNV;
    uint32_t lastFrameFNV;
    uint8_t selectedRow;
    uint8_t active;
    uint8_t page;
    uint8_t reserved;
} EspNativeGameplayHubView;

/*
 * Permanent bounded gameplay-hub owner.
 *
 * v2 projects two read-only views from the one canonical PlayerState owner:
 * Inventory and Status. It never duplicates or mutates PlayerState and never
 * retains a framebuffer snapshot. The hub may paint only the resident 160x80
 * world viewport (y=20..99); the native HUD bands remain untouched and the
 * world is restored by the resident gameplay renderer after HUB_CLOSE.
 */
void EspNativeGameplayHub_reset(void);
int EspNativeGameplayHub_isActive(void);
const EspNativeGameplayHubView* EspNativeGameplayHub_view(void);

EspNativeGameplayHubStatus EspNativeGameplayHub_open(void);
EspNativeGameplayHubStatus EspNativeGameplayHub_handleAction(uint8_t action);
const char* EspNativeGameplayHub_statusName(EspNativeGameplayHubStatus status);

#ifdef __cplusplus
}
#endif

#endif