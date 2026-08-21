#ifndef DOOMRPG_ESP32_MAP_UI_INTENT_H
#define DOOMRPG_ESP32_MAP_UI_INTENT_H

#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_DIALOG 8U
#define ESP_MAP_OPCODE_FORCE_MESSAGE 24U
#define ESP_MAP_OPCODE_DIALOG_NO_BACK 26U
#define ESP_MAP_OPCODE_NOTE 40U

#define ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK 0x01U
#define ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT 0x02U
#define ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN 0x04U
#define ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY 0x08U
#define ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR 0x10U

typedef enum EspMapUiIntentKind_e {
    ESP_MAP_UI_INTENT_NONE = 0,
    ESP_MAP_UI_INTENT_DIALOG = 1,
    ESP_MAP_UI_INTENT_FORCE_MESSAGE = 2,
    ESP_MAP_UI_INTENT_APPEND_NOTE = 3
} EspMapUiIntentKind;

typedef enum EspMapUiIntentStatus_e {
    ESP_MAP_UI_INTENT_INVALID = 0,
    ESP_MAP_UI_INTENT_UNSUPPORTED = 1,
    ESP_MAP_UI_INTENT_STRING_NOT_FOUND = 2,
    ESP_MAP_UI_INTENT_OK = 3
} EspMapUiIntentStatus;

typedef struct EspMapUiIntent_s {
    EspMapStringRef text;
    uint32_t arg1;
    uint32_t arg2;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t resumeCommandOffset;
    uint8_t codeId;
    uint8_t kind;
    uint8_t flags;
    uint8_t status;
} EspMapUiIntent;

/*
 * Translate the recovered UI/string opcode family into a compact native
 * effect intent. No legacy DoomCanvas/Hud/Player state is touched here.
 *
 * Dialog intents preserve the recovered pause/resume semantics by carrying the
 * source event + command offset and setting PAUSE_SCRIPT/SKIP_ADVANCE_TURN.
 * FORCE_MESSAGE represents empty-string clearing explicitly. NOTE becomes an
 * APPEND_NOTE intent; the future player-owned notebook consumer applies the
 * legacy "text + ||" append semantics.
 */
int EspMapUiIntent_supports(uint8_t codeId);
EspMapUiIntentStatus EspMapUiIntent_build(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapUiIntent* outIntent);

#ifdef __cplusplus
}
#endif

#endif
