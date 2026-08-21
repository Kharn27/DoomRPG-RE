#ifndef DOOMRPG_ESP32_MAP_STATUS_MESSAGE_H
#define DOOMRPG_ESP32_MAP_STATUS_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_ui_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspMapStatusMessageState_s {
    EspMapStringRef text;
    uint8_t active;
    uint8_t reserved;
} EspMapStatusMessageState;

typedef enum EspMapStatusMessageApplyStatus_e {
    ESP_MAP_STATUS_MESSAGE_APPLY_INVALID = 0,
    ESP_MAP_STATUS_MESSAGE_APPLY_UNSUPPORTED = 1,
    ESP_MAP_STATUS_MESSAGE_APPLY_BUFFER_TOO_SMALL = 2,
    ESP_MAP_STATUS_MESSAGE_APPLY_IO_ERROR = 3,
    ESP_MAP_STATUS_MESSAGE_APPLY_OK = 4
} EspMapStatusMessageApplyStatus;

/*
 * Compact native owner for EV_FORCEMESSAGE semantics.
 *
 * The legacy engine stores Hud.statBarMessage as a pointer into the current
 * map-wide string allocation. The native equivalent owns only the immutable
 * EspMapStringRef plus an active bit. Text bytes remain in the native pack.
 *
 * This is a caller-owned value type: no allocation, global buffer or legacy
 * Hud/Render mutation occurs in this module.
 */
void EspMapStatusMessage_reset(EspMapStatusMessageState* state);
int EspMapStatusMessage_isActive(const EspMapStatusMessageState* state);
int EspMapStatusMessage_getRef(const EspMapStatusMessageState* state,
                               EspMapStringRef* outRef);

/*
 * Consume only a validated EV_FORCEMESSAGE intent.
 *
 * scratch is transient caller storage used by EspMapStrings_read(). State is
 * committed only after the bounded read succeeds, so invalid/unsupported/read
 * failures leave the owner unchanged. The recovered legacy empty semantics are
 * exact: first payload byte '\0' clears the status message; otherwise the
 * canonical string ref becomes active.
 */
EspMapStatusMessageApplyStatus EspMapStatusMessage_apply(
    EspMapStatusMessageState* state,
    const EspAssetPackEntry* sourceEntry,
    const EspMapUiIntent* intent,
    char* scratch,
    size_t scratchCapacity,
    size_t* outReadLength);

#ifdef __cplusplus
}
#endif

#endif
