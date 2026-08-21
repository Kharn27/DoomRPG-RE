#ifndef DOOMRPG_ESP32_MAP_STRINGS_H
#define DOOMRPG_ESP32_MAP_STRINGS_H

#include <stddef.h>
#include <stdint.h>

#include "esp_asset_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspMapStringRef_s {
    uint16_t index;
    uint16_t sourceOffset;
    uint16_t length;
} EspMapStringRef;

typedef enum EspMapStringReadStatus_e {
    ESP_MAP_STRING_READ_INVALID = 0,
    ESP_MAP_STRING_READ_BUFFER_TOO_SMALL = 1,
    ESP_MAP_STRING_READ_IO_ERROR = 2,
    ESP_MAP_STRING_READ_OK = 3
} EspMapStringReadStatus;

/*
 * Resolve one BSP map string to its payload span inside the current source
 * entry. The string bytes remain on the native asset pack; this API allocates
 * nothing and never materializes map-wide text.
 */
int EspMapStrings_getRef(uint32_t index, EspMapStringRef* outRef);

/*
 * Read one already-resolved string payload from the current map's native-pack
 * entry into caller-owned storage. The source entry and ref must match the
 * currently loaded EspMapRuntime source identity. On success the payload is
 * followed by a synthesized C-string terminator; capacity must therefore be at
 * least ref->length + 1. No allocation, decompression or legacy ZIP access is
 * performed here.
 */
EspMapStringReadStatus EspMapStrings_read(
    const EspAssetPackEntry* sourceEntry,
    const EspMapStringRef* ref,
    char* destination,
    size_t capacity,
    size_t* outLength);

#ifdef __cplusplus
}
#endif

#endif
