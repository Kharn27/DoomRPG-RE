#ifndef DOOMRPG_ESP32_MAP_STRINGS_H
#define DOOMRPG_ESP32_MAP_STRINGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspMapStringRef_s {
    uint16_t index;
    uint16_t sourceOffset;
    uint16_t length;
} EspMapStringRef;

/*
 * Resolve one BSP map string to its payload span inside the current source
 * entry. The string bytes remain on the native asset pack; this API allocates
 * nothing and never materializes map-wide text.
 */
int EspMapStrings_getRef(uint32_t index, EspMapStringRef* outRef);

#ifdef __cplusplus
}
#endif

#endif
