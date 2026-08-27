#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_DIALOG_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_DIALOG_H

#include <stdint.h>

#include "esp_map_opcode_executor.h"
#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY 384U
#define ESP_NATIVE_GAMEPLAY_DIALOG_TYPE_MS 25U
#define ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES 4U

typedef enum EspNativeGameplayDialogBeginStatus_e {
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE = 2,
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME = 3,
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED = 4,
    ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK = 5
} EspNativeGameplayDialogBeginStatus;

typedef enum EspNativeGameplayDialogInputStatus_e {
    ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_IGNORED = 1,
    ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN = 2,
    ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_RESUME = 3,
    ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_CANCEL = 4
} EspNativeGameplayDialogInputStatus;

typedef enum EspNativeGameplayDialogResumeStatus_e {
    ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND = 1,
    ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED = 2,
    ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK = 3
} EspNativeGameplayDialogResumeStatus;

typedef struct EspNativeGameplayDialogClose_s {
    uint32_t runFlags;
    uint16_t sourceEventIndex;
    uint16_t resumeGlobalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t dialogCodeId;
    uint8_t resumeCodeId;
    uint8_t resumeHasCommand;
    uint8_t resumeRequested;
    uint8_t backAllowed;
    uint8_t removedBefore;
} EspNativeGameplayDialogClose;

typedef struct EspNativeGameplayDialogResumeResult_s {
    EspMapOpcodeExecResult opcode;
    uint16_t globalCommandIndex;
    uint8_t codeId;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t mutated;
    uint8_t rollbackAvailable;
    uint8_t reserved[3];
} EspNativeGameplayDialogResumeResult;

/*
 * One active native dialog session over the current resident BSP.
 *
 * begin() rebuilds the already hardware-proven EspMapUiIntent and
 * EspMapDialogOwner provenance, reads exactly one map string into a bounded
 * 384-byte buffer, and presents a 4-line dialog overlay. The native PAK stays
 * logically open only while the dialog is active so progressive font blits can
 * reuse the resident range cache; close/cancel always closes it before gameplay
 * rendering resumes.
 *
 * Only a zero-or-one state-op continuation (11/19/20) is accepted in this
 * milestone. That continuation is preflighted before the dialog becomes visible
 * so closing the dialog cannot reveal a partially unsupported script path.
 */
void EspNativeGameplayDialog_reset(void);
int EspNativeGameplayDialog_isActive(void);
EspNativeGameplayDialogBeginStatus EspNativeGameplayDialog_begin(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags);

/* Advance the recovered 25-ms/character typewriter and present only on change. */
int EspNativeGameplayDialog_tick(void);

/*
 * Consume gameplay semantic actions while the dialog owns input.
 * SELECT/PASS fast-forward, page, then close+resume. MOVE_FORWARD/BACK scroll.
 * For EV_DIALOG only, TURN_LEFT/RIGHT or MENU cancel through the recovered Back
 * behavior and deliberately do not resume the event continuation.
 */
EspNativeGameplayDialogInputStatus EspNativeGameplayDialog_handleAction(
    uint8_t action,
    EspNativeGameplayDialogClose* outClose);

/* Execute the preflighted continuation after CLOSE_RESUME. */
EspNativeGameplayDialogResumeStatus EspNativeGameplayDialog_resume(
    const EspNativeGameplayDialogClose* close,
    EspNativeGameplayDialogResumeResult* outResult);

/* Exact rollback for a successful mutating resume if the following redraw fails. */
int EspNativeGameplayDialog_rollbackResume(
    const EspNativeGameplayDialogResumeResult* result);

const char* EspNativeGameplayDialog_beginStatusName(
    EspNativeGameplayDialogBeginStatus status);
const char* EspNativeGameplayDialog_resumeStatusName(
    EspNativeGameplayDialogResumeStatus status);

#ifdef __cplusplus
}
#endif

#endif
