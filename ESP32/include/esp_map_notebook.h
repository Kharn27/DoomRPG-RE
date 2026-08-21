#ifndef DOOMRPG_ESP32_MAP_NOTEBOOK_H
#define DOOMRPG_ESP32_MAP_NOTEBOOK_H

#include <stddef.h>
#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_ui_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_NOTEBOOK_CAPACITY 512U

typedef enum EspMapNotebookApplyStatus_e {
    ESP_MAP_NOTEBOOK_APPLY_INVALID = 0,
    ESP_MAP_NOTEBOOK_APPLY_UNSUPPORTED = 1,
    ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL = 2,
    ESP_MAP_NOTEBOOK_APPLY_IO_ERROR = 3,
    ESP_MAP_NOTEBOOK_APPLY_OK = 4
} EspMapNotebookApplyStatus;

typedef struct EspMapNotebookState_s {
    char text[ESP_MAP_NOTEBOOK_CAPACITY];
    uint16_t length;
} EspMapNotebookState;

/*
 * Native map-local notebook owner for opcode 40 / EV_NOTE.
 *
 * Legacy Player.NotebookString is a 512-byte C-string reset for each map and
 * EV_NOTE appends mapStringsIDs[arg1] followed by "||" through a bounded
 * snprintf. This owner preserves that 511-byte payload + trailing-NUL shape
 * without mutating Player_t or allocating heap memory.
 *
 * The caller supplies one bounded string-reader scratch buffer. The semantic
 * notebook is committed only after intent provenance and pack I/O succeed.
 */
void EspMapNotebook_reset(EspMapNotebookState* state);
uint16_t EspMapNotebook_length(const EspMapNotebookState* state);
const char* EspMapNotebook_text(const EspMapNotebookState* state);
EspMapNotebookApplyStatus EspMapNotebook_apply(
    EspMapNotebookState* state,
    const EspAssetPackEntry* sourceEntry,
    const EspMapUiIntent* intent,
    char* scratch,
    size_t scratchCapacity,
    size_t* outReadLength);

#ifdef __cplusplus
}
#endif

#endif
