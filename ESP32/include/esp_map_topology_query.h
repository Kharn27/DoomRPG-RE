#ifndef DOOMRPG_ESP32_MAP_TOPOLOGY_QUERY_H
#define DOOMRPG_ESP32_MAP_TOPOLOGY_QUERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_TOPOLOGY_QUERY_NO_SPRITE 0xffffU

/*
 * Allocation-free decoded view of one currently-linked map-sprite entity.
 * Immutable x/y/info come from EspMapRuntime; type/subtype/tile/order come from
 * the compact mutable EspMapSpriteTopology owner. No legacy Entity_t/Sprite_t
 * pointers escape this boundary.
 */
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
 * Return the linked entity on `tileIndex` with the greatest link order strictly
 * below `beforeOrder`. Pass 0xffff for the tile head, then feed the returned
 * linkOrder back to walk nextOnTile in legacy-equivalent head-to-tail order.
 * Returns 1 when an entity is produced and 0 for no match/invalid context.
 */
int EspMapTopologyQuery_findLinkedOnTile(
    uint16_t tileIndex,
    uint16_t beforeOrder,
    EspMapTopologyEntityRef* outEntity);

#ifdef __cplusplus
}
#endif

#endif
