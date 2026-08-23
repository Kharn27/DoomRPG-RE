#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_map_facing_index.h"
#include "esp_map_line_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_player_view_state.h"
#include "esp_post_spawn_refresh.h"

#define MAP_WIDTH 32
#define MAP_TILE_COUNT 1024U
#define TRACE_ENTITY_LIMIT 8U

#define LINE_FLAG_EAST_SOUTH 0x00000008UL
#define LINE_FLAG_WEST_NORTH 0x00000010UL
#define LINE_FLAG_VERTICAL 0x00000100UL
#define LINE_FLAG_HORIZONTAL 0x00000200UL
#define LINE_LINK_OFFSET_NORTH 0x00000800UL
#define LINE_LINK_OFFSET_EAST 0x00002000UL
#define LINE_LINK_OFFSET_SOUTH 0x00001000UL
#define LINE_LINK_OFFSET_WEST 0x00004000UL

static EspPostSpawnRefreshState refreshState;

static int clampTileCoord(int value) {
    if (value < 0) return 0;
    if (value >= MAP_WIDTH) return MAP_WIDTH - 1;
    return value;
}

static int typeIsTraced(uint8_t type) {
    return type < 32U &&
           (ESP_POST_SPAWN_FACING_TRACE_FLAGS & (1UL << type)) != 0U;
}

static int lineEntityTile(uint32_t lineIndex, uint16_t* outTileIndex) {
    EspMapLine line;
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t x;
    int32_t y;

    if (outTileIndex == NULL || !EspMapRuntime_getLine(lineIndex, &line)) {
        return 0;
    }

    x1 = (int32_t)line.x1;
    y1 = (int32_t)line.y1;
    x2 = (int32_t)line.x2;
    y2 = (int32_t)line.y2;

    /* Render_beginLoadMapData() permanently nudges this legacy line geometry
     * before Game_loadMapEntities() computes the special-line link tile. */
    if ((line.flags & LINE_FLAG_HORIZONTAL) != 0U) {
        if ((line.flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line.flags & LINE_FLAG_WEST_NORTH) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line.flags & LINE_FLAG_VERTICAL) != 0U) {
        if ((line.flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line.flags & LINE_FLAG_WEST_NORTH) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line.flags & LINE_LINK_OFFSET_NORTH) != 0U) {
        --y;
    }
    else if ((line.flags & LINE_LINK_OFFSET_EAST) != 0U) {
        ++x;
    }
    else if ((line.flags & LINE_LINK_OFFSET_SOUTH) != 0U) {
        ++y;
    }
    else if ((line.flags & LINE_LINK_OFFSET_WEST) != 0U) {
        --x;
    }

    x >>= 6;
    y >>= 6;
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_WIDTH) return 0;
    *outTileIndex = (uint16_t)((uint32_t)y * MAP_WIDTH + (uint32_t)x);
    return 1;
}

static int rayForView(const EspPlayerViewState* view,
                      int32_t* outStartX,
                      int32_t* outStartY,
                      int32_t* outEndX,
                      int32_t* outEndY,
                      int* outStepX,
                      int* outStepY) {
    int32_t startX;
    int32_t startY;
    int stepX;
    int stepY;

    if (view == NULL || outStartX == NULL || outStartY == NULL ||
        outEndX == NULL || outEndY == NULL || outStepX == NULL ||
        outStepY == NULL || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle) {
        return 0;
    }

    startX = view->destX;
    startY = view->destY;
    stepX = 0;
    stepY = 0;

    switch ((uint32_t)view->destAngle & 255U) {
        case 0U:
            startX += 31;
            stepX = 64;
            break;
        case 64U:
            startY -= 31;
            stepY = -64;
            break;
        case 128U:
            startX -= 31;
            stepX = -64;
            break;
        case 192U:
            startY += 31;
            stepY = 64;
            break;
        default:
            return 0;
    }

    *outStartX = startX;
    *outStartY = startY;
    *outEndX = startX + 3 * stepX;
    *outEndY = startY + 3 * stepY;
    *outStepX = stepX;
    *outStepY = stepY;
    return 1;
}

static int spriteCrossesSpecialPlane(const EspMapSprite* sprite,
                                     int32_t srcX,
                                     int32_t srcY,
                                     int32_t destX,
                                     int32_t destY) {
    uint32_t info;

    if (sprite == NULL) return 0;
    info = sprite->info;
    if ((info & 0x00020000UL) == 0U) return 1;

    if ((info & 0x00180000UL) != 0U) {
        return (srcY <= (int32_t)sprite->y && destY > (int32_t)sprite->y) ||
               (srcY >= (int32_t)sprite->y && destY < (int32_t)sprite->y);
    }
    if ((info & 0x00600000UL) != 0U) {
        return (srcX <= (int32_t)sprite->x && destX > (int32_t)sprite->x) ||
               (srcX >= (int32_t)sprite->x && destX < (int32_t)sprite->x);
    }
    return 0;
}

static int findNextLinkedSprite(uint16_t tileIndex,
                                uint16_t belowOrder,
                                uint16_t* outSpriteIndex,
                                uint8_t* outType,
                                uint8_t* outSubType,
                                uint16_t* outOrder) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;
    uint16_t state;
    uint16_t order;
    uint16_t bestOrder = 0U;
    uint16_t bestIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint8_t type;
    uint8_t subType;
    uint8_t bestType = ESP_POST_SPAWN_NO_TYPE;
    uint8_t bestSubType = ESP_POST_SPAWN_NO_TYPE;

    if (topology == NULL || outSpriteIndex == NULL || outType == NULL ||
        outSubType == NULL || outOrder == NULL) return 0;

    for (i = 0U; i < topology->spriteCount; ++i) {
        if (!EspMapSpriteTopology_getEntity(i, &type, &subType, &state, &order)) {
            return 0;
        }
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tileIndex ||
            order == 0U || order >= belowOrder) {
            continue;
        }
        if (bestIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            order > bestOrder) {
            bestIndex = (uint16_t)i;
            bestOrder = order;
            bestType = type;
            bestSubType = subType;
        }
    }

    *outSpriteIndex = bestIndex;
    *outType = bestType;
    *outSubType = bestSubType;
    *outOrder = bestOrder;
    return 1;
}

static int tileHasLineEntity(uint16_t tileIndex, uint8_t* outHasLine) {
    const EspMapFacingIndexView* index = EspMapFacingIndex_view();
    uint32_t i;
    uint8_t type;
    uint16_t lineTile;

    if (index == NULL || outHasLine == NULL) return 0;
    *outHasLine = 0U;
    for (i = 0U; i < index->lineCount; ++i) {
        if (!EspMapFacingIndex_getLineEntityType(i, &type)) return 0;
        if (type == ESP_MAP_FACING_LINE_NO_ENTITY) continue;
        if (!lineEntityTile(i, &lineTile)) return 0;
        if (lineTile == tileIndex) {
            *outHasLine = 1U;
            return 1;
        }
    }
    return 1;
}

static int selectFacing(const EspPlayerViewState* view,
                        EspPostSpawnRefreshState* next) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapFacingIndexView* index = EspMapFacingIndex_view();
    int32_t srcX;
    int32_t srcY;
    int32_t destX;
    int32_t destY;
    int stepX;
    int stepY;
    int sX;
    int sY;
    int dX;
    int dY;
    int pX = 0;
    int pY = 0;
    int cnt = 0;
    int srcPitch;
    int srcIndex;
    int sourceRawTileX;
    int sourceRawTileY;
    uint32_t lineIndex;
    uint8_t lineType;
    uint16_t lineTile;
    uint8_t hasLine;
    uint8_t tileFlags;
    uint16_t spriteIndex;
    uint16_t spriteOrder;
    uint16_t belowOrder;
    uint8_t spriteType;
    uint8_t spriteSubType;
    EspMapSprite sprite;
    int stopTrace;

    if (runtime == NULL || index == NULL || view == NULL || next == NULL ||
        runtime->lineCount != index->lineCount || runtime->lineCount > 0xffffU ||
        runtime->mapSpriteCount > 0xffffU ||
        !rayForView(view, &srcX, &srcY, &destX, &destY, &stepX, &stepY)) {
        return 0;
    }

    if (srcX < INT16_MIN || srcX > INT16_MAX ||
        srcY < INT16_MIN || srcY > INT16_MAX ||
        destX < INT16_MIN || destX > INT16_MAX ||
        destY < INT16_MIN || destY > INT16_MAX) {
        return 0;
    }

    next->rayStartX = (int16_t)srcX;
    next->rayStartY = (int16_t)srcY;
    next->rayEndX = (int16_t)destX;
    next->rayEndY = (int16_t)destY;

    sourceRawTileX = srcX >> 6;
    sourceRawTileY = srcY >> 6;
    sX = clampTileCoord(sourceRawTileX);
    sY = clampTileCoord(sourceRawTileY);
    dX = clampTileCoord(destX >> 6);
    dY = clampTileCoord(destY >> 6);

    if (sX > dX) {
        pX = -1;
        cnt = sX - dX + 1;
    }
    else if (sX < dX) {
        pX = 1;
        cnt = dX - sX + 1;
    }
    if (sY > dY) {
        pY = -1;
        cnt = sY - dY + 1;
    }
    else if (sY < dY) {
        pY = 1;
        cnt = dY - sY + 1;
    }

    next->sourceTileIndex = (uint16_t)(sY * MAP_WIDTH + sX);
    next->endTileIndex = (uint16_t)(dY * MAP_WIDTH + dX);
    srcPitch = pY * MAP_WIDTH + pX;
    srcIndex = next->sourceTileIndex;

    while (--cnt >= 0 && next->traceEntityCount < TRACE_ENTITY_LIMIT) {
        ++next->tracedTileCount;

        if (!tileHasLineEntity((uint16_t)srcIndex, &hasLine) ||
            !EspMapState_getTileFlags((uint32_t)srcIndex, &tileFlags)) {
            return 0;
        }

        /* Legacy post-load wall sentinel replaces a non-line tile head and
         * therefore masks every map-sprite entity otherwise linked there. */
        if (hasLine == 0U && (tileFlags & ESP_MAP_TILE_WALL) != 0U) {
            ++next->traceEntityCount;
            next->facingKind = ESP_POST_SPAWN_FACING_WALL;
            next->facingEntityType = ESP_MAP_FACING_DEFAULT_WALL_TYPE;
            next->facingTileIndex = (uint16_t)srcIndex;
            return 1;
        }

        /* Line entities are linked after map sprites, so descending line index
         * reproduces the initial fresh-map linked-list head order. */
        for (lineIndex = runtime->lineCount; lineIndex-- > 0U;) {
            if (!EspMapFacingIndex_getLineEntityType(lineIndex, &lineType)) {
                return 0;
            }
            if (lineType == ESP_MAP_FACING_LINE_NO_ENTITY ||
                !lineEntityTile(lineIndex, &lineTile) ||
                lineTile != (uint16_t)srcIndex || !typeIsTraced(lineType)) {
                continue;
            }

            ++next->traceEntityCount;
            next->facingKind = ESP_POST_SPAWN_FACING_LINE;
            next->facingEntityType = lineType;
            next->facingIndex = (uint16_t)lineIndex;
            next->facingTileIndex = (uint16_t)srcIndex;
            return 1;
        }

        belowOrder = 0xffffU;
        while (next->traceEntityCount < TRACE_ENTITY_LIMIT) {
            if (!findNextLinkedSprite((uint16_t)srcIndex, belowOrder,
                                      &spriteIndex, &spriteType,
                                      &spriteSubType, &spriteOrder)) {
                return 0;
            }
            if (spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) break;
            belowOrder = spriteOrder;
            if (!typeIsTraced(spriteType)) continue;
            if (!EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;

            stopTrace = 0;
            if (spriteType == 14U || spriteType == 15U) {
                if ((sprite.info & 0x00020000UL) != 0U) {
                    if (!spriteCrossesSpecialPlane(&sprite, srcX, srcY,
                                                   destX, destY)) {
                        continue;
                    }
                    stopTrace = 1;
                }
            }

            ++next->traceEntityCount;
            if (spriteType == 14U ||
                ((int32_t)sprite.x >> 6) != sourceRawTileX ||
                ((int32_t)sprite.y >> 6) != sourceRawTileY) {
                next->facingKind = ESP_POST_SPAWN_FACING_SPRITE;
                next->facingEntityType = spriteType;
                next->facingEntitySubType = spriteSubType;
                next->facingIndex = spriteIndex;
                next->facingTileIndex = (uint16_t)srcIndex;
                return 1;
            }
            if (stopTrace != 0 ||
                next->traceEntityCount >= TRACE_ENTITY_LIMIT) {
                return 1;
            }
        }

        srcIndex += srcPitch;
        if (srcIndex < 0 || srcIndex >= (int)MAP_TILE_COUNT) return 0;
    }

    return 1;
}

void EspPostSpawnRefresh_reset(void) {
    memset(&refreshState, 0, sizeof(refreshState));
}

int EspPostSpawnRefresh_isReady(void) {
    return refreshState.active == 1U && refreshState.hudRefreshRouted == 1U &&
           refreshState.facingResolved == 1U;
}

const EspPostSpawnRefreshState* EspPostSpawnRefresh_view(void) {
    return EspPostSpawnRefresh_isReady() ? &refreshState : NULL;
}

EspPostSpawnRefreshStatus EspPostSpawnRefresh_query(
    const EspPlayerViewState* playerView,
    EspPostSpawnRefreshState* outState) {
    const EspMapRuntimeView* runtime;
    const EspMapLineStateView* lineState;
    const EspMapSpriteTopologyView* topology;
    const EspMapFacingIndexView* index;
    EspPostSpawnRefreshState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || outState == NULL) {
        return ESP_POST_SPAWN_REFRESH_INVALID;
    }
    if (!EspMapResidentLifecycle_isReady() || !EspPlayerView_isReady()) {
        return ESP_POST_SPAWN_REFRESH_NOT_READY;
    }
    if (playerView->active != 1U || playerView->spawnApplied != 1U ||
        playerView->loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP ||
        !EspMapCatalog_isValidId(playerView->targetMapId) ||
        playerView->gameplayLoadMapId == 0U ||
        playerView->hudRefreshPending != 1U ||
        playerView->facingRefreshPending != 1U ||
        playerView->playerSetupPending != 1U ||
        playerView->tileEnterPending != 1U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle ||
        (((uint32_t)playerView->destAngle & 63U) != 0U)) {
        return ESP_POST_SPAWN_REFRESH_UNSUPPORTED_CONTEXT;
    }

    runtime = EspMapRuntime_view();
    lineState = EspMapLineState_view();
    topology = EspMapSpriteTopology_view();
    index = EspMapFacingIndex_view();
    if (runtime == NULL || lineState == NULL || topology == NULL || index == NULL ||
        index->lineCount != runtime->lineCount ||
        topology->spriteCount != runtime->mapSpriteCount) {
        return ESP_POST_SPAWN_REFRESH_INDEX_NOT_READY;
    }

    /* Initial Game_spawnPlayer() refresh happens before any door event. The
     * current compact line state does not yet own legacy line relink history,
     * so reject a mutated door world rather than guess linked-list order. */
    if (lineState->openCount != 0U) {
        return ESP_POST_SPAWN_REFRESH_WORLD_MUTATED;
    }

    memset(&next, 0, sizeof(next));
    next.facingIndex = ESP_POST_SPAWN_NO_INDEX;
    next.facingTileIndex = ESP_POST_SPAWN_NO_INDEX;
    next.facingEntityType = ESP_POST_SPAWN_NO_TYPE;
    next.facingEntitySubType = ESP_POST_SPAWN_NO_TYPE;
    next.hudRefreshIntent = 1U;
    next.hudRefreshRouted = 1U;
    next.facingResolved = 1U;
    next.active = 1U;
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;

    if (!selectFacing(playerView, &next)) {
        return ESP_POST_SPAWN_REFRESH_QUERY_FAILED;
    }

    *outState = next;
    return ESP_POST_SPAWN_REFRESH_OK;
}

EspPostSpawnRefreshStatus EspPostSpawnRefresh_apply(void) {
    const EspPlayerViewState* playerView;
    EspPostSpawnRefreshState next;
    EspPostSpawnRefreshStatus status;

    if (EspPostSpawnRefresh_isReady()) {
        return ESP_POST_SPAWN_REFRESH_ALREADY_ACTIVE;
    }
    playerView = EspPlayerView_view();
    if (playerView == NULL) return ESP_POST_SPAWN_REFRESH_NOT_READY;

    status = EspPostSpawnRefresh_query(playerView, &next);
    if (status != ESP_POST_SPAWN_REFRESH_OK) return status;
    if (!EspPlayerView_consumePostSpawnRefresh(&next)) {
        return ESP_POST_SPAWN_REFRESH_VIEW_CONSUME_FAILED;
    }

    refreshState = next;
    return ESP_POST_SPAWN_REFRESH_OK;
}
