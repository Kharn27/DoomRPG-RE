#include <stdint.h>
#include <stdio.h>

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_monster_state.h"

#define WITNESS_MAP_WIDTH 32U
#define WITNESS_TILE_SIZE 64U
#define WITNESS_TILE_CENTER 32U
#define WITNESS_SUBTYPE_MASK 0x7fU

static uint32_t loggedArenaFNV1a;

const EspNativeGameplayMonsterView*
__real_EspNativeGameplayMonsterState_view(void);

static uint16_t read16le(const uint8_t* bytes) {
    if (bytes == NULL) return 0U;
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

const EspNativeGameplayMonsterView*
__wrap_EspNativeGameplayMonsterState_view(void) {
    const EspNativeGameplayMonsterView* monsters =
        __real_EspNativeGameplayMonsterState_view();
    const EspMapSpriteTopologyView* topology;
    uint32_t subtype4 = 0U;
    uint32_t subtype13 = 0U;
    uint32_t witnesses = 0U;
    uint32_t i;

    if (monsters == NULL || monsters->records == NULL ||
        monsters->sourceArenaFNV1a == 0U ||
        loggedArenaFNV1a == monsters->sourceArenaFNV1a ||
        !EspMapSpriteTopology_isReady()) {
        return monsters;
    }

    topology = EspMapSpriteTopology_view();
    if (topology == NULL || topology->entityTypes == NULL ||
        topology->entitySubTypes == NULL || topology->linkStatesLE == NULL ||
        topology->linkOrdersLE == NULL) {
        return monsters;
    }

    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* monster = &monsters->records[i];
        uint32_t spriteIndex;
        uint8_t topologyType;
        uint8_t topologySubtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t tile;
        uint32_t tileX;
        uint32_t tileY;
        uint32_t worldX;
        uint32_t worldY;

        if (monster->alive == 0U ||
            (monster->subtype != 4U && monster->subtype != 13U)) {
            continue;
        }
        if (monster->subtype == 4U) ++subtype4;
        else ++subtype13;

        spriteIndex = monster->spriteIndex;
        if (spriteIndex >= topology->spriteCount) {
            printf("[MONSTER3GOAL] WITNESS-DEFER sprite=%u subtype=%u cause=sprite-range mutation=no allocation=no\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype);
            continue;
        }

        topologyType = topology->entityTypes[spriteIndex];
        topologySubtype =
            topology->entitySubTypes[spriteIndex] & WITNESS_SUBTYPE_MASK;
        linkState = read16le(&topology->linkStatesLE[spriteIndex * 2U]);
        linkOrder = read16le(&topology->linkOrdersLE[spriteIndex * 2U]);

        if (topologyType != ESP_MAP_ENTITY_TYPE_ENEMY ||
            topologySubtype != monster->subtype) {
            printf("[MONSTER3GOAL] WITNESS-DEFER sprite=%u subtype=%u topologyType=%u topologySubtype=%u linkState=%04x linkOrder=%u cause=topology-mismatch mutation=no allocation=no\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype,
                   (unsigned int)topologyType,
                   (unsigned int)topologySubtype,
                   (unsigned int)linkState,
                   (unsigned int)linkOrder);
            continue;
        }

        tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
        tileX = (uint32_t)tile % WITNESS_MAP_WIDTH;
        tileY = (uint32_t)tile / WITNESS_MAP_WIDTH;
        worldX = tileX * WITNESS_TILE_SIZE + WITNESS_TILE_CENTER;
        worldY = tileY * WITNESS_TILE_SIZE + WITNESS_TILE_CENTER;
        ++witnesses;

        printf("[MONSTER3GOAL] WITNESS sprite=%u subtype=%u alt=%u tile=%u pos=%u,%u linked=%u topologyAlive=%u linkOrder=%u source=monster-state+topology-view mutation=no allocation=no\n",
               (unsigned int)spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)monster->alternateAttack,
               (unsigned int)tile,
               (unsigned int)worldX,
               (unsigned int)worldY,
               (unsigned int)((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U),
               (unsigned int)((linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) != 0U),
               (unsigned int)linkOrder);
    }

    printf("[MONSTER3GOAL] CENSUS arena=%08x subtype4=%u subtype13=%u witnesses=%u totalMonsters=%u immutable=yes mutation=no allocation=no\n",
           (unsigned int)monsters->sourceArenaFNV1a,
           (unsigned int)subtype4,
           (unsigned int)subtype13,
           (unsigned int)witnesses,
           (unsigned int)monsters->count);
    loggedArenaFNV1a = monsters->sourceArenaFNV1a;
    return monsters;
}
