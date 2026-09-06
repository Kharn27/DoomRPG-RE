#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_three_goal_turn.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_player_view_state.h"

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
} ThreeGoalMovePlan;

typedef struct ThreeGoalPathResult_s {
    int distance;
    int status;
} ThreeGoalPathResult;

static EspNativeGameplayMonsterThreeGoalTurnView threeGoalView;
static EspNativeGameplayMonsterMovementView syntheticMovementView;
static uint8_t syntheticMovementActive;

const EspNativeGameplayMonsterMovementView*
__real_EspNativeGameplayMonsterMovement_view(void);
int __real_EspNativeGameplayMonsterTurn_postMoveGoal(
    struct DoomRPG_s* doomRpg,
    uint16_t spriteIndex,
    uint16_t sourceTile,
    uint16_t destTile);
void __real_EspNativeGameplayMonsterMovementProbe_reset(void);

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

static int closedLineBlocksTile(uint16_t tile, uint16_t mask) {
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

static int specialEntityBlocks(uint32_t spriteIndex,
                               int32_t sourceX,
                               int32_t sourceY,
                               int32_t destX,
                               int32_t destY) {
    EspMapSprite sprite;
    int32_t sprX;
    int32_t sprY;
    if (!EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return -1;
    if ((sprite.info & THREEGOAL_SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;
    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
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

/* Return 1 blocked, 0 clear, -1 not ready, -2 unsupported legacy special
 * crossing. This is the exact continuation-goal subset of the proven movement
 * planner and deliberately keeps the same fail-closed special-plane boundary. */
static int blockingSpriteOnTile(uint16_t tile,
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
            if (strictSpecial) return -2;
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY);
            if (specialBlocks < 0) return -1;
            if (specialBlocks == 0) continue;
        }
        if (!found || linkOrder > bestOrder) {
            found = 1;
            bestOrder = linkOrder;
        }
    }
    return found;
}

static int cardinalTraceClear(int32_t sourceX,
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

    for (i = 1U; i <= distance; ++i) {
        int32_t x = sourceX + stepX * (int32_t)i;
        int32_t y = sourceY + stepY * (int32_t)i;
        uint16_t tile;
        uint8_t flags;
        int lineBlock;
        int spriteBlock;

        if (!tileIndexFor(x, y, &tile) || !EspMapState_getTileFlags(tile, &flags)) {
            return 0;
        }
        if ((flags & ESP_MAP_TILE_WALL) != 0U) return 0;
        lineBlock = closedLineBlocksTile(tile, mask);
        if (lineBlock < 0) return -1;
        if (lineBlock > 0) return 0;
        spriteBlock = blockingSpriteOnTile(tile,
                                           x - stepX, y - stepY, x, y,
                                           attackerSprite, mask, strictSpecial);
        if (spriteBlock < 0) return spriteBlock;
        if (spriteBlock > 0) return 0;
    }
    return 1;
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
    if (!tileIndexFor(srcX, srcY, &outPlan->sourceTile)) return -1;

    clear = cardinalTraceClear(srcX, srcY, srcX + THREEGOAL_TILE_SIZE, srcY,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX + 1, sY, dX, dY);
        if (path.status < 0) return path.status;
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
        threeGoalView.sourceArenaFNV1a = monsters->sourceArenaFNV1a;
        threeGoalView.lastSpriteIndex = 0xffffU;
        threeGoalView.lastTile = 0xffffU;
        threeGoalView.active = 1U;
        printf("[MONSTER3GOAL] READY arena=%08x ownerBytes=%u subtypes=4/13 goalCount=3 continuationTarget=player-dest continuationRng=one-tie-byte-per-successful-goal movementPublish=existing-transaction interpolation=deferred multiLoopAttack=fail-closed\n",
               (unsigned int)threeGoalView.sourceArenaFNV1a,
               (unsigned int)sizeof(threeGoalView));
    }
    return 1;
}

void EspNativeGameplayMonsterThreeGoalTurn_reset(void) {
    memset(&threeGoalView, 0, sizeof(threeGoalView));
    memset(&syntheticMovementView, 0, sizeof(syntheticMovementView));
    syntheticMovementActive = 0U;
}

const EspNativeGameplayMonsterThreeGoalTurnView*
EspNativeGameplayMonsterThreeGoalTurn_view(void) {
    return syncOwner() ? &threeGoalView : NULL;
}

/* The existing live publisher requires the proven movement planner view to
 * advance by exactly one probe. Continuation planning is a bounded extension of
 * that same planner contract; expose a synthetic one-call delta only while the
 * existing publisher validates/commits this captured continuation step. */
const EspNativeGameplayMonsterMovementView*
__wrap_EspNativeGameplayMonsterMovement_view(void) {
    if (syntheticMovementActive != 0U) return &syntheticMovementView;
    return __real_EspNativeGameplayMonsterMovement_view();
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

    realMovement = __real_EspNativeGameplayMonsterMovement_view();
    if (realMovement == NULL || realMovement->active != 1U ||
        realMovement->sourceArenaFNV1a != threeGoalView.sourceArenaFNV1a ||
        realMovement->plannedMoves != plannedMovesBefore) {
        printf("[MONSTER3GOAL] DEFER sprite=%u goal=%u/3 cause=movement-view-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep);
        return -1;
    }

    randomBefore = doomRpg->random;
    memset(&probeRng, 0, sizeof(probeRng));
    probeRng.state = randomBefore;
    memset(&plan, 0, sizeof(plan));
    planStatus = aiGoalPlan(spriteIndex, monster->subtype,
                            (int32_t)position->worldX,
                            (int32_t)position->worldY,
                            player->destX, player->destY,
                            &probeRng, &plan);
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
    if (!EspNativeGameplayMonsterPosition_prepareCardinalMove(
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
    randomExact = memcmp(&doomRpg->random, &randomBefore, sizeof(randomBefore)) == 0;
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
    realMovement = __real_EspNativeGameplayMonsterMovement_view();
    if (realMovement == NULL || realMovement->active != 1U) return -1;
    plannedBefore = realMovement->plannedMoves;
    EspNativeGameplayMonsterMovementPublish_beginCycle();

    if (atByteBoundary(&doomRpg->random)) {
        Random_t saved;
        uint8_t prepared = 0U;
        int restoredExact = 0;

        if (!EspNativeRngReplayGuard_beginProbeBoundary(&doomRpg->random,
                                                        &saved, &prepared)) {
            printf("[MONSTER3GOALRNG] DEFER sprite=%u goal=%u/3 cause=rng-reservation-conflict mutation=no\n",
                   (unsigned int)spriteIndex, (unsigned int)goalStep);
            return -1;
        }
        printf("[MONSTER3GOALRNG] ARM sprite=%u goal=%u/3 next=127->0 prepared=%u liveRandom=temporary-post-refill reservation=persistent\n",
               (unsigned int)spriteIndex, (unsigned int)goalStep,
               (unsigned int)prepared);
        planStatus = probeContinuationGoal(doomRpg, spriteIndex, goalStep,
                                           plannedBefore);
        if (planStatus == 1) {
            syntheticMovementActive = 1U;
            publishOk = EspNativeGameplayMonsterMovementPublish_afterProbe(
                doomRpg, "NO-IMMEDIATE-ATTACK", &saved, prepared,
                plannedBefore, outPublish);
            syntheticMovementActive = 0U;
        }
        else {
            publishOk = 0;
        }

        if (prepared != 0U && outPublish->boundaryClosed == 0U) {
            restoredExact = EspNativeRngReplayGuard_endProbeBoundary(
                &doomRpg->random, &saved, prepared);
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
        planStatus = probeContinuationGoal(doomRpg, spriteIndex, goalStep,
                                           plannedBefore);
        if (planStatus <= 0) return planStatus;
        syntheticMovementActive = 1U;
        publishOk = EspNativeGameplayMonsterMovementPublish_afterProbe(
            doomRpg, "NO-IMMEDIATE-ATTACK", NULL, 0U,
            plannedBefore, outPublish);
        syntheticMovementActive = 0U;
        if (!publishOk || outPublish->committed == 0U) return -1;
    }

    ++threeGoalView.continuationCommits;
    threeGoalView.lastTile = outPublish->destTile;
    printf("[MONSTER3GOAL] COMMIT sprite=%u goal=%u/3 tile=%u->%u rngCalls=%u sameTurn=yes topologyRelink=committed renderer=snap-destination interpolation=deferred rollback=closed\n",
           (unsigned int)spriteIndex, (unsigned int)goalStep,
           (unsigned int)outPublish->sourceTile,
           (unsigned int)outPublish->destTile,
           (unsigned int)outPublish->rngCalls);
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

int __wrap_EspNativeGameplayMonsterTurn_postMoveGoal(
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

    if (monster == NULL || (monster->subtype != 4U && monster->subtype != 13U)) {
        return __real_EspNativeGameplayMonsterTurn_postMoveGoal(
            doomRpgBase, spriteIndex, sourceTile, destTile);
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
    printf("[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u legacyGoalCount=3 frameTime=0 firstGoalAlreadyCommitted=yes continuationGoals=bounded-2 attackLoops=3-fail-closed\n",
           (unsigned int)threeGoalView.observedChains,
           (unsigned int)spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)sourceTile,
           (unsigned int)destTile);

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
            ++threeGoalView.shortcutStops;
            printf("[MONSTER3GOAL] ATTACK-GATE-DEFER sprite=%u subtype=%u frameTime=%u->3 goal=%u/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=yes remainingGoals=skipped loops=%u cause=multi-loop-attack-family-deferred movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
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
                printf("[MONSTER3GOAL] ATTACK-GATE-DEFER sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=yes shortcut=no loops=%u cause=multi-loop-attack-family-deferred movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)currentDest,
                       (unsigned int)distance2,
                       (unsigned int)THREEGOAL_MULTI_LOOP_SHOTS);
            }
            else {
                printf("[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=no attack=no movementChain=complete sameTurn=yes mutation=no additionalRng=0\n",
                       (unsigned int)spriteIndex,
                       (unsigned int)monster->subtype,
                       (unsigned int)currentDest,
                       (unsigned int)distance2);
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

void __wrap_EspNativeGameplayMonsterMovementProbe_reset(void) {
    EspNativeGameplayMonsterThreeGoalTurn_reset();
    __real_EspNativeGameplayMonsterMovementProbe_reset();
}
