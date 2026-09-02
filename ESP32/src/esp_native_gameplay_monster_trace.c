#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_trace.h"
#include "esp_player_view_state.h"

#define TRACE_MASK 0x5687U
#define TRACE_TILES 8U
#define TYPE_ENEMY 1U
#define TYPE_SPECIAL_TRACE 14U
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

static int cardinalTurnReady(const EspNativeGameplayTurnState* turn) {
    if (turn == NULL || turn->active != 1U) return 0;
    return (turn->viewStepX == TILE_SIZE && turn->viewStepY == 0) ||
           (turn->viewStepX == -TILE_SIZE && turn->viewStepY == 0) ||
           (turn->viewStepX == 0 && turn->viewStepY == TILE_SIZE) ||
           (turn->viewStepX == 0 && turn->viewStepY == -TILE_SIZE);
}

static int typeInTraceMask(uint8_t type) {
    return type < 16U && (TRACE_MASK & (1U << type)) != 0U;
}

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
    *outTile = (uint16_t)(tileY * MAP_WIDTH + tileX);
    return 1;
}

static int closedLineBlocksTile(uint16_t tile) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;

    if (runtime == NULL) return -1;
    i = runtime->lineCount;
    while (i > 0U) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t type;
        uint8_t subtype;
        int hasDefinition;

        --i;
        if (!EspMapLineState_getOpen(i, &open)) return -1;
        if (open != 0U) continue;
        if (!EspMapRuntime_getLine(i, &line)) return -1;
        lookup = LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        (void)subtype;
        if (!hasDefinition) {
            if ((line.flags & LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            type = 0U;
        }
        if (!typeInTraceMask(type)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile == tile) return 1;
    }
    return 0;
}

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

static int findBlockingSprite(uint16_t tile,
                              int32_t sourceX,
                              int32_t sourceY,
                              int32_t destX,
                              int32_t destY,
                              uint16_t* outSprite,
                              uint8_t* outType,
                              uint8_t* outSubtype) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t best = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint8_t bestType = 0xffU;
    uint8_t bestSubtype = 0xffU;
    uint32_t i;

    if (outSprite != NULL) *outSprite = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    if (topology == NULL || outSprite == NULL || outType == NULL ||
        outSubtype == NULL) return -1;

    for (i = 0U; i < topology->spriteCount; ++i) {
        const EspNativeGameplayMonsterRecord* monster;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int specialBlocks;

        /* Use the action-engine private leaf so fire/destructible removal overlays
         * are honored without recursing through the combat public wrapper. */
        if (!EspNativeGameplayActionEngine_getEntity(
                i, &type, &subtype, &linkState, &linkOrder)) return -1;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !typeInTraceMask(type)) {
            continue;
        }

        if (type == TYPE_ENEMY) {
            monster = EspNativeGameplayMonsterState_find((uint16_t)i);
            if (monster == NULL || monster->alive == 0U) continue;
        }
        if (type == TYPE_SPECIAL_TRACE) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY,
                                                destX, destY);
            if (specialBlocks < 0) return -1;
            if (specialBlocks == 0) continue;
        }

        if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            linkOrder > bestOrder) {
            best = (uint16_t)i;
            bestOrder = linkOrder;
            bestType = type;
            bestSubtype = subtype;
        }
    }

    if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) return 0;
    *outSprite = best;
    *outType = bestType;
    *outSubtype = bestSubtype;
    return 1;
}

EspNativeGameplayMonsterTraceStatus EspNativeGameplayMonsterTrace_forward(
    EspNativeGameplayMonsterTarget* outTarget) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayTurnState* turn = EspNativeGameplayDispatch_view();
    int32_t sourceX;
    int32_t sourceY;
    uint32_t distance;

    if (outTarget == NULL) return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_INVALID;
    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;

    if (view == NULL || turn == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle || !cardinalTurnReady(turn) ||
        !EspMapRuntime_isLoaded() || !EspMapState_isReady() ||
        !EspMapLineState_isReady() || !EspMapSpriteTopology_isReady() ||
        !EspNativeGameplayMonsterState_isReady()) {
        return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_NOT_READY;
    }

    sourceX = view->destX;
    sourceY = view->destY;
    for (distance = 1U; distance <= TRACE_TILES; ++distance) {
        int32_t destX = view->destX + turn->viewStepX * (int32_t)distance;
        int32_t destY = view->destY + turn->viewStepY * (int32_t)distance;
        uint16_t tile;
        uint16_t spriteIndex;
        uint8_t type;
        uint8_t subtype;
        uint8_t tileFlags;
        int lineBlock;
        int spriteBlock;

        if (!tileIndexFor(destX, destY, &tile)) {
            return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_CLEAR;
        }
        if (!EspMapState_getTileFlags(tile, &tileFlags)) {
            return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_NOT_READY;
        }
        if ((tileFlags & ESP_MAP_TILE_WALL) != 0U) {
            return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_BLOCKED_OTHER;
        }

        lineBlock = closedLineBlocksTile(tile);
        if (lineBlock < 0) return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_NOT_READY;
        if (lineBlock > 0) return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_BLOCKED_OTHER;

        spriteBlock = findBlockingSprite(tile, sourceX, sourceY, destX, destY,
                                         &spriteIndex, &type, &subtype);
        if (spriteBlock < 0) return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_NOT_READY;
        if (spriteBlock > 0) {
            outTarget->spriteIndex = spriteIndex;
            outTarget->tileIndex = tile;
            outTarget->subtype = subtype;
            outTarget->distance = (uint8_t)distance;
            outTarget->type = type;
            outTarget->worldDistance =
                (uint32_t)(distance * TILE_SIZE) *
                (uint32_t)(distance * TILE_SIZE);
            return type == TYPE_ENEMY
                       ? ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND
                       : ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_BLOCKED_OTHER;
        }
        sourceX = destX;
        sourceY = destY;
    }
    return ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_CLEAR;
}
