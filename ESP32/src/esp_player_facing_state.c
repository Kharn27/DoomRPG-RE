#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_topology_query.h"
#include "esp_player_facing_state.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_MAX_COUNT 1024U
#define MAP_WIDTH 32U
#define MAP_TILE_SIZE 64
#define BLOCK_FLAG_WALL 0x01U
#define ENTITY_INFO_LINE 0x00200000UL
#define SPRITE_FLAG_WALL 0x00020000UL
#define SPRITE_FLAG_NORTH 0x00080000UL
#define SPRITE_FLAG_SOUTH 0x00100000UL
#define SPRITE_FLAG_EAST 0x00200000UL
#define SPRITE_FLAG_WEST 0x00400000UL
#define LINE_FLAG_SOUTH 0x00000800UL
#define LINE_FLAG_NORTH 0x00001000UL
#define LINE_FLAG_WEST 0x00002000UL
#define LINE_FLAG_EAST 0x00004000UL
#define TRACE_ENTITY_LIMIT 8U

static EspPlayerFacingState facingState;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int identityMatches(const EspPlayerViewState* view,
                           uint8_t targetMapId,
                           uint8_t gameplayLoadMapId,
                           uint8_t loadType) {
    return view != NULL && view->targetMapId == targetMapId &&
           view->gameplayLoadMapId == gameplayLoadMapId &&
           view->loadType == loadType;
}

static int traceTypeEnabled(uint8_t type) {
    return type < 32U &&
           (ESP_PLAYER_FACING_TRACE_FLAGS & (1UL << type)) != 0U;
}

static int spritePlaneCrosses(const EspMapSprite* sprite,
                              int32_t srcX,
                              int32_t srcY,
                              int32_t destX,
                              int32_t destY) {
    uint32_t info;
    int32_t sprX;
    int32_t sprY;

    if (sprite == NULL) return 0;
    info = sprite->info;
    if ((info & SPRITE_FLAG_WALL) == 0U) return 0;
    sprX = (int32_t)sprite->x;
    sprY = (int32_t)sprite->y;

    if ((info & (SPRITE_FLAG_NORTH | SPRITE_FLAG_SOUTH)) != 0U) {
        return (srcY <= sprY && destY > sprY) ||
               (srcY >= sprY && destY < sprY);
    }
    if ((info & (SPRITE_FLAG_EAST | SPRITE_FLAG_WEST)) != 0U) {
        return (srcX <= sprX && destX > sprX) ||
               (srcX >= sprX && destX < sprX);
    }
    return 0;
}

static int resolveEntityDef(const EspAssetPackEntry* entry,
                            uint16_t lookup,
                            uint8_t* outFound,
                            uint8_t* outType,
                            uint8_t* outSubType) {
    uint8_t header[2];
    uint8_t record[4];
    uint32_t count;
    uint32_t i;

    if (outFound != NULL) *outFound = 0U;
    if (outType != NULL) *outType = 0xffU;
    if (outSubType != NULL) *outSubType = 0xffU;
    if (entry == NULL || outFound == NULL || outType == NULL ||
        outSubType == NULL || !EspAssetPack_readRange(entry, 0U, header, 2U)) return 0;

    count = readLe16(header);
    if (count == 0U || count > ENTITY_DEF_MAX_COUNT ||
        2U + count * ENTITY_DEF_RECORD_BYTES > entry->size) return 0;

    for (i = 0U; i < count; ++i) {
        if (!EspAssetPack_readRange(entry,
                                    2U + i * ENTITY_DEF_RECORD_BYTES,
                                    record, sizeof(record))) return 0;
        if (readLe16(record) == lookup) {
            *outFound = 1U;
            *outType = record[2];
            *outSubType = record[3] & 0x7fU;
            return 1;
        }
    }
    return 1;
}

static int lineEntityTile(const EspMapLine* line, uint16_t* outTile) {
    int32_t x;
    int32_t y;

    if (line == NULL || outTile == NULL) return 0;
    x = (int32_t)line->x1 + (((int32_t)line->x2 - (int32_t)line->x1) / 2);
    y = (int32_t)line->y1 + (((int32_t)line->y2 - (int32_t)line->y1) / 2);

    if ((line->flags & LINE_FLAG_SOUTH) != 0U) --y;
    else if ((line->flags & LINE_FLAG_WEST) != 0U) ++x;
    else if ((line->flags & LINE_FLAG_NORTH) != 0U) ++y;
    else if ((line->flags & LINE_FLAG_EAST) != 0U) --x;

    if (x < 0 || y < 0 || x >= MAP_WIDTH * MAP_TILE_SIZE ||
        y >= MAP_WIDTH * MAP_TILE_SIZE) return 0;
    *outTile = (uint16_t)(((uint32_t)y >> 6U) * MAP_WIDTH +
                          ((uint32_t)x >> 6U));
    return 1;
}

static int appendTrace(EspPlayerFacingState* state) {
    if (state == NULL || state->traceEntityCount >= TRACE_ENTITY_LIMIT) return 0;
    ++state->traceEntityCount;
    return 1;
}

static void setNoneDefaults(EspPlayerFacingState* state) {
    if (state == NULL) return;
    state->hitIndex = ESP_PLAYER_FACING_NO_INDEX;
    state->hitTile = ESP_PLAYER_FACING_NO_TILE;
    state->entityType = 0xffU;
    state->entitySubType = 0xffU;
}

static EspPlayerFacingStatus resolveTrace(EspPlayerFacingState* state) {
    const EspMapRuntimeView* runtime;
    const EspMapSpriteTopologyView* topology;
    const EspMapLineStateView* lineState;
    EspAssetPackEntry defsEntry;
    EspMapTopologyEntityRef entity;
    EspMapSprite sprite;
    EspMapLine line;
    uint16_t tile;
    uint16_t beforeOrder;
    uint16_t lineTile;
    uint32_t lineIndex;
    int32_t tileY;
    int32_t endTileY;
    uint8_t blockFlags;
    uint8_t defFound;
    uint8_t type;
    uint8_t subType;
    uint8_t lineOpen;
    int initialLineEntityPresent;
    int stopTrace;
    int queryResult;
    int opened = 0;
    EspPlayerFacingStatus status = ESP_PLAYER_FACING_OK;

    if (state == NULL || !EspMapRuntime_isLoaded() ||
        !EspMapSpriteTopology_isReady() || !EspMapLineState_isReady()) {
        return ESP_PLAYER_FACING_TOPOLOGY_INVALID;
    }
    runtime = EspMapRuntime_view();
    topology = EspMapSpriteTopology_view();
    lineState = EspMapLineState_view();
    if (runtime == NULL || topology == NULL || lineState == NULL ||
        runtime->lineCount != lineState->lineCount) return ESP_PLAYER_FACING_TOPOLOGY_INVALID;

    if (EspAssetPack_isOpen() || !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_PLAYER_FACING_STORAGE_ERROR;
    }
    opened = 1;
    if (!EspAssetPack_findEntry("/entities.db", &defsEntry) ||
        (defsEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
        status = ESP_PLAYER_FACING_STORAGE_ERROR;
        goto cleanup;
    }

    tileY = state->traceStartY >> 6;
    endTileY = state->traceEndY >> 6;
    stopTrace = 0;

    while (tileY >= endTileY && !stopTrace) {
        tile = (uint16_t)((uint32_t)tileY * MAP_WIDTH +
                          ((uint32_t)state->traceStartX >> 6U));
        initialLineEntityPresent = 0;

        lineIndex = runtime->lineCount;
        while (lineIndex != 0U) {
            --lineIndex;
            if (!EspMapRuntime_getLine(lineIndex, &line) ||
                !lineEntityTile(&line, &lineTile)) {
                status = ESP_PLAYER_FACING_TOPOLOGY_INVALID;
                goto cleanup;
            }
            if (lineTile != tile) continue;

            if (!resolveEntityDef(&defsEntry, (uint16_t)(305U + line.texture),
                                  &defFound, &type, &subType)) {
                status = ESP_PLAYER_FACING_STORAGE_ERROR;
                goto cleanup;
            }
            if (defFound == 0U) continue;
            initialLineEntityPresent = 1;

            if (!EspMapLineState_getOpen(lineIndex, &lineOpen)) {
                status = ESP_PLAYER_FACING_TOPOLOGY_INVALID;
                goto cleanup;
            }
            if (lineOpen != 0U || !traceTypeEnabled(type)) continue;

            if (type == 14U || type == 15U) {
                if (lineIndex >= runtime->mapSpriteCount ||
                    !EspMapRuntime_getMapSprite(lineIndex, &sprite)) {
                    status = ESP_PLAYER_FACING_UNSUPPORTED_CONTEXT;
                    goto cleanup;
                }
                if (!spritePlaneCrosses(&sprite, state->traceStartX,
                                        state->traceStartY, state->traceEndX,
                                        state->traceEndY)) continue;
            }

            if (!appendTrace(state)) {
                status = ESP_PLAYER_FACING_TRACE_OVERFLOW;
                goto cleanup;
            }
            state->legacyIdentity = ((uint32_t)lineIndex + 1U) | ENTITY_INFO_LINE;
            state->hitIndex = (uint16_t)lineIndex;
            state->hitTile = tile;
            state->kind = ESP_PLAYER_FACING_KIND_LINE;
            state->entityType = type;
            state->entitySubType = subType;
            state->active = 1U;
            goto cleanup;
        }

        if (!EspMapRuntime_getBlockCell(tile, &blockFlags)) {
            status = ESP_PLAYER_FACING_TOPOLOGY_INVALID;
            goto cleanup;
        }
        if (!initialLineEntityPresent && (blockFlags & BLOCK_FLAG_WALL) != 0U) {
            if (!appendTrace(state)) {
                status = ESP_PLAYER_FACING_TRACE_OVERFLOW;
                goto cleanup;
            }
            state->legacyIdentity = 0U;
            state->hitIndex = ESP_PLAYER_FACING_NO_INDEX;
            state->hitTile = tile;
            state->kind = ESP_PLAYER_FACING_KIND_WALL;
            state->entityType = 0xffU;
            state->entitySubType = 0xffU;
            state->active = 1U;
            goto cleanup;
        }

        beforeOrder = 0xffffU;
        queryResult = EspMapTopologyQuery_findLinkedOnTile(tile, beforeOrder, &entity);
        while (queryResult > 0) {
            beforeOrder = entity.linkOrder;
            if (traceTypeEnabled(entity.type)) {
                if (entity.type == 14U || entity.type == 15U) {
                    if (!EspMapRuntime_getMapSprite(entity.spriteIndex, &sprite)) {
                        status = ESP_PLAYER_FACING_TOPOLOGY_INVALID;
                        goto cleanup;
                    }
                    if (spritePlaneCrosses(&sprite, state->traceStartX,
                                           state->traceStartY, state->traceEndX,
                                           state->traceEndY)) {
                        stopTrace = 1;
                    }
                    else {
                        queryResult = EspMapTopologyQuery_findLinkedOnTile(
                            tile, beforeOrder, &entity);
                        continue;
                    }
                }

                if (!appendTrace(state)) {
                    status = ESP_PLAYER_FACING_TRACE_OVERFLOW;
                    goto cleanup;
                }

                if (entity.type == 14U ||
                    ((entity.x >> 6U) != ((uint32_t)state->traceStartX >> 6U)) ||
                    ((entity.y >> 6U) != ((uint32_t)state->traceStartY >> 6U))) {
                    state->legacyIdentity = (uint32_t)entity.spriteIndex + 1U;
                    state->hitIndex = entity.spriteIndex;
                    state->hitTile = tile;
                    state->kind = ESP_PLAYER_FACING_KIND_SPRITE;
                    state->entityType = entity.type;
                    state->entitySubType = entity.subType;
                    state->active = 1U;
                    goto cleanup;
                }
                if (stopTrace) break;
            }
            queryResult = EspMapTopologyQuery_findLinkedOnTile(tile, beforeOrder, &entity);
        }
        if (queryResult < 0) {
            status = ESP_PLAYER_FACING_TOPOLOGY_INVALID;
            goto cleanup;
        }

        --tileY;
    }

    state->kind = ESP_PLAYER_FACING_KIND_NONE;
    state->legacyIdentity = 0U;
    setNoneDefaults(state);
    state->active = 1U;

cleanup:
    if (opened) EspAssetPack_close();
    if (status != ESP_PLAYER_FACING_OK) memset(state, 0, sizeof(*state));
    return status;
}

void EspPlayerFacing_reset(void) {
    memset(&facingState, 0, sizeof(facingState));
}

int EspPlayerFacing_isReady(void) {
    return facingState.active == 1U;
}

const EspPlayerFacingState* EspPlayerFacing_view(void) {
    return EspPlayerFacing_isReady() ? &facingState : NULL;
}

EspPlayerFacingStatus EspPlayerFacing_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    const EspPlayerOrientationState* orientation,
    const EspPlayerFinishRotationTileState* secondTile,
    EspPlayerFacingState* outState) {
    EspPlayerFacingState next;
    EspPlayerFacingStatus traceStatus;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || initialTile == NULL || orientation == NULL ||
        secondTile == NULL || outState == NULL) return ESP_PLAYER_FACING_INVALID;

    if (playerView->active != 1U || playerView->spawnApplied != 1U) {
        return ESP_PLAYER_FACING_VIEW_INVALID;
    }
    if (initialTile->active != 1U ||
        !identityMatches(playerView, initialTile->targetMapId,
                         initialTile->gameplayLoadMapId, initialTile->loadType)) {
        return ESP_PLAYER_FACING_INITIAL_INVALID;
    }
    if (orientation->active != 1U || orientation->prepared != 1U ||
        !identityMatches(playerView, orientation->targetMapId,
                         orientation->gameplayLoadMapId, orientation->loadType)) {
        return ESP_PLAYER_FACING_ORIENTATION_INVALID;
    }
    if (secondTile->active != 1U ||
        !identityMatches(playerView, secondTile->targetMapId,
                         secondTile->gameplayLoadMapId, secondTile->loadType)) {
        return ESP_PLAYER_FACING_SECOND_TILE_INVALID;
    }

    if (playerView->loadType != 0U || playerView->destAngle != 64 ||
        playerView->viewAngle != 64 || orientation->destAngle != 64U ||
        orientation->viewSin != 65536 || orientation->viewCos != 0 ||
        orientation->viewStepX != 0 || orientation->viewStepY != -64 ||
        secondTile->inputFlags != ESP_PLAYER_FINISH_ROTATION_TILE_FLAGS ||
        playerView->destX != playerView->viewX ||
        playerView->destY != playerView->viewY) {
        return ESP_PLAYER_FACING_UNSUPPORTED_CONTEXT;
    }
    if (playerView->hudRefreshPending != 0U ||
        playerView->playerSetupPending != 0U ||
        playerView->tileEnterPending != 0U ||
        playerView->facingRefreshPending != 1U) {
        return ESP_PLAYER_FACING_UNSUPPORTED_ORDER;
    }

    memset(&next, 0, sizeof(next));
    next.traceStartX = playerView->destX +
        (int32_t)(((int64_t)orientation->viewCos * ESP_PLAYER_FACING_NEAR_OFFSET) >> 16);
    next.traceStartY = playerView->destY +
        (int32_t)(((-(int64_t)orientation->viewSin) * ESP_PLAYER_FACING_NEAR_OFFSET) >> 16);
    next.traceEndX = next.traceStartX +
        ESP_PLAYER_FACING_STEP_COUNT * orientation->viewStepX;
    next.traceEndY = next.traceStartY +
        ESP_PLAYER_FACING_STEP_COUNT * orientation->viewStepY;
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;
    setNoneDefaults(&next);

    if (next.traceStartX != next.traceEndX || next.traceStartY <= next.traceEndY ||
        next.traceStartX < 0 || next.traceStartY < 0 || next.traceEndY < 0 ||
        next.traceStartX >= MAP_WIDTH * MAP_TILE_SIZE ||
        next.traceStartY >= MAP_WIDTH * MAP_TILE_SIZE ||
        next.traceEndY >= MAP_WIDTH * MAP_TILE_SIZE ||
        (next.traceStartY >> 6) - (next.traceEndY >> 6) > 3) {
        return ESP_PLAYER_FACING_UNSUPPORTED_CONTEXT;
    }

    traceStatus = resolveTrace(&next);
    if (traceStatus != ESP_PLAYER_FACING_OK) return traceStatus;

    *outState = next;
    return ESP_PLAYER_FACING_OK;
}

EspPlayerFacingStatus EspPlayerFacing_route(void) {
    EspPlayerFacingState next;
    EspPlayerFacingStatus status;
    const EspPlayerViewState* view;

    if (EspPlayerFacing_isReady()) return ESP_PLAYER_FACING_ALREADY_ACTIVE;

    view = EspPlayerView_view();
    status = EspPlayerFacing_prepare(
        view, EspPlayerInitialTile_view(), EspPlayerOrientation_view(),
        EspPlayerFinishRotationTile_view(), &next);
    if (status != ESP_PLAYER_FACING_OK) return status;

    if (!EspPlayerView_consumeFacing(next.targetMapId,
                                     next.gameplayLoadMapId,
                                     next.loadType)) {
        return ESP_PLAYER_FACING_VIEW_CONSUME_FAILED;
    }

    facingState = next;
    return ESP_PLAYER_FACING_OK;
}
