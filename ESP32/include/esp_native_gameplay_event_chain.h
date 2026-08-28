#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_EVENT_CHAIN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_EVENT_CHAIN_H

#include <stdint.h>

#include "esp_native_gameplay_dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS 12U

typedef enum EspNativeGameplayEventChainPreflightStatus_e {
    ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_UNSUPPORTED = 2,
    ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK = 3
} EspNativeGameplayEventChainPreflightStatus;

typedef struct EspNativeGameplayEventChainMask_s {
    uint16_t global[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    uint8_t removedBefore[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    uint8_t count;
    uint8_t active;
    uint8_t reserved[2];
} EspNativeGameplayEventChainMask;

/*
 * Dialog begin still contains the older one-state-op resume preflight.  This
 * bridge first proves the entire post-dialog continuation against the current
 * bounded production families, then temporarily masks those already-proven
 * commands while the legacy-sized dialog owner is opened.  The mask is always
 * restored immediately; no command is executed by this preflight.
 */
EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_maskForDialogBegin(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint32_t runFlags,
    EspNativeGameplayEventChainMask* outMask);

int EspNativeGameplayEventChain_restoreDialogMask(
    EspNativeGameplayEventChainMask* mask);

/* One-shot diagnostic over the resident event corpus. Allocation-free and
 * mutation-free; intended to tell hardware testing which opcode families will
 * still fail closed before the player reaches them. */
void EspNativeGameplayEventChain_logCorpus(void);

#ifdef __cplusplus
}
#endif

#endif
