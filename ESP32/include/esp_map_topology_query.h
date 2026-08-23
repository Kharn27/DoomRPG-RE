#ifndef DOOMRPG_ESP32_MAP_TOPOLOGY_QUERY_H
#define DOOMRPG_ESP32_MAP_TOPOLOGY_QUERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_TOPOLOGY_QUERY_NO_SPRITE 0xffffU

typedef struct EspMapTopologyEntityRef_s {
    uint32_t info;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint16_t linkOrder;
    uint16_t x;
    uint16_t y;
    uint8_t type;
    uint8_t subType;
} EspMapTopologyEntityRef;

/*
 * Walk one compact nextOnTile chain in legacy head-to-tail order.
 * Return values are deliberately tri-state:
 *   1  linked entity returned
 *   0  valid context, no lower-order entity remains
 *  -1  invalid/inconsistent native context
 */
int EspMapTopologyQuery_findLinkedOnTile(
    uint16_t tileIndex,
    uint16_t beforeOrder,
    EspMapTopologyEntityRef* outEntity);

#ifdef __cplusplus
}
#endif

#endif
