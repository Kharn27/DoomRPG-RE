#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_collision.h"

#define TILE_SIZE 64
#define TILE_CENTER 32
#define MAP_WIDTH 32
#define MAP_MAX_CENTER (((MAP_WIDTH - 1) * TILE_SIZE) + TILE_CENTER)
#define SPECIAL_TRACE_ENTITY_FLAG 0x00020000UL
#define SPECIAL_TRACE_Y_MASK 0x00180000UL
#define SPECIAL_TRACE_X_MASK 0x00600000UL
#define LINE_ENTITY_DEF_BASE 305U
#define LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL
#define LINE_GEOMETRY_AXIS_X 0x00000008UL
#define LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define LINE_ENTITY_NUDGE_X_NEG 0x00004000UL

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

static int entityTypeInTraceMask(uint8_t type) {
    return type < 16U &&
           (ESP_NATIVE_GAMEPLAY_COLLISION_LEGACY_TRACE_MASK & (1U << type)) != 0U;
}

/* Reproduce the coordinates seen by legacy Game_loadMapEntities(). Render map
 * loading first applies the +/-3 line geometry nudge; line-entity placement
 * then takes that midpoint and applies its mutually-exclusive +/-1 cell-side
 * nudge before linking the entity. */
static int lineEntityTile(const EspMapLine* line, uint16_t* outTile) {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t x;
    int32_t y;
    uint32_t tileX;
    uint32_t tileY;

    if (line == NULL || outTile == NULL) return 0;
    x1 = (int32_t)line->x1;
    y1 = (int32_t)line->y1;
    x2 = (int32_t)line->x2;
    y2 = (int32_t)line->y2;

    if ((line->flags & LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    if (x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MAP_WIDTH || tileY >= MAP_WIDTH) return 0;
    *outTile = (uint16_t)((tileY * MAP_WIDTH) + tileX);
    return 1;
}

/* Game_loadMapEntities() appends line entities after all map-sprite entities,
 * and Game_linkEntity() inserts at the tile-list head. Reverse line order is
 * therefore the native equivalent for the first linked line blocker on one
 * tile. Legacy door-open unlinks only that line entity; consume the compact
 * per-line open bit directly and continue scanning older closed lines. */
static int findLinkedLineBlocker(uint16_t tile,
                                 uint16_t* outLineIndex,
                                 uint16_t* outTexture,
                                 uint32_t* outFlags,
                                 uint8_t* outType) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;

    if (runtime == NULL || outLineIndex == NULL || outTexture == NULL ||
        outFlags == NULL || outType == NULL ||
        !EspEntityDefTypeCatalog_isReady()) {
        return -1;
    }

    i = runtime->lineCount;
    while (i > 0U) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t type;
        int hasDefinition;

        --i;
        if (!EspMapLineState_getOpen(i, &open)) return -1;
        if (open != 0U) continue;
        if (!EspMapRuntime_getLine(i, &line)) return -1;
        lookup = LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition =
            lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
            EspEntityDefTypeCatalog_getType((uint16_t)lookup, &type);
        if (!hasDefinition) {
            if ((line.flags & LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            /* Legacy fallback is game->entities[0].def, the static wall type. */
            type = 0U;
        }
        if (!entityTypeInTraceMask(type)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile != tile) continue;

        *outLineIndex = (uint16_t)i;
        *outTexture = line.texture;
        *outFlags = line.flags;
        *outType = type;
        return 1;
    }
    return 0;
}

/* Game_trace() treats map-sprite entity types 14/15 as thin crossing planes.
 * They are appended only when the movement segment crosses the axis selected by
 * sprite info; otherwise the entity is skipped. Reproduce that exact predicate
 * from immutable EspMapSprite data rather than treating 14/15 as solid tiles. */
static int specialEntityBlocks(uint32_t spriteIndex,
                               int32_t sourceX,
                               int32_t sourceY,
                               int32_t destX,
                               int32_t destY) {
    EspMapSprite sprite;
    int32_t sprX;
    int32_t sprY;

    if (!EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return -1;
    if ((sprite.info & SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;

    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
    if ((sprite.info & SPECIAL_TRACE_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite.info & SPECIAL_TRACE_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
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
    uint16_t blockerLine;
    uint16_t blockerLineTexture;
    uint32_t blockerLineFlags;
    uint8_t blockerLineType;
    int lineBlocker;

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
        !EspMapSpriteTopology_isReady() || !EspMapRuntime_isLoaded() ||
        !EspEntityDefTypeCatalog_isReady()) {
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

    /* Legacy Game_trace walks both cells for a one-tile move. The classic
     * loader installs its sentinel wall entity on blocked map cells. Native
     * gameplay has no pointer database, so the compact WALL bit is the direct
     * equivalent. */
    if ((outResult->sourceFlags & ESP_MAP_TILE_WALL) != 0U ||
        (outResult->destFlags & ESP_MAP_TILE_WALL) != 0U) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_WALL);
    }

    /* Special/map-door lines are real legacy Entity_t records while linked.
     * Tile flags alone cannot represent a door entity whose midpoint is linked
     * into the destination cell. The compact line overlay is the permanent
     * relink owner: closed lines participate, open lines are skipped. Trace
     * source then destination, matching Game_trace's tile walk. */
    lineBlocker = findLinkedLineBlocker(sourceTile, &blockerLine,
                                        &blockerLineTexture,
                                        &blockerLineFlags, &blockerLineType);
    if (lineBlocker < 0) {
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
    }
    if (lineBlocker == 0) {
        lineBlocker = findLinkedLineBlocker(destTile, &blockerLine,
                                            &blockerLineTexture,
                                            &blockerLineFlags, &blockerLineType);
        if (lineBlocker < 0) {
            return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
        }
    }
    if (lineBlocker > 0) {
        outResult->blockerType = blockerLineType;
        outResult->blockerSubType = 0xffU;
        outResult->linkedBlockers = 1U;
        printf("[LINECOLLISION] BLOCK source=%u dest=%u line=%u texture=%u flags=%08x type=%u defTile=%u\n",
               (unsigned int)sourceTile, (unsigned int)destTile,
               (unsigned int)blockerLine,
               (unsigned int)blockerLineTexture,
               (unsigned int)blockerLineFlags,
               (unsigned int)blockerLineType,
               (unsigned int)(LINE_ENTITY_DEF_BASE + blockerLineTexture));
        return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY);
    }

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subType;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t tile;
        int specialBlocks;

        if (!EspMapSpriteTopology_getEntity(
                i, &type, &subType, &linkState, &linkOrder)) {
            return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            !entityTypeInTraceMask(type)) {
            continue;
        }
        tile = linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK;
        if (tile != sourceTile && tile != destTile) continue;

        if (type == 14U || type == 15U) {
            specialBlocks = specialEntityBlocks(
                i, sourceX, sourceY, destX, destY);
            if (specialBlocks < 0) {
                return finish(outResult, ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY);
            }
            if (specialBlocks == 0) continue;
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