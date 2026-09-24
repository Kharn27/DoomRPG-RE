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

/* Non-mutating full continuation proof for modal owners such as EV_PASSWORD
 * that do not need the compact dialog owner's temporary removed-bit mask. */
EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_preflight(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint32_t runFlags);

/* Bounded synchronous prefix proof used when another pause boundary (for
 * example DIALOG/DIALOGNOBACK) appears later in the same event. The end offset
 * is exclusive and no command at/after it is inspected or executed. */
EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_preflightRange(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint16_t endCommandOffsetExclusive,
    uint32_t runFlags);

/*
 * A dialog reached after another pause boundary (currently EV_PASSWORD) is not
 * a synchronous chain command: legacy pauses again at that dialog.  These APIs
 * prove/open that exact command without replaying any already-handled prefix.
 * The dialog's own post-dialog continuation is still fully preflighted and
 * remains fail-closed.
 */
EspNativeGameplayDialogBeginStatus
EspNativeGameplayEventChain_preflightDialogCommand(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags);

EspNativeGameplayDialogBeginStatus
EspNativeGameplayEventChain_beginDialogCommand(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags);

/* Execute one already-preflightable synchronous event suffix without a dialog
 * owner. Used by SELECT families such as EV_GIVEMAP that are legacy
 * synchronous commands. The same rollback journal backs render failure. */
EspNativeGameplayDialogResumeStatus
EspNativeGameplayEventChain_execute(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags,
    EspNativeGameplayDialogResumeResult* outResult);

EspNativeGameplayDialogResumeStatus
EspNativeGameplayEventChain_executeRange(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint16_t endCommandOffsetExclusive,
    uint32_t runFlags,
    EspNativeGameplayDialogResumeResult* outResult);

/* One-shot diagnostic over the resident event corpus. Allocation-free and
 * mutation-free; intended to tell hardware testing which opcode families will
 * still fail closed before the player reaches them. */
void EspNativeGameplayEventChain_logCorpus(void);

#ifdef __cplusplus
}
#endif

#endif
