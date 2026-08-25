#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_line_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_collision.h"

#define TILE_SIZE 64
#define TILE_CENTER 32
#define MAP_WIDTH 32
#define MAP_MAX_CENTER (((MAP_WIDTH - 1) * TILE_SIZE) + TILE_CENTER)

typedef char EspNativeGameplayCollisionResult_must_be_16_bytes[
    sizeof(EspNativeGameplayCollisionResult) == 16U ? 1 : -1];

static int centeredCoordinate(int32_t value) {
    return value >= TILE_CENTER && value <= MAP_MAX_CENTER &&
           (value & (TILE_SIZE - 1)) == TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MAP_WIDTH || tileY >= MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * MAP_WIDTH + tileX);
    return 1;
}

static int oneCardinalStep(int32_t sourceX,
                           int32_t sourceY,
                           int32_t destX,
                           int32_t destY) {
    const int32_t dx = destX - sourceX;
    const int32_t dy = destY - sourceY;
    return (dx == TILE_SIZE && dy == 0) ||
           (dx == -TILE_SIZE && dy == 0) ||
           (dx == 0 && dy == TILE_SIZE) ||
           (dx == 0 && dy == -TILE_SIZE);
}

static int entityTypeBlocks(uint8_t type) {
    return type < 16U &&
           (ESP_NATIVE_GAMEPLAY_COLLISION_LEGACY_TRACE_MASK & (1U << type)) != 0U;
}

static EspNativeGameplayCollisionStatus finish(
    EspNativeGameplayCollisionResult* result,
    EspNativeGameplayCollisionStatus status) {
    if (result != NULL) result->status = (uint8_t)status;
    return status;
}

EspNativeGameplayCollisionStatus EspNativeGameplayCollision_traceCardinalStep(
    int32_t sourceX,
    int32_t sourceY,
    int32_t destX,
    int32_t destY,
    EspNativeGameplayCollisionResult* outResult) {
    const EspMapStateView* mapState;
    const EspMapLineStateView* lineState;
    const EspMapSpriteTopologyView* topology;
    uint16_t sourceTile;
    uint16_t destTile;
    uint32_t i;
    uint16_t bestOrder = 0U;
    uint16_t blocker = ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE;
    uint8_t blockerType = 0xffU;
    uint8_t blockerSubType = 0xffU;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_COLLISION_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    outResult->blockerSpriteIndex = ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE;
    outResult->blockerType = 0xffU;
    outResult->blockerSubType = 0xffU;

    if (!oneCardinalStep(sourceX, sourceY, destX, destY) ||
        !tileIndexFor(sourceX, sourceY, &sourceTile) ||
        !tileIndexFor(destX, destY, &destTile)) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_INVALID);
    }

    if (!EspMapState_isReady() || !EspMapLineState_isReady() ||
        !EspMapSpriteTopology_isReady()) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
    }
    mapState = EspMapState_view();
    lineState = EspMapLineState_view();
    topology = EspMapSpriteTopology_view();
    if (mapState == NULL || lineState == NULL || topology == NULL ||
        mapState->tileCount != ESP_MAP_STATE_TILE_COUNT) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
    }

    outResult->sourceTile = sourceTile;
    outResult->destTile = destTile;
    if (!EspMapState_getTileFlags(sourceTile, &outResult->sourceFlags) ||
        !EspMapState_getTileFlags(destTile, &outResult->destFlags)) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
    }

    outResult->openLineCount =
        (uint8_t)(lineState->openCount > 255U ? 255U : lineState->openCount);
    if (lineState->openCount != 0U) {
        return finish(outResult,
                      ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES);
    }

    if ((outResult->destFlags & ESP_MAP_TILE_WALL) != 0U) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_WALL);
    }

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subType;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!EspMapSpriteTopology_getEntity(
                i, &type, &subType, &linkState, &linkOrder)) {
            return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != destTile ||
            !entityTypeBlocks(type)) {
            continue;
        }
        if (outResult->linkedBlockers != 0xffU) ++outResult->linkedBlockers;
        if (blocker == ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE ||
            linkOrder > bestOrder) {
            blocker = (uint16_t)i;
            bestOrder = linkOrder;
            blockerType = type;
            blockerSubType = subType;
        }
    }

    if (blocker != ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE) {
        outResult->blockerSpriteIndex = blocker;
        outResult->blockerType = blockerType;
        outResult->blockerSubType = blockerSubType;
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY);
    }

    return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_CLEAR);
}

const char* EspNativeGameplayCollision_statusName(
    EspNativeGameplayCollisionStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_COLLISION_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES:
        return "DYNAMIC_LINES_UNSUPPORTED";
    case ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_WALL: return "WALL";
    case ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY: return "ENTITY";
    case ESP_NATIVE_GAMEPLAY_COLLISION_CLEAR: return "CLEAR";
    default: return "UNKNOWN";
    }
}
