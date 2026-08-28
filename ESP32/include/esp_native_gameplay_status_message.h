#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_STATUS_MESSAGE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_STATUS_MESSAGE_H

#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_status_message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayStatusMessageApplyStatus_e {
    ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_UNSUPPORTED = 2,
    ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_IO_FAILED = 3,
    ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_OK = 4
} EspNativeGameplayStatusMessageApplyStatus;

typedef struct EspNativeGameplayStatusMessageResult_s {
    EspMapStatusMessageState before;
    EspMapStatusMessageState after;
    uint16_t eventIndex;
    uint16_t globalCommandIndex;
    uint8_t commandOffset;
    uint8_t codeId;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t ownerChanged;
    uint8_t removeIfHandled;
    uint8_t rollbackAvailable;
    uint8_t reserved;
} EspNativeGameplayStatusMessageResult;

/* Permanent gameplay owner for legacy Hud.statBarMessage fallback semantics. */
void EspNativeGameplayStatusMessage_reset(void);
int EspNativeGameplayStatusMessage_isReady(void);
const EspMapStatusMessageState* EspNativeGameplayStatusMessage_view(void);

/* Execute exactly one already-filtered EV_FORCEMESSAGE command. */
EspNativeGameplayStatusMessageApplyStatus EspNativeGameplayStatusMessage_apply(
    const EspMapEventDescriptor* descriptor,
    uint8_t commandOffset,
    EspNativeGameplayStatusMessageResult* outResult);

int EspNativeGameplayStatusMessage_rollback(
    const EspNativeGameplayStatusMessageResult* result);

/* Repaint only the 160x20 legacy top-bar fallback when the owner changed.
 * No present occurs here; the enclosing gameplay frame owns presentation. */
int EspNativeGameplayStatusMessage_paintIfDirty(void);

/* One allocation-free diagnostic dump of the resident FORCE_MESSAGE corpus. */
void EspNativeGameplayStatusMessage_logCorpus(void);

const char* EspNativeGameplayStatusMessage_applyStatusName(
    EspNativeGameplayStatusMessageApplyStatus status);

#ifdef __cplusplus
}
#endif

#endif
