#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASSWORD_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASSWORD_H

#include <stdint.h>

#include "esp_native_gameplay_dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_PASSWORD 10U
#define ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE 16U
#define ESP_NATIVE_GAMEPLAY_PASSWORD_PROMPT_CAPACITY 384U

typedef enum EspNativeGameplayPasswordBeginStatus_e {
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_TEXT_TOO_LARGE = 2,
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_UNSUPPORTED_RESUME = 3,
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_IO_FAILED = 4,
    ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_OK = 5
} EspNativeGameplayPasswordBeginStatus;

typedef enum EspNativeGameplayPasswordTapStatus_e {
    ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_IGNORED = 1,
    ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_REDRAWN = 2,
    ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_SUBMITTED = 3
} EspNativeGameplayPasswordTapStatus;

typedef struct EspNativeGameplayPasswordCompletion_s {
    EspNativeGameplayDialogClose close;
    uint16_t expectedLength;
    uint16_t enteredLength;
    uint8_t correct;
    uint8_t hadInput;
    uint8_t pending;
    uint8_t reserved;
} EspNativeGameplayPasswordCompletion;

/*
 * Native EV_PASSWORD modal. The BSP keeps both strings in the native PAK:
 * arg1 low byte is the expected numeric code and arg1 high byte is the prompt.
 * The owner materializes only those two bounded strings and one indexed-font
 * descriptor. No legacy DoomCanvas/Game password state is touched.
 */
void EspNativeGameplayPassword_reset(void);
int EspNativeGameplayPassword_isActive(void);

EspNativeGameplayPasswordBeginStatus EspNativeGameplayPassword_begin(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags);

/* Logical 160x120 coordinates. The modal owns a 3x4 touch keypad:
 * 1..9 / DEL,0,VALID. Digits and DEL repaint immediately; VALID publishes one
 * completion for the resident gameplay owner to resume or reject. */
EspNativeGameplayPasswordTapStatus EspNativeGameplayPassword_handleTap(
    int logicalX,
    int logicalY);

int EspNativeGameplayPassword_takeCompletion(
    EspNativeGameplayPasswordCompletion* outCompletion);

const char* EspNativeGameplayPassword_beginStatusName(
    EspNativeGameplayPasswordBeginStatus status);

#ifdef __cplusplus
}
#endif

#endif
