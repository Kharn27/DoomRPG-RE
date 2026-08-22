#ifndef DOOMRPG_ESP32_MAP_SPRITE_TOPOLOGY_H
#define DOOMRPG_ESP32_MAP_SPRITE_TOPOLOGY_H

#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_SHOW 7U
#define ESP_MAP_OPCODE_HIDE 18U
#define ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE 0x00000200UL

#define ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE 0xffffU
#define ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK 0x03ffU
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT 0x0800U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U
#define ESP_MAP_SPRITE_TOPOLOGY_EXISTS 0x2000U

#define ESP_MAP_SHOW_EFFECT_VISUAL_STATE 0x0001U
#define ESP_MAP_SHOW_EFFECT_REMOVE_BLOCKER 0x0002U
#define ESP_MAP_SHOW_EFFECT_LINK_TARGET 0x0004U
#define ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY 0x0008U

#define ESP_MAP_HIDE_EFFECT_VISUAL_STATE 0x01U
#define ESP_MAP_HIDE_EFFECT_UNLINK 0x02U

#define ESP_MAP_ENTITY_TYPE_ENEMY 1U
#define ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE 12U
#define ESP_MAP_ENTITY_SUBTYPE_CRATE 2U

typedef enum EspMapSpriteTopologyStatus_e {
    ESP_MAP_SPRITE_TOPOLOGY_INVALID = 0,
    ESP_MAP_SPRITE_TOPOLOGY_UNSUPPORTED = 1,
    ESP_MAP_SPRITE_TOPOLOGY_NOT_READY = 2,
    ESP_MAP_SPRITE_TOPOLOGY_OUT_OF_RANGE = 3,
    ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED = 4,
    ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER = 5,
    ESP_MAP_SPRITE_TOPOLOGY_ORDER_EXHAUSTED = 6,
    ESP_MAP_SPRITE_TOPOLOGY_OK = 7
} EspMapSpriteTopologyStatus;

typedef struct EspMapSpriteTopologyView_s {
    const uint8_t* storage;
    const uint8_t* entityTypes;
    const uint8_t* entitySubTypes;
    const uint8_t* visualStates;
    const uint8_t* linkStatesLE;
    const uint8_t* linkOrdersLE;
    uint32_t spriteCount;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t entityDefCount;
    uint32_t entityCount;
    uint32_t linkedCount;
    uint32_t hiddenCount;
    uint32_t enemyCount;
    uint32_t destructibleCount;
    uint16_t nextLinkOrder;
} EspMapSpriteTopologyView;

typedef struct EspMapShowResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint16_t blocker0SpriteIndex;
    uint16_t blocker1SpriteIndex;
    uint16_t effectFlags;
    uint8_t sourceCommandOffset;
    uint8_t showFlags;
    uint8_t visualBefore;
    uint8_t visualAfter;
    uint8_t blockersFound;
    uint8_t blockersRemoved;
    uint8_t blockerNoops;
    uint8_t targetHasEntity;
    uint8_t targetLinkedBefore;
    uint8_t targetLinkedAfter;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapShowResult;

typedef struct EspMapHideResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t tileIndex;
    uint16_t firstHiddenSpriteIndex;
    uint16_t lastHiddenSpriteIndex;
    uint16_t hiddenEntityCount;
    uint8_t sourceCommandOffset;
    uint8_t tileX;
    uint8_t tileY;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
    uint8_t effectFlags;
} EspMapHideResult;

/*
 * Build a compact native map-sprite/entity-topology owner from the resident
 * immutable map plus /entities.db. The caller owns PAK open/close. The owner
 * deliberately contains no Entity_t pointers, monster/AI state, Render objects
 * or 1024-entry pointer database.
 */
void EspMapSpriteTopology_reset(void);
int EspMapSpriteTopology_buildFromRuntime(const EspAssetPackEntry* entityDefsEntry);
int EspMapSpriteTopology_resetMutableFromRuntime(void);
int EspMapSpriteTopology_isReady(void);
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void);

int EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                        uint8_t* outVisualState);
int EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                   uint8_t* outType,
                                   uint8_t* outSubType,
                                   uint16_t* outLinkState,
                                   uint16_t* outLinkOrder);

/* Execute only the final MAP_INTRO sprite/entity-topology families. */
EspMapSpriteTopologyStatus EspMapSpriteTopology_applyShow(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapShowResult* outResult);

EspMapSpriteTopologyStatus EspMapSpriteTopology_applyHide(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapHideResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
