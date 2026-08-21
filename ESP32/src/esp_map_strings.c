#include <stddef.h>
#include <stdint.h>

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
