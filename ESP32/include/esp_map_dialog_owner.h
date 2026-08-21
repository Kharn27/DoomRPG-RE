#ifndef DOOMRPG_ESP32_MAP_DIALOG_OWNER_H
#define DOOMRPG_ESP32_MAP_DIALOG_OWNER_H

#include <stdint.h>

#include "esp_map_strings.h"
#include "esp_map_ui_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspMapDialogOwnerApplyStatus_e {
    ESP_MAP_DIALOG_OWNER_APPLY_INVALID = 0,
    ESP_MAP_DIALOG_OWNER_APPLY_UNSUPPORTED = 1,
    ESP_MAP_DIALOG_OWNER_APPLY_OK = 2
} EspMapDialogOwnerApplyStatus;

typedef struct EspMapDialogOwnerState_s {
    EspMapStringRef text;
    uint16_t sourceEventIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t flags;
    uint8_t active;
} EspMapDialogOwnerState;

/*
 * Compact caller-owned pause/presentation intent for EV_DIALOG (8) and
 * EV_DIALOGNOBACK (26).
 *
 * The owner retains only immutable map text identity plus static continuation
 * metadata. It does not copy text, read the native pack, mutate Game_t or
 * DoomCanvas, or present UI. A future native presenter reads state.text via
 * EspMapStrings_read(); a future event loop owns the dynamic execution flags
 * required to resume filtering.
 */
void EspMapDialogOwner_reset(EspMapDialogOwnerState* state);
int EspMapDialogOwner_isActive(const EspMapDialogOwnerState* state);
int EspMapDialogOwner_getRef(const EspMapDialogOwnerState* state,
                             EspMapStringRef* outRef);
EspMapDialogOwnerApplyStatus EspMapDialogOwner_apply(
    EspMapDialogOwnerState* state,
    const EspMapUiIntent* intent);

#ifdef __cplusplus
}
#endif

#endif
