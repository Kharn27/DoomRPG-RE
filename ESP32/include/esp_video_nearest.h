#ifndef DOOMRPG_ESP32_VIDEO_NEAREST_H
#define DOOMRPG_ESP32_VIDEO_NEAREST_H

#include <stdint.h>

/* Bounded 16-bit destination -> source coordinate, nearest neighbour.
 * Equal to floor(dstCoord * srcExtent / physicalExtent). No allocation.
 * UINT16_MAX denotes an invalid coordinate/extent and must not be indexed. */
static inline uint16_t EspVideoNearest_source(uint16_t dstCoord,
                                              uint16_t srcExtent,
                                              uint16_t physicalExtent) {
    if (srcExtent == 0U || physicalExtent == 0U ||
        dstCoord >= physicalExtent) {
        return UINT16_MAX;
    }
    return (uint16_t)(((uint32_t)dstCoord * (uint32_t)srcExtent) /
                      (uint32_t)physicalExtent);
}

#endif
