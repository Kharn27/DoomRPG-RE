#include <stdint.h>
#include <string.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_topology_query.h"

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int EspMapTopologyQuery_findLinkedOnTile(
    uint16_t tileIndex,
    uint16_t beforeOrder,
    EspMapTopologyEntityRef* outEntity) {
    const EspMapSpriteTopologyView* topology;
    const EspMapRuntimeView* runtime;
    EspMapSprite sprite;
    uint32_t i;
    uint16_t state;
    uint16_t order;
    uint16_t bestOrder = 0U;
    uint16_t bestIndex = ESP_MAP_TOPOLOGY_QUERY_NO_SPRITE;

    if (outEntity != NULL) memset(outEntity, 0, sizeof(*outEntity));
    if (outEntity == NULL || tileIndex >= 1024U || beforeOrder == 0U ||
        !EspMapSpriteTopology_isReady() || !EspMapRuntime_isLoaded()) return -1;

    topology = EspMapSpriteTopology_view();
    runtime = EspMapRuntime_view();
    if (topology == NULL || runtime == NULL || topology->storage == NULL ||
        topology->linkStatesLE == NULL || topology->linkOrdersLE == NULL ||
        topology->entityTypes == NULL || topology->entitySubTypes == NULL ||
        topology->spriteCount != runtime->mapSpriteCount) return -1;

    for (i = 0U; i < topology->spriteCount; ++i) {
        state = readLe16(topology->linkStatesLE + (i * 2U));
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tileIndex) continue;

        order = readLe16(topology->linkOrdersLE + (i * 2U));
        if (order == 0U || order >= beforeOrder) continue;
        if (bestIndex == ESP_MAP_TOPOLOGY_QUERY_NO_SPRITE || order > bestOrder) {
            bestIndex = (uint16_t)i;
            bestOrder = order;
        }
    }

    if (bestIndex == ESP_MAP_TOPOLOGY_QUERY_NO_SPRITE) return 0;
    if (!EspMapRuntime_getMapSprite(bestIndex, &sprite)) return -1;

    outEntity->info = sprite.info;
    outEntity->spriteIndex = bestIndex;
    outEntity->tileIndex = tileIndex;
    outEntity->linkOrder = bestOrder;
    outEntity->x = sprite.x;
    outEntity->y = sprite.y;
    outEntity->type = topology->entityTypes[bestIndex];
    outEntity->subType = topology->entitySubTypes[bestIndex] & 0x7fU;
    return 1;
}
