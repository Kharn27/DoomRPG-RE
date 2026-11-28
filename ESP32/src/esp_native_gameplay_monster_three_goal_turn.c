#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "doomrpg_log.h"
#include "esp_perf_game_p1.h"
#if DOOMRPG_PERF_GAME_P1
#include "esp_perf_trace_cache.h"
#include "esp_perf_spatial_sprite.h"
#include "esp_perf_spatial_line.h"
#endif
#if DOOMRPG_PERF_GAME_P1
#include <esp_timer.h>
#endif
#include "esp_native_process_rng_owner.h"

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_activation.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_three_goal_turn.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_player_view_state.h"

#define ESP_PERF_LINE_FIRST_KIND 2
#define THREEGOAL_MAP_WIDTH 32U
#define THREEGOAL_TILE_SIZE 64
#define THREEGOAL_TILE_CENTER 32
#define THREEGOAL_TRACE_LEGACY_MASK 0xff87U
#define THREEGOAL_TYPE_ENEMY 1U
#define THREEGOAL_TYPE_SPECIAL_TRACE 14U
#define THREEGOAL_TYPE_SPECIAL_TRACE_2 15U
#define THREEGOAL_SPECIAL_TRACE_ENTITY_FLAG 0x00020000UL
#define THREEGOAL_SPECIAL_TRACE_Y_MASK 0x00180000UL
#define THREEGOAL_SPECIAL_TRACE_X_MASK 0x00600000UL
#define THREEGOAL_LINE_ENTITY_DEF_BASE 305U
#define THREEGOAL_LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL
#define THREEGOAL_LINE_GEOMETRY_AXIS_X 0x00000008UL
#define THREEGOAL_LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define THREEGOAL_LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define THREEGOAL_LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define THREEGOAL_LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define THREEGOAL_LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define THREEGOAL_LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define THREEGOAL_LINE_ENTITY_NUDGE_X_NEG 0x00004000UL
#define THREEGOAL_TOTAL_GOALS 3U
#define THREEGOAL_MULTI_LOOP_SHOTS 3U

typedef struct ThreeGoalProbeRng_s {
    Random_t state;
    uint32_t calls;
    uint8_t boundary;
    uint8_t reserved[3];
} ThreeGoalProbeRng;

typedef struct ThreeGoalMovePlan_s {
    int32_t deltaX;
    int32_t deltaY;
    uint16_t sourceTile;
    uint16_t destTile;
    uint16_t traceMask;
    uint8_t visitCount;
    uint8_t choice;
    uint8_t tieRand;
    uint8_t tieRandUsed;
    int navScore[4]; /* E,W,S,N; -1 = first step blocked. */
    uint8_t navTraceBits[4]; /* clear bits for the two greedy rounds. */
} ThreeGoalMovePlan;

typedef struct ThreeGoalPathResult_s {
    int distance;
    int status;
    uint8_t navTraceBits;
} ThreeGoalPathResult;

static EspNativeGameplayMonsterThreeGoalTurnView threeGoalView;
/* Per-plan origin-plane witness; no retained world state. */
static uint16_t originPlaneClear, originPlaneBlocked;
#if DOOMRPG_PERF_GAME_P1
static uint8_t navSampleCount;
#endif
static EspNativeGameplayMonsterMovementView syntheticMovementView;

static int centeredCoordinate(int32_t value) {
    return value >= THREEGOAL_TILE_CENTER &&
           value <= (int32_t)(((THREEGOAL_MAP_WIDTH - 1U) * THREEGOAL_TILE_SIZE) +
                              THREEGOAL_TILE_CENTER) &&
           (value & (THREEGOAL_TILE_SIZE - 1)) == THREEGOAL_TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= THREEGOAL_MAP_WIDTH || tileY >= THREEGOAL_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * THREEGOAL_MAP_WIDTH + tileX);
    return 1;
}

static int tileCenterForCoord(int tileX, int tileY, int32_t* outX, int32_t* outY) {
    if (outX == NULL || outY == NULL || tileX < 0 || tileY < 0 ||
        tileX >= (int)THREEGOAL_MAP_WIDTH || tileY >= (int)THREEGOAL_MAP_WIDTH) {
        return 0;
    }
    *outX = tileX * THREEGOAL_TILE_SIZE + THREEGOAL_TILE_CENTER;
    *outY = tileY * THREEGOAL_TILE_SIZE + THREEGOAL_TILE_CENTER;
    return 1;
}

static int typeInMask(uint8_t type, uint16_t mask) {
    return type < 16U && (mask & (uint16_t)(1U << type)) != 0U;
}

static uint16_t movementMaskForSubtype(uint8_t subtype) {
    uint16_t mask = THREEGOAL_TRACE_LEGACY_MASK;
    if (subtype == 4U || subtype == 13U) mask &= (uint16_t)~0x0c00U;
    return mask;
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

    if ((line->flags & THREEGOAL_LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & THREEGOAL_LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & THREEGOAL_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & THREEGOAL_LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & THREEGOAL_LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & THREEGOAL_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & THREEGOAL_LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & THREEGOAL_LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & THREEGOAL_LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & THREEGOAL_LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    if (x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= THREEGOAL_MAP_WIDTH || tileY >= THREEGOAL_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * THREEGOAL_MAP_WIDTH + tileX);
    return 1;
}

static int closedLineBlocksTileReference(uint16_t tile, uint16_t mask) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;
    if (runtime == NULL || !EspEntityDefTypeCatalog_isReady()) return -1;

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
        lookup = THREEGOAL_LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        (void)subtype;
        if (!hasDefinition) {
            if ((line.flags & THREEGOAL_LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            type = 0U;
        }
        if (!typeInMask(type, mask)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile == tile) return 1;
    }
    return 0;
}
#if DOOMRPG_PERF_GAME_P1
/* P1j independent full reference remains authoritative in shadow mode. */
static int closedLineBlocksTileIndexed(uint16_t tile, uint16_t mask) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;
    if (runtime == NULL || !EspEntityDefTypeCatalog_isReady()) return -1;

    i = EspPerfSpatialLine_first(&EspPerfGameP1_spatialLineIndex, tile);
    while (i != ESP_PERF_SPATIAL_LINE_NONE) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t type;
        uint8_t subtype;
        int hasDefinition;

        uint32_t lineIndex = i;
        i = EspPerfSpatialLine_next(&EspPerfGameP1_spatialLineIndex, (uint16_t)i, tile);
        if (!EspMapLineState_getOpen(lineIndex, &open)) return -1;
        if (open != 0U) continue;
        if (!EspMapRuntime_getLine(lineIndex, &line)) return -1;
        lookup = THREEGOAL_LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        (void)subtype;
        if (!hasDefinition) {
            if ((line.flags & THREEGOAL_LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            type = 0U;
        }
        if (!typeInMask(type, mask)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile == tile) return 1;
    }
    return 0;
}
static int closedLineBlocksTile(uint16_t tile, uint16_t mask) {
    if(EspPerfGameP1_spatialLineIndex.active) {
        const int indexed=closedLineBlocksTileIndexed(tile,mask);
#if DOOMRPG_PERF_LINE_FAST
        EspPerfGameP1_lineShadow(ESP_PERF_LINE_FIRST_KIND,0);
        return indexed;
#else
        const int reference=closedLineBlocksTileReference(tile,mask);
        EspPerfGameP1_lineShadow(ESP_PERF_LINE_FIRST_KIND, indexed!=reference);
        return reference;
#endif
    }
    return closedLineBlocksTileReference(tile,mask);
}
#else
#define closedLineBlocksTile closedLineBlocksTileReference
#endif


static int specialEntityBlocks(uint32_t spriteIndex,
                               int32_t sourceX,
                               int32_t sourceY,
                               int32_t destX,
                               int32_t destY,
                               int tileOrigin) {
    EspMapSprite sprite;
    int32_t sprX;
    int32_t sprY;
    if (!EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return -1;
    if ((sprite.info & THREEGOAL_SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;
    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
    /* Entity_calcPath passes tile origins (x << 6), unlike the initial
     * Entity_aiGoal_MOVE center-to-center LOS. Tile membership is unchanged;
     * the special-plane crossing must use the original endpoints. */
    if (tileOrigin) {
        sourceX -= THREEGOAL_TILE_CENTER;
        sourceY -= THREEGOAL_TILE_CENTER;
        destX -= THREEGOAL_TILE_CENTER;
        destY -= THREEGOAL_TILE_CENTER;
    }
    if ((sprite.info & THREEGOAL_SPECIAL_TRACE_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite.info & THREEGOAL_SPECIAL_TRACE_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
}

/* Return 1 blocked, 0 clear, -1 not ready. For calcPath's single-tile
 * subtrace, evaluate type14/15 with the legacy tile-origin endpoints. */
static int blockingSpriteOnTileReference(uint16_t tile,
                                int32_t sourceX,
                                int32_t sourceY,
                                int32_t destX,
                                int32_t destY,
                                uint16_t attackerSprite,
                                uint16_t mask,
                                int strictSpecial) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t bestOrder = 0U;
    int found = 0;
    uint32_t i;

    if (topology == NULL) return -1;
    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int specialBlocks;

        if (i == attackerSprite) continue;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) return -1;
        (void)subtype;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !typeInMask(type, mask)) {
            continue;
        }
        if (type == THREEGOAL_TYPE_SPECIAL_TRACE ||
            type == THREEGOAL_TYPE_SPECIAL_TRACE_2) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY,
                                                strictSpecial);
            if (specialBlocks < 0) return -1;
            if (strictSpecial) {
                if (specialBlocks) ++originPlaneBlocked;
                else ++originPlaneClear;
            }
            if (specialBlocks == 0) continue;
        }
        if (!found || linkOrder > bestOrder) {
            found = 1;
            bestOrder = linkOrder;
        }
    }
    return found;
}

#if DOOMRPG_PERF_GAME_P1
/* P1H-SPRITE-INDEX BEGIN: shadow authority stays with the original scan. */
static int blockingSpriteOnTileIndexed(uint16_t tile,
                                int32_t sourceX,
                                int32_t sourceY,
                                int32_t destX,
                                int32_t destY,
                                uint16_t attackerSprite,
                                uint16_t mask,
                                int strictSpecial) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t bestOrder = 0U;
    int found = 0;
    uint32_t i;

    if (topology == NULL) return -1;
    for (i = EspPerfSpatialSprite_first(&EspPerfGameP1_spatialSpriteIndex, tile);
         i != ESP_PERF_SPATIAL_SPRITE_NONE;
         i = EspPerfSpatialSprite_next(&EspPerfGameP1_spatialSpriteIndex, (uint16_t)i, tile)) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int specialBlocks;

        if (i == attackerSprite) continue;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) return -1;
        (void)subtype;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !typeInMask(type, mask)) {
            continue;
        }
        if (type == THREEGOAL_TYPE_SPECIAL_TRACE ||
            type == THREEGOAL_TYPE_SPECIAL_TRACE_2) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY,
                                                strictSpecial);
            if (specialBlocks < 0) return -1;
            if (strictSpecial) {
                if (specialBlocks) ++originPlaneBlocked;
                else ++originPlaneClear;
            }
            if (specialBlocks == 0) continue;
        }
        if (!found || linkOrder > bestOrder) {
            found = 1;
            bestOrder = linkOrder;
        }
    }
    return found;
}

static int blockingSpriteOnTile(uint16_t tile,
                                 int32_t sourceX,int32_t sourceY,
                                 int32_t destX,int32_t destY,
                                 uint16_t attackerSprite,uint16_t mask,
                                 int strictSpecial) {
    if(EspPerfGameP1_spatialSpriteIndex.active != 0U) {
        const int indexed = blockingSpriteOnTileIndexed(tile, sourceX, sourceY, destX, destY, attackerSprite, mask, strictSpecial);
#if DOOMRPG_PERF_SPATIAL_FAST
        EspPerfGameP1_spatialShadow(1,0);
        return indexed;
#else
        const int reference = blockingSpriteOnTileReference(tile, sourceX, sourceY, destX, destY, attackerSprite, mask, strictSpecial);
        EspPerfGameP1_spatialShadow(1,indexed!=reference);
        return reference;
#endif
    }
    return blockingSpriteOnTileReference(tile, sourceX, sourceY, destX, destY, attackerSprite, mask, strictSpecial);
}
/* P1H-SPRITE-INDEX END */
#else
#define blockingSpriteOnTile blockingSpriteOnTileReference
#endif

static int cardinalTraceClearBody(int32_t sourceX,
                              int32_t sourceY,
                              int32_t destX,
                              int32_t destY,
                              uint16_t attackerSprite,
                              uint16_t mask,
                              int strictSpecial) {
    int32_t stepX = 0;
    int32_t stepY = 0;
    uint32_t distance;
    uint32_t i;
    uint16_t playerTile = UINT16_MAX;

    if (!centeredCoordinate(sourceX) || !centeredCoordinate(sourceY) ||
        !centeredCoordinate(destX) || !centeredCoordinate(destY)) return -1;
    if (sourceX == destX && sourceY != destY) {
        stepY = destY > sourceY ? THREEGOAL_TILE_SIZE : -THREEGOAL_TILE_SIZE;
        distance = (uint32_t)((destY > sourceY ? destY - sourceY : sourceY - destY) /
                              THREEGOAL_TILE_SIZE);
    }
    else if (sourceY == destY && sourceX != destX) {
        stepX = destX > sourceX ? THREEGOAL_TILE_SIZE : -THREEGOAL_TILE_SIZE;
        distance = (uint32_t)((destX > sourceX ? destX - sourceX : sourceX - destX) /
                              THREEGOAL_TILE_SIZE);
    }
    else {
        return 0;
    }
    if (distance == 0U || distance > 31U) return 0;
    /* Only calcPath's single cardinal step is proven for tile-origin mode. */
    if (strictSpecial && distance != 1U) return -2;

    /* Game_trace checks the player sentinel on every visited tile, including
     * the source cell. Topology contains map entities only, so check the
     * settled player explicitly for movement masks, never attack LOS. */
    if ((mask & 0x0100U) != 0U) {
        const EspPlayerViewState* player = EspPlayerView_view();
        if (player == NULL || player->active != 1U ||
            player->viewX != player->destX || player->viewY != player->destY ||
            !tileIndexFor(player->destX, player->destY, &playerTile)) return -1;
    }

    for (i = 1U; i <= distance; ++i) {
        int32_t prevX = sourceX + stepX * (int32_t)(i - 1U);
        int32_t prevY = sourceY + stepY * (int32_t)(i - 1U);
        int32_t x = sourceX + stepX * (int32_t)i;
        int32_t y = sourceY + stepY * (int32_t)i;
        uint16_t tile;
        uint8_t flags;
        int lineBlock;
        int spriteBlock;

        /*
         * Legacy Game_trace() walks the source cell first, then every crossed
         * destination cell. The previous native planner inspected only the
         * destination cell, which let a movement-only thin blocker (for
         * example shoot-through bars linked on the source side) be skipped.
         * Inspect the original source cell exactly once, then keep the existing
         * per-destination walk. The attacker itself remains excluded by
         * blockingSpriteOnTile().
         */
        if (i == 1U) {
            uint16_t sourceTile;
            uint8_t sourceFlags;
            if (!tileIndexFor(prevX, prevY, &sourceTile) ||
                !EspMapState_getTileFlags(sourceTile, &sourceFlags)) {
                return 0;
            }
            if ((sourceFlags & ESP_MAP_TILE_WALL) != 0U) return 0;
            if (sourceTile == playerTile) return 0;
            lineBlock = closedLineBlocksTile(sourceTile, mask);
            if (lineBlock < 0) return -1;
            if (lineBlock > 0) {
                DRPG_LOGT("[MONSTER3GOAL] TRACE-BLOCK side=source kind=line tile=%u mask=%04x from=%d,%d to=%d,%d\n",
                       (unsigned int)sourceTile,
                       (unsigned int)mask,
                       (int)prevX, (int)prevY, (int)x, (int)y);
                return 0;
            }
            spriteBlock = blockingSpriteOnTile(sourceTile,
                                               prevX, prevY, x, y,
                                               attackerSprite, mask,
                                               strictSpecial);
            if (spriteBlock < 0) return spriteBlock;
            if (spriteBlock > 0) {
                DRPG_LOGT("[MONSTER3GOAL] TRACE-BLOCK side=source kind=entity tile=%u mask=%04x from=%d,%d to=%d,%d\n",
                       (unsigned int)sourceTile,
                       (unsigned int)mask,
                       (int)prevX, (int)prevY, (int)x, (int)y);
                return 0;
            }
        }

        if (!tileIndexFor(x, y, &tile) || !EspMapState_getTileFlags(tile, &flags)) {
            return 0;
        }
        if ((flags & ESP_MAP_TILE_WALL) != 0U) return 0;
        if (tile == playerTile) return 0;
        lineBlock = closedLineBlocksTile(tile, mask);
        if (lineBlock < 0) return -1;
        if (lineBlock > 0) return 0;
        spriteBlock = blockingSpriteOnTile(tile,
                                           prevX, prevY, x, y,
                                           attackerSprite, mask, strictSpecial);
        if (spriteBlock < 0) return spriteBlock;
        if (spriteBlock > 0) return 0;
    }
    return 1;
}

#if DOOMRPG_PERF_GAME_P1
#define traceCache EspPerfGameP1_sharedTraceCache
#endif
static int cardinalTraceClear(int32_t sourceX, int32_t sourceY,
                              int32_t destX, int32_t destY,
                              uint16_t attackerSprite, uint16_t mask, int strictSpecial) {
#if DOOMRPG_PERF_GAME_P1
    EspPerfGameP1_phaseBegin(9, (uint64_t)esp_timer_get_time());
#endif
#if DOOMRPG_PERF_GAME_P1
    int cached=0;
    int hit=EspPerfTraceCache_lookup(&traceCache,sourceX,sourceY,destX,destY,attackerSprite,mask,strictSpecial,&cached);
#endif
#if DOOMRPG_PERF_GAME_P1 && DOOMRPG_PERF_TRACE_FAST
    int result=hit?cached:cardinalTraceClearBody(sourceX,sourceY,destX,destY,attackerSprite,mask,strictSpecial);
#else
    int result=cardinalTraceClearBody(sourceX,sourceY,destX,destY,attackerSprite,mask,strictSpecial);
#endif
#if DOOMRPG_PERF_GAME_P1
    if(traceCache.active) {
        EspPerfGameP1_traceShadow(1,hit,hit && cached!=result);
        if(!hit) EspPerfTraceCache_store(&traceCache,sourceX,sourceY,destX,destY,attackerSprite,mask,strictSpecial,result);
    }
#endif
#if DOOMRPG_PERF_GAME_P1
    EspPerfGameP1_phaseEnd(9, (uint64_t)esp_timer_get_time());
#endif
    return result;
}

static ThreeGoalPathResult calcPath(uint16_t attackerSprite,
                                    uint16_t mask,
                                    int srcX,
                                    int srcY,
                                    int destX,
                                    int destY) {
    int visitDist[4] = {0, 0, 0, 0};
    int i;
    ThreeGoalPathResult result;

    result.distance = 999999;
    result.status = 1;
    result.navTraceBits = 0U;
    if (srcX == destX && srcY == destY) {
        result.distance = 0;
        return result;
    }

    for (i = 0; i < 2; ++i) {
        int dist = 999999;
        int stepX = 0;
        int stepY = 0;
        int j;

        for (j = 0; j < 4; ++j) {
            static const int dirX[4] = {1, -1, 0, 0};
            static const int dirY[4] = {0, 0, 1, -1};
            int32_t worldX;
            int32_t worldY;
            int32_t worldDestX;
            int32_t worldDestY;
            int clear;
            int newDist;

            if (!tileCenterForCoord(srcX, srcY, &worldX, &worldY) ||
                !tileCenterForCoord(srcX + dirX[j], srcY + dirY[j],
                                    &worldDestX, &worldDestY)) {
                continue;
            }
            clear = cardinalTraceClear(worldX, worldY, worldDestX, worldDestY,
                                       attackerSprite, mask, 1);
            if (clear > 0) result.navTraceBits |= (uint8_t)(1U << (4 * i + j));
            if (clear == -2) {
                result.status = -2;
                return result;
            }
            if (clear < 0) {
                result.status = -1;
                return result;
            }
            newDist = (srcX + dirX[j] - destX) * (srcX + dirX[j] - destX) +
                      (srcY + dirY[j] - destY) * (srcY + dirY[j] - destY);
            if (clear > 0 && newDist < dist) {
                dist = newDist;
                stepX = dirX[j];
                stepY = dirY[j];
            }
        }
        srcX += stepX;
        srcY += stepY;
        visitDist[(i * 2) + 0] = srcX;
        visitDist[(i * 2) + 1] = srcY;
        if (srcX == destX && srcY == destY) break;
    }

    if (i == 2) {
        int dX = visitDist[2];
        int dY = visitDist[3];
        result.distance = i + (dX - destX) * (dX - destX) +
                          (dY - destY) * (dY - destY);
    }
    else {
        result.distance = i;
    }
    return result;
}

static int probeNextByte(ThreeGoalProbeRng* rng, uint8_t* outValue) {
    int next;
    if (rng == NULL || outValue == NULL) return 0;
    next = rng->state.nextRand;
    if (next < 0 || next + (int)sizeof(byte) >= RANDTABLESIZE) {
        rng->boundary = 1U;
        return 0;
    }
    *outValue = rng->state.randTable[next];
    rng->state.nextRand = next + (int)sizeof(byte);
    ++rng->calls;
    return 1;
}

/* Exact Entity_aiGoal_MOVE continuation planner. Subtype 4/13 has already
 * passed aiThink for this turn, so goals 2/3 target the settled player dest and
 * consume only the visit-choice byte: no ai-decision and no close-bias byte. */
static int aiGoalPlan(uint16_t attackerSprite,
                      uint8_t subtype,
                      int32_t srcX,
                      int32_t srcY,
                      int32_t destX,
                      int32_t destY,
                      ThreeGoalProbeRng* rng,
                      ThreeGoalMovePlan* outPlan) {
    int sX;
    int sY;
    int dX;
    int dY;
    int closestPathDist = 9999;
    uint16_t mask = movementMaskForSubtype(subtype);
    uint8_t visitOrder[4];
    uint8_t visitCount = 0U;
    int clear;
    ThreeGoalPathResult path;

    if (rng == NULL || outPlan == NULL || !centeredCoordinate(srcX) ||
        !centeredCoordinate(srcY)) return -1;
    memset(outPlan, 0, sizeof(*outPlan));
    sX = srcX >> 6;
    sY = srcY >> 6;
    dX = destX >> 6;
    dY = destY >> 6;
    outPlan->traceMask = mask;
    for (unsigned d = 0; d < 4; ++d) outPlan->navScore[d] = -1;
    if (!tileIndexFor(srcX, srcY, &outPlan->sourceTile)) return -1;

    clear = cardinalTraceClear(srcX, srcY, srcX + THREEGOAL_TILE_SIZE, srcY,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX + 1, sY, dX, dY);
        if (path.status < 0) return path.status;
        outPlan->navScore[0] = path.distance;
        outPlan->navTraceBits[0] = path.navTraceBits;
        if (path.distance < 9999) {
            closestPathDist = path.distance;
            visitCount = 1U;
            visitOrder[0] = 2U;
        }
        else if (path.distance == 9999) {
            visitOrder[visitCount++] = 2U;
        }
    }

    clear = cardinalTraceClear(srcX, srcY, srcX - THREEGOAL_TILE_SIZE, srcY,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX - 1, sY, dX, dY);
        if (path.status < 0) return path.status;
        outPlan->navScore[1] = path.distance;
        outPlan->navTraceBits[1] = path.navTraceBits;
        if (path.distance < closestPathDist) {
            closestPathDist = path.distance;
            visitCount = 1U;
            visitOrder[0] = 3U;
        }
        else if (path.distance == closestPathDist) {
            visitOrder[visitCount++] = 3U;
        }
    }

    clear = cardinalTraceClear(srcX, srcY, srcX, srcY + THREEGOAL_TILE_SIZE,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX, sY + 1, dX, dY);
        if (path.status < 0) return path.status;
        outPlan->navScore[2] = path.distance;
        outPlan->navTraceBits[2] = path.navTraceBits;
        if (path.distance < closestPathDist) {
            closestPathDist = path.distance;
            visitCount = 1U;
            visitOrder[0] = 1U;
        }
        else if (path.distance == closestPathDist) {
            visitOrder[visitCount++] = 1U;
        }
    }

    clear = cardinalTraceClear(srcX, srcY, srcX, srcY - THREEGOAL_TILE_SIZE,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX, sY - 1, dX, dY);
        if (path.status < 0) return path.status;
        outPlan->navScore[3] = path.distance;
        outPlan->navTraceBits[3] = path.navTraceBits;
        if (path.distance < closestPathDist) {
            visitCount = 1U;
            visitOrder[0] = 0U;
        }
        else if (path.distance == closestPathDist) {
            visitOrder[visitCount++] = 0U;
        }
    }

    outPlan->visitCount = visitCount;
    if (visitCount == 0U) return 0;
    if (!probeNextByte(rng, &outPlan->tieRand)) return -3;
    outPlan->tieRandUsed = 1U;
    outPlan->choice = visitOrder[(outPlan->tieRand & 3U) % visitCount];
    switch (outPlan->choice) {
    case 0U: outPlan->deltaY = -THREEGOAL_TILE_SIZE; break;
    case 1U: outPlan->deltaY = THREEGOAL_TILE_SIZE; break;
    case 2U: outPlan->deltaX = THREEGOAL_TILE_SIZE; break;
    case 3U: outPlan->deltaX = -THREEGOAL_TILE_SIZE; break;
    default: return -1;
    }
    if (!tileIndexFor(srcX + outPlan->deltaX,
                      srcY + outPlan->deltaY, &outPlan->destTile)) return -1;
    return 1;
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterView* monsters = EspNativeGameplayMonsterState_view();
    if (monsters == NULL || monsters->records == NULL ||
        monsters->sourceArenaFNV1a == 0U) {
        return 0;
    }
    if (threeGoalView.active == 0U ||
        threeGoalView.sourceArenaFNV1a != monsters->sourceArenaFNV1a) {
        memset(&threeGoalView, 0, sizeof(threeGoalView));
#if DOOMRPG_PERF_GAME_P1
        navSampleCount = 0U;
#endif
        threeGoalView.sourceArenaFNV1a = monsters->sourceArenaFNV1a;
        threeGoalView.lastSpriteIndex = 0xffffU;
        threeGoalView.lastTile = 0xffffU;
        threeGoalView.active = 1U;
        printf("[MONSTER3GOAL] READY arena=%08x ownerBytes=%u subtypes=4/13 goalCount=3 continuationTarget=player-dest continuationRng=one-tie-byte-per-successful-goal movementPublish=existing-transaction interpolation=deferred multiLoopAttack=turn-probe-live/simultaneous-fail-closed\n",
               (unsigned int)threeGoalView.sourceArenaFNV1a,
               (unsigned int)sizeof(threeGoalView));
    }
    return 1;
}

void EspNativeGameplayMonsterThreeGoalTurn_reset(void) {
    memset(&threeGoalView, 0, sizeof(threeGoalView));
#if DOOMRPG_PERF_GAME_P1
    navSampleCount = 0U;
#endif
    memset(&syntheticMovementView, 0, sizeof(syntheticMovementView));
}

const EspNativeGameplayMonsterThreeGoalTurnView*
EspNativeGameplayMonsterThreeGoalTurn_view(void) {
    return syncOwner() ? &threeGoalView : NULL;
}

static int exactCommittedPosition(uint16_t spriteIndex,
                                  const EspPlayerViewState** outPlayer,
                                  const EspNativeGameplayMonsterPositionRecord** outPosition,
                                  const EspNativeGameplayMonsterRecord** outMonster) {
    const EspPlayerViewState* player = EspPlayerView_view();
    const EspNativeGameplayMonsterPositionRecord* position =
        EspNativeGameplayMonsterPosition_find(spriteIndex);
    const EspNativeGameplayMonsterRecord* monster =
        EspNativeGameplayMonsterState_find(spriteIndex);
    uint8_t type;
    uint8_t subtype;
    uint16_t linkState;
    uint16_t linkOrder;

    if (outPlayer != NULL) *outPlayer = NULL;
    if (outPosition != NULL) *outPosition = NULL;
    if (outMonster != NULL) *outMonster = NULL;
    if (player == NULL || position == NULL || monster == NULL ||
        player->active != 1U || player->viewX != player->destX ||
        player->viewY != player->destY || player->viewAngle != player->destAngle ||
        monster->alive == 0U ||
        (monster->subtype != 4U && monster->subtype != 13U) ||
        !EspNativeGameplayMonsterActivation_isActive(spriteIndex) ||
        !EspMapSpriteTopology_getEntity(spriteIndex, &type, &subtype,
                                        &linkState, &linkOrder) ||
        type != THREEGOAL_TYPE_ENEMY || subtype != monster->subtype ||
        (linkState & (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                      ESP_MAP_SPRITE_TOPOLOGY_ALIVE)) !=
            (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
             ESP_MAP_SPRITE_TOPOLOGY_ALIVE) ||
        (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != position->tileIndex ||
        linkOrder == 0U) {
        return 0;
    }
    if (outPlayer != NULL) *outPlayer = player;
    if (outPosition != NULL) *outPosition = position;
    if (outMonster != NULL) *outMonster = monster;
    return 1;
}

/* Return 1 planned, 0 exact legacy no-move, -1 deliberate defer. The live RNG
 * remains untouched; the existing publisher replays the one tie byte only after
 * every mutable owner and render preflight succeeds. */
static int probeContinuationGoal(DoomRPG_t* doomRpg,
                                 uint16_t spriteIndex,
                                 uint8_t goalStep,
                                 uint32_t plannedMovesBefore) {
    const EspPlayerViewState* player;
    const EspNativeGameplayMonsterPositionRecord* position;
    const EspNativeGameplayMonsterRecord* monster;
    const EspNativeGameplayMonsterMovementView* realMovement;
    EspNativeGameplayMonsterPositionRecord positionBefore;
    EspNativeGameplayMonsterPositionRecord positionAfter;
    ThreeGoalProbeRng probeRng;
    ThreeGoalMovePlan plan;
    Random_t randomBefore;
    uint32_t positionFNVBefore;
    uint32_t positionFNVCommitted;
    uint32_t positionFNVAfter;
    int planStatus;
    int positionExact;
    int randomExact;

    if (doomRpg == NULL || goalStep < 2U || goalStep > THREEGOAL_TOTAL_GOALS ||
        !syncOwner() ||
        !exactCommittedPosition(spriteIndex, &player, &position, &monster)) {
        printf("[MONSTER3GOAL] DEFER sprite=%u goal=%u/3 cause=continuation-state-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep);
        return -1;
    }

    realMovement = EspNativeGameplayMonsterMovement_view();
    if (realMovement == NULL || realMovement->active != 1U ||
        realMovement->sourceArenaFNV1a != threeGoalView.sourceArenaFNV1a ||
        realMovement->plannedMoves != plannedMovesBefore) {
        printf("[MONSTER3GOAL] DEFER sprite=%u goal=%u/3 cause=movement-view-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep);
        return -1;
    }

    randomBefore = (*EspNativeProcessRng_live());
    memset(&probeRng, 0, sizeof(probeRng));
    probeRng.state = randomBefore;
    memset(&plan, 0, sizeof(plan));
#if DOOMRPG_PERF_GAME_P1
    EspPerfTraceCache_begin(&traceCache);
    EspPerfGameP1_phaseBegin(7, (uint64_t)esp_timer_get_time());
    (void)EspPerfSpatialSprite_begin(&EspPerfGameP1_spatialSpriteIndex);
    EspPerfGameP1_lineBuild(EspPerfSpatialLine_begin(&EspPerfGameP1_spatialLineIndex, lineEntityTile));
#endif
    originPlaneClear = originPlaneBlocked = 0U;
    planStatus = aiGoalPlan(spriteIndex, monster->subtype,
                            (int32_t)position->worldX,
                            (int32_t)position->worldY,
                            player->destX, player->destY,
                            &probeRng, &plan);
#if DOOMRPG_PERF_GAME_P1
    EspPerfGameP1_phaseEnd(7, (uint64_t)esp_timer_get_time());
    EspPerfSpatialLine_end(&EspPerfGameP1_spatialLineIndex);
    EspPerfSpatialSprite_end(&EspPerfGameP1_spatialSpriteIndex);
    EspPerfTraceCache_end(&traceCache);
    /* Print outside navigation phase timing; bounded successful-plan witness. */
    if (planStatus == 1 && (monster->subtype == 4U || monster->subtype == 13U) && navSampleCount < 48U) {
        ++navSampleCount;
        printf("[AI14NAV] phase=continuation sprite=%u sub=%u src=%d,%d target=%d,%d dst=%u mask=%04x score=%d,%d,%d,%d bits=%02x,%02x,%02x,%02x ties=%u byte=%u choice=%u goal=%u player=%d,%d angle=%d rngBefore=%d rngAfter=%d calls=%u sample=%u\n",
               (unsigned)spriteIndex, (unsigned)monster->subtype, (int)position->worldX, (int)position->worldY,
               (int)player->destX, (int)player->destY, (unsigned)plan.destTile, (unsigned)plan.traceMask,
               plan.navScore[0], plan.navScore[1], plan.navScore[2], plan.navScore[3],
               (unsigned)plan.navTraceBits[0], (unsigned)plan.navTraceBits[1],
               (unsigned)plan.navTraceBits[2], (unsigned)plan.navTraceBits[3],
               (unsigned)plan.visitCount, (unsigned)plan.tieRand, (unsigned)plan.choice,
               (unsigned)goalStep, (int)player->destX, (int)player->destY, (int)player->destAngle,
               (int)randomBefore.nextRand, (int)probeRng.state.nextRand, (unsigned)probeRng.calls, (unsigned)navSampleCount);
    }
#endif
    if (originPlaneClear != 0U || originPlaneBlocked != 0U) {
        printf("[AI14PLANE] planner=continuation sprite=%u goal=%u originClear=%u originBlocked=%u planStatus=%d\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep,
               (unsigned int)originPlaneClear, (unsigned int)originPlaneBlocked, planStatus);
    }
    if (planStatus == -3 || probeRng.boundary != 0U) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=rng-boundary-unprepared mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep);
        return -1;
    }
    if (planStatus == -2) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=calcPath-special-plane-corpus-needed mask=%04x mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep,
               (unsigned int)movementMaskForSubtype(monster->subtype));
        return -1;
    }
    if (planStatus < 0) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=planner-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep);
        return -1;
    }
    if (planStatus == 0) {
        printf("[MONSTER3GOAL] NO-MOVE sprite=%u subtype=%u frameTime=%u goal=%u/3 sourceTile=%u target=%d,%d mask=%04x visitCount=0 rngCalls=0 chain=end mutation=no\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)(goalStep - 1U), (unsigned int)goalStep,
               (unsigned int)position->tileIndex,
               (int)player->destX, (int)player->destY,
               (unsigned int)movementMaskForSubtype(monster->subtype));
        return 0;
    }
    if (probeRng.calls != 1U || plan.tieRandUsed == 0U) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=continuation-rng-contract calls=%u mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep, (unsigned int)probeRng.calls);
        return -1;
    }

    positionFNVBefore = EspNativeGameplayMonsterPosition_fingerprint();
    if (!EspNativeGameplayMonsterMovementActivation_prepareCardinalMove(
            spriteIndex, plan.deltaX, plan.deltaY,
            &positionBefore, &positionAfter) ||
        positionBefore.tileIndex != plan.sourceTile ||
        positionAfter.tileIndex != plan.destTile ||
        !EspNativeGameplayMonsterPosition_commitPrepared(&positionBefore,
                                                         &positionAfter)) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=position-probe-prepare-or-commit mutation=no-published rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep);
        return -1;
    }
    positionFNVCommitted = EspNativeGameplayMonsterPosition_fingerprint();
    if (!EspNativeGameplayMonsterPosition_rollbackPrepared(&positionAfter,
                                                           &positionBefore)) {
        printf("[MONSTER3GOAL] FATAL-PROBE sprite=%u subtype=%u goal=%u/3 cause=position-rollback-failed liveMove=no\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep);
        return -1;
    }
    positionFNVAfter = EspNativeGameplayMonsterPosition_fingerprint();
    position = EspNativeGameplayMonsterPosition_find(spriteIndex);
    positionExact = position != NULL &&
                    memcmp(position, &positionBefore, sizeof(positionBefore)) == 0 &&
                    positionFNVAfter == positionFNVBefore;
    randomExact = memcmp(EspNativeProcessRng_live(), &randomBefore, sizeof(randomBefore)) == 0;
    if (!positionExact || !randomExact) {
        printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u goal=%u/3 cause=probe-rollback-not-exact positionExact=%s randomExact=%s mutation=no\n",
               (unsigned int)spriteIndex, (unsigned int)monster->subtype,
               (unsigned int)goalStep,
               positionExact ? "yes" : "NO",
               randomExact ? "yes" : "NO");
        return -1;
    }

    syntheticMovementView = *realMovement;
    syntheticMovementView.plannedMoves = plannedMovesBefore + 1U;
    syntheticMovementView.rollbackMoves = realMovement->rollbackMoves + 1U;
    syntheticMovementView.lastSpriteIndex = spriteIndex;
    syntheticMovementView.lastSourceTile = plan.sourceTile;
    syntheticMovementView.lastDestTile = plan.destTile;
    syntheticMovementView.lastPositionFNV1a = positionFNVAfter;

    ++threeGoalView.continuationPlans;
    threeGoalView.lastSpriteIndex = spriteIndex;
    threeGoalView.lastTile = plan.destTile;
    threeGoalView.lastGoalStep = goalStep;
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    printf("[MONSTER3GOAL] PLAN sprite=%u subtype=%u frameTime=%u goal=%u/3 sourceTile=%u source=%u,%u target=%d,%d destTile=%u delta=%d,%d visitCount=%u tieRand=%u choice=%u mask=%04x rngCalls=1 randomLiveUntouched=yes positionFNV=%08x->%08x->%08x positionRollback=yes publish=pending\n",
           (unsigned int)spriteIndex, (unsigned int)monster->subtype,
           (unsigned int)(goalStep - 1U), (unsigned int)goalStep,
           (unsigned int)plan.sourceTile,
           (unsigned int)positionBefore.worldX,
           (unsigned int)positionBefore.worldY,
           (int)player->destX, (int)player->destY,
           (unsigned int)plan.destTile,
           (int)plan.deltaX, (int)plan.deltaY,
           (unsigned int)plan.visitCount,
           (unsigned int)plan.tieRand,
           (unsigned int)plan.choice,
           (unsigned int)plan.traceMask,
           (unsigned int)positionFNVBefore,
           (unsigned int)positionFNVCommitted,
           (unsigned int)positionFNVAfter);
#else
    /* P3i: successful probe audit remains countable at INFO; detailed path
     * coordinates and tie choice remain available in the TRACE profile. */
    printf("[MONSTER3GOAL] PLAN sprite=%u goal=%u/3 tile=%u->%u rngCalls=1 positionFNV=%08x->%08x->%08x positionRollback=yes\n",
           (unsigned int)spriteIndex, (unsigned int)goalStep,
           (unsigned int)plan.sourceTile, (unsigned int)plan.destTile,
           (unsigned int)positionFNVBefore,
           (unsigned int)positionFNVCommitted,
           (unsigned int)positionFNVAfter);
#endif
    return 1;
}

static int atByteBoundary(const Random_t* random) {
    return random != NULL &&
           (random->nextRand + (int)sizeof(byte)) >= RANDTABLESIZE;
}

/* Return 1 committed, 0 exact no-move, -1 defer. Publication reuses the
 * hardware-proven live move transaction. For subtype4/13 rangeMin==0, the
 * existing NO-IMMEDIATE-ATTACK publication replay is exactly one tie byte. */
static int publishContinuationGoal(DoomRPG_t* doomRpg,
                                   uint16_t spriteIndex,
                                   uint8_t goalStep,
                                   EspNativeGameplayMonsterMovementPublishResult* outPublish) {
    const EspNativeGameplayMonsterMovementView* realMovement;
    uint32_t plannedBefore;
    int planStatus;
    int publishOk;

    if (outPublish != NULL) memset(outPublish, 0, sizeof(*outPublish));
    if (doomRpg == NULL || outPublish == NULL) return -1;
    realMovement = EspNativeGameplayMonsterMovement_view();
    if (realMovement == NULL || realMovement->active != 1U) return -1;
    plannedBefore = realMovement->plannedMoves;
    EspNativeGameplayMonsterMovementPublish_beginCycle();

    if (atByteBoundary(EspNativeProcessRng_live())) {
        Random_t saved;
        uint8_t prepared = 0U;
        int restoredExact = 0;

        if (!EspNativeRngReplayGuard_beginProbeBoundary(EspNativeProcessRng_live(),
                                                        &saved, &prepared)) {
            printf("[MONSTER3GOALRNG] DEFER sprite=%u goal=%u/3 cause=rng-reservation-conflict mutation=no\n",
                   (unsigned int)spriteIndex, (unsigned int)goalStep);
            return -1;
        }
        printf("[MONSTER3GOALRNG] ARM sprite=%u goal=%u/3 next=127->0 prepared=%u liveRandom=temporary-post-refill reservation=persistent\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep,
               (unsigned int)prepared);
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseBegin(4, (uint64_t)esp_timer_get_time());
#endif
        planStatus = probeContinuationGoal(doomRpg, spriteIndex, goalStep,
                                           plannedBefore);
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseEnd(4, (uint64_t)esp_timer_get_time());
#endif
        if (planStatus == 1) {
#if DOOMRPG_PERF_GAME_P1
            EspPerfGameP1_phaseBegin(5, (uint64_t)esp_timer_get_time());
#endif
            publishOk = EspNativeGameplayMonsterMovementPublish_afterProbeWithView(
                doomRpg, "NO-IMMEDIATE-ATTACK", &saved, prepared,
                plannedBefore, &syntheticMovementView, outPublish);
#if DOOMRPG_PERF_GAME_P1
            EspPerfGameP1_phaseEnd(5, (uint64_t)esp_timer_get_time());
#endif
        }
        else {
            publishOk = 0;
        }

        if (prepared != 0U && outPublish->boundaryClosed == 0U) {
            restoredExact = EspNativeRngReplayGuard_endProbeBoundary(
                EspNativeProcessRng_live(), &saved, prepared);
            printf("[MONSTER3GOALRNG] RESTORE sprite=%u goal=%u/3 randomLiveExact=%s reservation=pending-until-real-byte-draw\n",
                   (unsigned int)spriteIndex, (unsigned int)goalStep,
                   restoredExact ? "yes" : "NO");
        }
        else if (outPublish->committed != 0U) {
            printf("[MONSTER3GOALRNG] COMMIT sprite=%u goal=%u/3 rngCalls=%u reservation=consumed-by-live-move randomLive=advanced-exactly\n",
                   (unsigned int)spriteIndex, (unsigned int)goalStep,
                   (unsigned int)outPublish->rngCalls);
        }
        else if (outPublish->boundaryClosed != 0U) {
            printf("[MONSTER3GOALRNG] ROLLBACK sprite=%u goal=%u/3 reservation=downgraded-to-replay-lease randomLive=restored-pre-refill\n",
                   (unsigned int)spriteIndex, (unsigned int)goalStep);
        }

        if (planStatus <= 0) return planStatus;
        if (!publishOk || outPublish->committed == 0U) return -1;
    }
    else {
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseBegin(4, (uint64_t)esp_timer_get_time());
#endif
        planStatus = probeContinuationGoal(doomRpg, spriteIndex, goalStep,
                                           plannedBefore);
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseEnd(4, (uint64_t)esp_timer_get_time());
#endif
        if (planStatus <= 0) return planStatus;
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseBegin(5, (uint64_t)esp_timer_get_time());
#endif
        publishOk = EspNativeGameplayMonsterMovementPublish_afterProbeWithView(
            doomRpg, "NO-IMMEDIATE-ATTACK", NULL, 0U,
            plannedBefore, &syntheticMovementView, outPublish);
#if DOOMRPG_PERF_GAME_P1
        EspPerfGameP1_phaseEnd(5, (uint64_t)esp_timer_get_time());
#endif
        if (!publishOk || outPublish->committed == 0U) return -1;
    }

    ++threeGoalView.continuationCommits;
    threeGoalView.lastTile = outPublish->destTile;
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    printf("[MONSTER3GOAL] COMMIT sprite=%u goal=%u/3 tile=%u->%u rngCalls=%u sameTurn=yes topologyRelink=committed renderer=snap-destination interpolation=deferred rollback=closed\n",
           (unsigned int)spriteIndex, (unsigned int)goalStep,
           (unsigned int)outPublish->sourceTile,
           (unsigned int)outPublish->destTile,
           (unsigned int)outPublish->rngCalls);
#else
    printf("[MONSTER3GOAL] COMMIT sprite=%u goal=%u/3 tile=%u->%u rngCalls=%u rollback=closed\n",
           (unsigned int)spriteIndex, (unsigned int)goalStep,
           (unsigned int)outPublish->sourceTile,
           (unsigned int)outPublish->destTile,
           (unsigned int)outPublish->rngCalls);
#endif
    return 1;
}

static int adjacentLegacyShortcut(const EspPlayerViewState* player,
                                  const EspNativeGameplayMonsterPositionRecord* position,
                                  uint32_t* outDistance2) {
    int64_t dx;
    int64_t dy;
    uint32_t distance2;
    if (outDistance2 != NULL) *outDistance2 = 0U;
    if (player == NULL || position == NULL) return 0;
    dx = (int64_t)position->worldX - player->destX;
    dy = (int64_t)position->worldY - player->destY;
    distance2 = (uint32_t)(dx * dx + dy * dy);
    if (outDistance2 != NULL) *outDistance2 = distance2;
    return (dx == 0 || dy == 0) &&
           distance2 <= (uint32_t)(THREEGOAL_TILE_SIZE * THREEGOAL_TILE_SIZE);
}

int EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal(
    struct DoomRPG_s* doomRpgBase,
    uint16_t spriteIndex,
    uint16_t sourceTile,
    uint16_t destTile) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterRecord* monster =
        EspNativeGameplayMonsterState_find(spriteIndex);
    const EspPlayerViewState* player;
    const EspNativeGameplayMonsterPositionRecord* position;
    const EspNativeGameplayMonsterRecord* exactMonster;
    EspNativeGameplayMonsterMovementPublishResult publish;
    uint16_t currentSource = sourceTile;
    uint16_t currentDest = destTile;
    uint8_t goalStep = 1U;

    /*
     * Explicit special-family entry. Ordinary-vs-three-goal routing is owned
     * by MonsterTurn; direct misuse remains fail-closed.
     */
    if (monster == NULL || (monster->subtype != 4U && monster->subtype != 13U)) {
        return 0;
    }

    if (doomRpg == NULL || !syncOwner()) {
        printf("[MONSTER3GOAL] DEFER sprite=%u tile=%u->%u cause=owner-not-ready mutation=no\n",
               (unsigned int)spriteIndex,
               (unsigned int)sourceTile,
               (unsigned int)destTile);
        return 0;
    }

    ++threeGoalView.observedChains;
    threeGoalView.lastSpriteIndex = spriteIndex;
#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_TRACE
    printf("[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u\n",
           (unsigned int)threeGoalView.observedChains,
           (unsigned int)spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)sourceTile,
           (unsigned int)destTile);
#else
    printf("[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u legacyGoalCount=3 frameTime=0 firstGoalAlreadyCommitted=yes continuationGoals=bounded-2 attackLoops=3-native-probe/simultaneous-fail-closed\n",
           (unsigned int)threeGoalView.observedChains,
           (unsigned int)spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)sourceTile,
           (unsigned int)destTile);
#endif

    for (;;) {
        uint32_t distance2;
        int adjacent;
        int continuationStatus;

        if (!exactCommittedPosition(spriteIndex, &player, &position, &exactMonster) ||
            position->tileIndex != currentDest || exactMonster != monster) {
            ++threeGoalView.deferredChains;
            printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u frameTime=%u goal=%u/3 tile=%u cause=committed-position-not-exact mutation=no additionalRng=0\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype,
                   (unsigned int)goalStep,
                   (unsigned int)goalStep,
                   (unsigned int)currentDest);
            return 0;
        }

        threeGoalView.lastGoalStep = goalStep;
        threeGoalView.lastTile = currentDest;
        adjacent = adjacentLegacyShortcut(player, position, &distance2);

        /* Entity_aiMoveToGoal increments frameTime on each completed goal. Before
         * frameTime reaches 3, cardinal <=64 proximity jumps it directly to 3
         * and therefore suppresses all remaining movement goals. */
        if (goalStep < THREEGOAL_TOTAL_GOALS && adjacent) {
            int attackPublished;
            ++threeGoalView.shortcutStops;
            attackPublished = EspNativeGameplayMonsterTurn_publishThreeGoalAttack(
                doomRpgBase, spriteIndex, currentSource, currentDest, goalStep);
            if (!attackPublished) {
                ++threeGoalView.deferredChains;
                printf("[MONSTER3GOAL] ATTACK-DEFER sprite=%u subtype=%u frameTime=%u->3 goal=%u/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=yes remainingGoals=skipped loops=%u cause=turn-probe-not-published priorMovesRemainCommitted=yes sameTurn=closed-prefix mutation=no additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)goalStep,
                       (unsigned int)goalStep,
                       (unsigned int)currentDest,
                       (unsigned int)distance2,
                       (unsigned int)THREEGOAL_MULTI_LOOP_SHOTS);
                return 0;
            }
            printf("[MONSTER3GOAL] ATTACK-PROBE sprite=%u subtype=%u frameTime=%u->3 goal=%u/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=yes remainingGoals=skipped loops=%u producer=MonsterTurn movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype,
                   (unsigned int)goalStep,
                   (unsigned int)goalStep,
                   (unsigned int)currentDest,
                   (unsigned int)distance2,
                   (unsigned int)THREEGOAL_MULTI_LOOP_SHOTS);
            return 1;
        }

        if (goalStep == THREEGOAL_TOTAL_GOALS) {
            if (adjacent) {
                int attackPublished =
                    EspNativeGameplayMonsterTurn_publishThreeGoalAttack(
                        doomRpgBase, spriteIndex, currentSource, currentDest,
                        goalStep);
                if (!attackPublished) {
                    ++threeGoalView.deferredChains;
                    printf("[MONSTER3GOAL] ATTACK-DEFER sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=no loops=%u cause=turn-probe-not-published priorMovesRemainCommitted=yes sameTurn=closed-prefix mutation=no additionalRng=0\n",
                           (unsigned int)spriteIndex,
                           (unsigned int)monster->subtype,
                           (unsigned int)currentDest,
                           (unsigned int)distance2,
                           (unsigned int)THREEGOAL_MULTI_LOOP_SHOTS);
                    return 0;
                }
                printf("[MONSTER3GOAL] ATTACK-PROBE sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=no loops=%u producer=MonsterTurn movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)currentDest,
                       (unsigned int)distance2,
                       (unsigned int)THREEGOAL_MULTI_LOOP_SHOTS);
            }
            else {
#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_TRACE
                printf("[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u goal=3/3 tile=%u distance2=%u sameTurn=yes additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)currentDest,
                       (unsigned int)distance2);
#else
                printf("[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=no attack=no movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)currentDest,
                       (unsigned int)distance2);
#endif
            }
            return 1;
        }

        memset(&publish, 0, sizeof(publish));
        continuationStatus = publishContinuationGoal(
            doomRpg, spriteIndex, (uint8_t)(goalStep + 1U), &publish);
        if (continuationStatus == 0) {
            ++threeGoalView.noMoveStops;
            printf("[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u frameTime=%u goal=%u/3 tile=%u continuation=no-move attack=no movementChain=ended-early sameTurn=yes mutation=no additionalRng=0\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype,
                   (unsigned int)goalStep,
                   (unsigned int)(goalStep + 1U),
                   (unsigned int)currentDest);
            return 1;
        }
        if (continuationStatus < 0 || publish.committed == 0U) {
            ++threeGoalView.deferredChains;
            printf("[MONSTER3GOAL] DEFER sprite=%u subtype=%u frameTime=%u nextGoal=%u/3 tile=%u cause=continuation-not-published priorMovesRemainCommitted=yes sameTurn=closed-prefix mutation=no-new-publish\n",
                   (unsigned int)spriteIndex,
                   (unsigned int)monster->subtype,
                   (unsigned int)goalStep,
                   (unsigned int)(goalStep + 1U),
                   (unsigned int)currentDest);
            return 0;
        }

        currentSource = publish.sourceTile;
        currentDest = publish.destTile;
        ++goalStep;
        (void)currentSource;
    }
}

