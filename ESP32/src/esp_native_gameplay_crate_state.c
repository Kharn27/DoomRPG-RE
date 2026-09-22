#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_player_view_state.h"

#define CRATE_ENTITY_TYPE 12U
#define CRATE_ENTITY_SUBTYPE 2U
#define CRATE_RESOURCE_WORLD 3U
#define CRATE_RESOURCE_INVENTORY 4U
#define CRATE_RESOURCE_AMMO 6U

#define CRATE_DEF_MASK 511U
#define CRATE_DEF_TILE_FLAG 0x00040000UL
#define CRATE_DEF_TILE_BASE 305U
#define CRATE_SPRITE_INFO_DEF_CLEAR 0xfffffe00UL

typedef struct CrateTransformRecord_s {
    uint16_t spriteIndex;
    uint16_t effectiveDefTile;
} CrateTransformRecord;

typedef struct CrateStateOwner_s {
    EspNativeGameplayCrateStateView view;
    CrateTransformRecord records[ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS];
} CrateStateOwner;

static CrateStateOwner crateState;

int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);
int __real_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder);

static int rawDefinition(uint32_t spriteIndex,
                         uint16_t* outDefTile,
                         uint8_t* outType,
                         uint8_t* outSubtype,
                         int32_t* outParm) {
    EspMapSprite sprite;
    uint32_t lookup;
    if (!__real_EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;
    lookup = sprite.info & CRATE_DEF_MASK;
    if ((sprite.info & CRATE_DEF_TILE_FLAG) != 0U) {
        lookup += CRATE_DEF_TILE_BASE;
    }
    if (lookup >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) return 0;
    if (!EspEntityDefTypeCatalog_getMetadata((uint16_t)lookup,
                                             outType, outSubtype, outParm)) {
        return 0;
    }
    if (outDefTile != NULL) *outDefTile = (uint16_t)lookup;
    return 1;
}

static int allowedDrop(uint8_t type, uint8_t subtype) {
    if (type == CRATE_RESOURCE_WORLD) {
        return subtype == 21U || subtype == 22U || subtype == 23U;
    }
    if (type == CRATE_RESOURCE_INVENTORY) return subtype == 25U;
    if (type == CRATE_RESOURCE_AMMO) return subtype <= 4U;
    return 0;
}

static int targetTile(uint8_t type, uint8_t subtype, uint16_t* outTile) {
    uint16_t tile = 0U;
    uint8_t metaType = 0U;
    uint8_t metaSubtype = 0U;
    int32_t parm = 0;
    if (outTile == NULL ||
        !EspEntityDefTypeCatalog_findTileIndex(type, subtype, &tile) ||
        tile > CRATE_DEF_MASK ||
        !EspEntityDefTypeCatalog_getMetadata(tile, &metaType, &metaSubtype, &parm) ||
        metaType != type || metaSubtype != subtype || !allowedDrop(type, subtype)) {
        return 0;
    }
    *outTile = tile;
    return 1;
}

static int targetsReady(uint16_t outTiles[9]) {
    uint8_t i;
    if (outTiles == NULL) return 0;
    if (!targetTile(3U, 23U, &outTiles[0]) ||
        !targetTile(3U, 22U, &outTiles[1]) ||
        !targetTile(4U, 25U, &outTiles[2]) ||
        !targetTile(3U, 21U, &outTiles[3])) {
        return 0;
    }
    for (i = 0U; i < 5U; ++i) {
        if (!targetTile(6U, i, &outTiles[4U + i])) return 0;
    }
    return 1;
}

static int findRecord(uint32_t spriteIndex) {
    uint16_t i;
    for (i = 0U; i < crateState.view.transformedCount; ++i) {
        if (crateState.records[i].spriteIndex == spriteIndex) return (int)i;
    }
    return -1;
}

void EspNativeGameplayCrateState_reset(void) {
    memset(&crateState, 0, sizeof(crateState));
}

int EspNativeGameplayCrateState_ensure(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* playerView = EspPlayerView_view();
    uint16_t targetTiles[9];
    uint32_t i;
    uint16_t crates = 0U;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->mapSpriteCount == 0U ||
        runtime->mapSpriteCount > ESP_MAP_SPRITE_TOPOLOGY_MAX_SPRITES ||
        playerView == NULL || playerView->active != 1U ||
        playerView->targetMapId == 0U ||
        !EspMapSpriteTopology_isReady() ||
        !EspEntityDefTypeCatalog_isReady()) {
        return 0;
    }
    if (crateState.view.active != 0U &&
        crateState.view.sourceArenaFNV1a == runtime->arenaFNV1a &&
        crateState.view.spriteCount == runtime->mapSpriteCount &&
        crateState.view.targetMapId == playerView->targetMapId) {
        return crateState.view.fatal == 0U;
    }

    memset(&crateState, 0, sizeof(crateState));
    crateState.view.sourceArenaFNV1a = runtime->arenaFNV1a;
    crateState.view.spriteCount = (uint16_t)runtime->mapSpriteCount;
    crateState.view.targetMapId = playerView->targetMapId;
    crateState.view.active = 1U;

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!__real_EspMapSpriteTopology_getEntity(
                i, &type, &subtype, &linkState, &linkOrder)) {
            crateState.view.fatal = 1U;
            break;
        }
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U &&
            type == CRATE_ENTITY_TYPE && subtype == CRATE_ENTITY_SUBTYPE) {
            ++crates;
        }
    }
    crateState.view.crateCount = crates;
    if (crates > ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS ||
        !targetsReady(targetTiles)) {
        crateState.view.fatal = 1U;
    }

    printf("[CRATESTATE] READY arena=%08x sprites=%u crates=%u capacity=%u ownerBytes=%u targets=%s tiles=%u/%u/%u/%u ammo=%u/%u/%u/%u/%u persistence=deferred immutableBsp=yes allocation=no fatal=%u\n",
           (unsigned int)crateState.view.sourceArenaFNV1a,
           (unsigned int)crateState.view.spriteCount,
           (unsigned int)crateState.view.crateCount,
           (unsigned int)ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS,
           (unsigned int)sizeof(crateState),
           crateState.view.fatal == 0U ? "ready" : "NOT_READY",
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[0] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[1] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[2] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[3] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[4] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[5] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[6] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[7] : 0U,
           crateState.view.fatal == 0U ? (unsigned int)targetTiles[8] : 0U,
           (unsigned int)crateState.view.fatal);
    return crateState.view.fatal == 0U;
}

const EspNativeGameplayCrateStateView* EspNativeGameplayCrateState_view(void) {
    return EspNativeGameplayCrateState_ensure() ? &crateState.view : NULL;
}

int EspNativeGameplayCrateState_isTransformed(uint32_t spriteIndex) {
    if (!EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState.view.spriteCount) return 0;
    return findRecord(spriteIndex) >= 0;
}

int EspNativeGameplayCrateState_effectiveDefTile(uint32_t spriteIndex,
                                                 uint16_t* outDefTile) {
    int found;
    if (outDefTile == NULL || !EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState.view.spriteCount) return 0;
    found = findRecord(spriteIndex);
    if (found < 0) return 0;
    *outDefTile = crateState.records[found].effectiveDefTile;
    return 1;
}

int EspNativeGameplayCrateState_transform(uint16_t spriteIndex,
                                          uint16_t effectiveDefTile) {
    uint8_t sourceType;
    uint8_t sourceSubtype;
    uint8_t targetType;
    uint8_t targetSubtype;
    int32_t sourceParm;
    int32_t targetParm;
    uint16_t sourceTile;
    int found;

    if (!EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState.view.spriteCount ||
        effectiveDefTile > CRATE_DEF_MASK ||
        !rawDefinition(spriteIndex, &sourceTile, &sourceType, &sourceSubtype,
                       &sourceParm) ||
        sourceType != CRATE_ENTITY_TYPE || sourceSubtype != CRATE_ENTITY_SUBTYPE ||
        !EspEntityDefTypeCatalog_getMetadata(effectiveDefTile,
                                             &targetType, &targetSubtype,
                                             &targetParm) ||
        !allowedDrop(targetType, targetSubtype)) {
        return 0;
    }
    (void)sourceTile;
    (void)sourceParm;
    (void)targetParm;
    found = findRecord(spriteIndex);
    if (found >= 0) {
        return crateState.records[found].effectiveDefTile == effectiveDefTile;
    }
    if (crateState.view.transformedCount >= ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS) {
        return 0;
    }
    crateState.records[crateState.view.transformedCount].spriteIndex = spriteIndex;
    crateState.records[crateState.view.transformedCount].effectiveDefTile =
        effectiveDefTile;
    ++crateState.view.transformedCount;
    return 1;
}

int EspNativeGameplayCrateState_rollbackTransform(uint16_t spriteIndex,
                                                  uint16_t effectiveDefTile) {
    int found;
    uint16_t last;
    if (!EspNativeGameplayCrateState_ensure()) return 0;
    found = findRecord(spriteIndex);
    if (found < 0 ||
        crateState.records[found].effectiveDefTile != effectiveDefTile ||
        crateState.view.transformedCount == 0U) {
        return 0;
    }
    last = (uint16_t)(crateState.view.transformedCount - 1U);
    if ((uint16_t)found != last) crateState.records[found] = crateState.records[last];
    memset(&crateState.records[last], 0, sizeof(crateState.records[last]));
    --crateState.view.transformedCount;
    return 1;
}

int EspNativeGameplayCrateState_applyMapSprite(uint32_t spriteIndex,
                                               EspMapSprite* ioSprite) {
    uint16_t defTile;
    if (ioSprite == NULL) return 0;
    if (!EspNativeGameplayCrateState_effectiveDefTile(spriteIndex, &defTile)) {
        return 1;
    }
    ioSprite->info = (ioSprite->info & CRATE_SPRITE_INFO_DEF_CLEAR) |
                     (uint32_t)defTile;
    return 1;
}

int EspNativeGameplayCrateState_applyEntity(uint32_t spriteIndex,
                                            uint8_t* ioType,
                                            uint8_t* ioSubtype,
                                            uint16_t* ioLinkState) {
    uint16_t defTile;
    uint8_t type;
    uint8_t subtype;
    int32_t parm;
    if (!EspNativeGameplayCrateState_effectiveDefTile(spriteIndex, &defTile)) {
        return 1;
    }
    if (!EspEntityDefTypeCatalog_getMetadata(defTile, &type, &subtype, &parm) ||
        !allowedDrop(type, subtype)) {
        return 0;
    }
    (void)parm;
    if (ioType != NULL) *ioType = type;
    if (ioSubtype != NULL) *ioSubtype = subtype;
    if (ioLinkState != NULL) {
        *ioLinkState &= (uint16_t)~ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    }
    return 1;
}

int EspNativeGameplayCrateState_requiresSecondRng(uint8_t first) {
    return first >= 150U && first < 213U;
}

int EspNativeGameplayCrateState_resolveOutcome(
    uint8_t first,
    uint8_t second,
    uint8_t secondValid,
    EspNativeGameplayCrateOutcome* outOutcome,
    uint16_t* outEffectiveDefTile) {
    uint8_t type = 0U;
    uint8_t subtype = 0U;
    uint16_t tile = 0U;

    if (outOutcome == NULL || outEffectiveDefTile == NULL ||
        !EspNativeGameplayCrateState_ensure()) return 0;
    *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID;
    *outEffectiveDefTile = 0U;

    if (first < 2U) {
        if (secondValid != 0U) return 0;
        *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE;
        return 1;
    }
    if (first < 4U) {
        type = 3U; subtype = 23U;
    }
    else if (first < 12U) {
        type = 3U; subtype = 22U;
    }
    else if (first < 24U) {
        type = 4U; subtype = 25U;
    }
    else if (first < 150U) {
        type = 3U; subtype = 21U;
    }
    else if (first < 213U) {
        if (secondValid == 0U) return 0;
        type = 6U;
        subtype = (uint8_t)(second % 5U);
    }
    else {
        if (secondValid != 0U) return 0;
        *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE;
        return 1;
    }

    if (secondValid != 0U && !(first >= 150U && first < 213U)) return 0;
    if (!targetTile(type, subtype, &tile)) return 0;
    *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM;
    *outEffectiveDefTile = tile;
    return 1;
}
