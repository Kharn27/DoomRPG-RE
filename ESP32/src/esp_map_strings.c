#include <stddef.h>
#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_strings.h"

#define STRING_LENGTH_PREFIX_BYTES 2U

int EspMapStrings_getRef(uint32_t index, EspMapStringRef* outRef) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint16_t sourceOffset;
    uint16_t nextSourceOffset;
    uint32_t stringDataEnd;
    uint32_t length;

    if (outRef == NULL || runtime == NULL ||
        index >= runtime->stringCount || index > 0xffffU ||
        runtime->sourceBytes < runtime->blockMapBytes + runtime->planeMapBytes ||
        !EspMapRuntime_getStringSourceOffset(index, &sourceOffset)) {
        return 0;
    }

    stringDataEnd = runtime->sourceBytes -
                    runtime->blockMapBytes - runtime->planeMapBytes;
    if ((uint32_t)sourceOffset > stringDataEnd) {
        return 0;
    }

    if (index + 1U < runtime->stringCount) {
        if (!EspMapRuntime_getStringSourceOffset(index + 1U, &nextSourceOffset) ||
            (uint32_t)nextSourceOffset <
                (uint32_t)sourceOffset + STRING_LENGTH_PREFIX_BYTES ||
            (uint32_t)nextSourceOffset > stringDataEnd) {
            return 0;
        }
        length = (uint32_t)nextSourceOffset -
                 (uint32_t)sourceOffset - STRING_LENGTH_PREFIX_BYTES;
    }
    else {
        length = stringDataEnd - (uint32_t)sourceOffset;
    }

    if (length > 0xffffU) {
        return 0;
    }

    outRef->index = (uint16_t)index;
    outRef->sourceOffset = sourceOffset;
    outRef->length = (uint16_t)length;
    return 1;
}

EspMapStringReadStatus EspMapStrings_read(
    const EspAssetPackEntry* sourceEntry,
    const EspMapStringRef* ref,
    char* destination,
    size_t capacity,
    size_t* outLength) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapStringRef resolved;
    size_t required;

    if (outLength != NULL) {
        *outLength = 0U;
    }
    if (destination != NULL && capacity > 0U) {
        destination[0] = '\0';
    }

    if (sourceEntry == NULL || ref == NULL || destination == NULL ||
        capacity == 0U || runtime == NULL ||
        (sourceEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        sourceEntry->size != runtime->sourceBytes ||
        sourceEntry->crc32 != runtime->sourceCrc32 ||
        !EspMapStrings_getRef(ref->index, &resolved) ||
        resolved.index != ref->index ||
        resolved.sourceOffset != ref->sourceOffset ||
        resolved.length != ref->length) {
        return ESP_MAP_STRING_READ_INVALID;
    }

    required = (size_t)ref->length + 1U;
    if (capacity < required) {
        return ESP_MAP_STRING_READ_BUFFER_TOO_SMALL;
    }

    if ((uint32_t)ref->sourceOffset > sourceEntry->size ||
        (uint32_t)ref->length >
            sourceEntry->size - (uint32_t)ref->sourceOffset) {
        return ESP_MAP_STRING_READ_INVALID;
    }

    if (ref->length > 0U) {
        if (!EspAssetPack_isOpen() ||
            !EspAssetPack_readRange(sourceEntry,
                                    ref->sourceOffset,
                                    destination,
                                    ref->length)) {
            return ESP_MAP_STRING_READ_IO_ERROR;
        }
    }

    destination[ref->length] = '\0';
    if (outLength != NULL) {
        *outLength = ref->length;
    }
    return ESP_MAP_STRING_READ_OK;
}
