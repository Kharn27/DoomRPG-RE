#ifndef DOOMRPG_ESP32_MAP_PASSWORD_H
#define DOOMRPG_ESP32_MAP_PASSWORD_H

#include <stddef.h>
#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_events.h"
#include "esp_map_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_PASSWORD 10U
#define ESP_MAP_PASSWORD_INPUT_CAPACITY 8U
#define ESP_MAP_PASSWORD_MATCH_DELAY_MS 300U

#define ESP_MAP_PASSWORD_FLAG_PAUSE_SCRIPT       0x01U
#define ESP_MAP_PASSWORD_FLAG_SKIP_ADVANCE_TURN  0x02U
#define ESP_MAP_PASSWORD_FLAG_SAVE_CONTINUATION  0x04U
#define ESP_MAP_PASSWORD_FLAG_RESUME_ON_SUCCESS  0x08U
#define ESP_MAP_PASSWORD_EXPECTED_FLAGS           0x0fU

typedef enum EspMapPasswordOwnerStatus_e {
    ESP_MAP_PASSWORD_OWNER_INVALID = 0,
    ESP_MAP_PASSWORD_OWNER_UNSUPPORTED = 1,
    ESP_MAP_PASSWORD_OWNER_OK = 2
} EspMapPasswordOwnerStatus;

typedef enum EspMapPasswordSubmitStatus_e {
    ESP_MAP_PASSWORD_SUBMIT_INVALID = 0,
    ESP_MAP_PASSWORD_SUBMIT_BUFFER_TOO_SMALL = 1,
    ESP_MAP_PASSWORD_SUBMIT_IO_ERROR = 2,
    ESP_MAP_PASSWORD_SUBMIT_OK = 3
} EspMapPasswordSubmitStatus;

typedef enum EspMapPasswordOutcomeKind_e {
    ESP_MAP_PASSWORD_OUTCOME_NONE = 0,
    ESP_MAP_PASSWORD_OUTCOME_CORRECT = 1,
    ESP_MAP_PASSWORD_OUTCOME_INCORRECT = 2,
    ESP_MAP_PASSWORD_OUTCOME_EMPTY = 3
} EspMapPasswordOutcomeKind;

typedef struct EspMapPasswordOwnerState_s {
    EspMapStringRef expectedCode;
    EspMapStringRef prompt;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t flags;
    uint8_t active;
} EspMapPasswordOwnerState;

typedef struct EspMapPasswordSubmitResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t feedbackDelayMs;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t kind;
    uint8_t closeDialog;
    uint8_t resumeEvent;
    uint8_t forceStatusMessage;
} EspMapPasswordSubmitResult;

/*
 * Build the static pause/continuation owner for opcode 10 / EV_PASSWORD.
 * The owner retains only immutable map-string refs and source provenance; no
 * legacy DoomCanvas/Game state is touched and no text is copied.
 */
void EspMapPasswordOwner_reset(EspMapPasswordOwnerState* state);
int EspMapPasswordOwner_isActive(const EspMapPasswordOwnerState* state);
EspMapPasswordOwnerStatus EspMapPasswordOwner_apply(
    EspMapPasswordOwnerState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset);

/*
 * Evaluate one completed password submission without presentation or legacy
 * mutation. submittedLength is the payload length excluding NUL and must fit
 * the recovered 8-byte input buffer (max 7 bytes). The expected code is read
 * from the native pack through the proven bounded string reader. A submission
 * whose length equals the expected code length preserves the recovered 300 ms
 * feedback delay; an early SELECT-style shorter submission has zero delay.
 */
EspMapPasswordSubmitStatus EspMapPassword_evaluateSubmit(
    const EspAssetPackEntry* sourceEntry,
    const EspMapPasswordOwnerState* state,
    const char* submitted,
    size_t submittedLength,
    char* scratch,
    size_t scratchCapacity,
    size_t* outExpectedLength,
    EspMapPasswordSubmitResult* outResult);

/* Recovered forced status text for CORRECT / non-empty INCORRECT outcomes. */
const char* EspMapPassword_resultMessage(
    const EspMapPasswordSubmitResult* result);

#ifdef __cplusplus
}
#endif

#endif
