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
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_player_view_state.h"

#define MOVE_MAP_WIDTH 32U
#define MOVE_TILE_SIZE 64
#define MOVE_TILE_CENTER 32
#define MOVE_TRACE_ATTACK_MASK 0x5687U
#define MOVE_TRACE_LEGACY_MASK 0xff87U
#define MOVE_SUBTYPE_SPECIAL_AI 10U
#define MOVE_TYPE_ENEMY 1U
#define MOVE_TYPE_SPECIAL_TRACE 14U
#define MOVE_TYPE_SPECIAL_TRACE_2 15U
#define MOVE_SPECIAL_TRACE_ENTITY_FLAG 0x00020000UL
#define MOVE_SPECIAL_TRACE_Y_MASK 0x00180000UL
#define MOVE_SPECIAL_TRACE_X_MASK 0x00600000UL
#define MOVE_LINE_ENTITY_DEF_BASE 305U
#define MOVE_LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL
#define MOVE_LINE_GEOMETRY_AXIS_X 0x00000008UL
#define MOVE_LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define MOVE_LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define MOVE_LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define MOVE_LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define MOVE_LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define MOVE_LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define MOVE_LINE_ENTITY_NUDGE_X_NEG 0x00004000UL
#define MOVE_WEAPON_COUNT 19U
#define MOVE_NO_SPRITE 0xffffU

/* Exact CombatEntity.c subtype -> primary/alternate weapon table. */
static const uint8_t monsterAttacks[28] = {
    2U, 3U, 12U, 13U, 4U, 4U, 15U, 12U, 13U, 14U, 13U, 12U, 15U, 13U,
    15U, 14U, 7U, 12U, 7U, 3U, 15U, 15U, 16U, 17U, 7U, 17U, 12U, 13U
};

typedef struct MovementWeaponSpec_s {
    uint8_t rangeMin;
    uint8_t valid;
} MovementWeaponSpec;

static const MovementWeaponSpec movementWeapons[MOVE_WEAPON_COUNT] = {
    {0U, 0U}, {0U, 0U}, {5U, 1U}, {2U, 1U}, {3U, 1U},
    {0U, 0U}, {0U, 0U}, {8U, 1U}, {0U, 0U}, {0U, 0U},
    {0U, 0U}, {0U, 0U}, {0U, 1U}, {0U, 1U}, {0U, 1U},
    {3U, 1U}, {3U, 1U}, {2U, 1U}, {0U, 0U}
};

typedef struct MovementCandidate_s {
    const EspNativeGameplayMonsterRecord* monster;
    const EspNativeGameplayMonsterPositionRecord* position;
    uint32_t worldDistance;
    uint16_t tileIndex;
    uint8_t weaponId;
} MovementCandidate;

typedef struct ProbeRng_s {
    Random_t state;
    uint32_t calls;
    uint8_t boundary;
    uint8_t reserved[3];
} ProbeRng;

typedef struct MovePlan_s {
    int32_t deltaX;
    int32_t deltaY;
    uint16_t sourceTile;
    uint16_t destTile;
    uint16_t traceMask;
    uint8_t visitCount;
    uint8_t choice;
    uint8_t tieRand;
    uint8_t tieRandUsed;
} MovePlan;

typedef struct PathResult_s {
    int distance;
    int status;
} PathResult;

static EspNativeGameplayMonsterMovementView movementView;

static int centeredCoordinate(int32_t value) {
    return value >= MOVE_TILE_CENTER &&
           value <= (int32_t)(((MOVE_MAP_WIDTH - 1U) * MOVE_TILE_SIZE) +
                              MOVE_TILE_CENTER) &&
           (value & (MOVE_TILE_SIZE - 1)) == MOVE_TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MOVE_MAP_WIDTH || tileY >= MOVE_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * MOVE_MAP_WIDTH + tileX);
    return 1;
}

static int tileCenterForCoord(int tileX, int tileY, int32_t* outX, int32_t* outY) {
    if (outX == NULL || outY == NULL || tileX < 0 || tileY < 0 ||
        tileX >= (int)MOVE_MAP_WIDTH || tileY >= (int)MOVE_MAP_WIDTH) {
        return 0;
    }
    *outX = tileX * MOVE_TILE_SIZE + MOVE_TILE_CENTER;
    *outY = tileY * MOVE_TILE_SIZE + MOVE_TILE_CENTER;
    return 1;
}

static int typeInMask(uint8_t type, uint16_t mask) {
    return type < 16U && (mask & (uint16_t)(1U << type)) != 0U;
}

static uint16_t movementMaskForSubtype(uint8_t subtype) {
    uint16_t mask = MOVE_TRACE_LEGACY_MASK;
    if (subtype == 4U || subtype == 13U) mask &= (uint16_t)~0x0c00U;
    else if (subtype == 6U || subtype == 7U) mask &= (uint16_t)~0x0800U;
    else if (subtype == 10U) mask &= (uint16_t)~0x0400U;
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

    if ((line->flags & MOVE_LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & MOVE_LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & MOVE_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & MOVE_LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & MOVE_LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & MOVE_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & MOVE_LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & MOVE_LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & MOVE_LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & MOVE_LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    if (x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MOVE_MAP_WIDTH || tileY >= MOVE_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * MOVE_MAP_WIDTH + tileX);
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
        lookup = MOVE_LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        (void)subtype;
        if (!hasDefinition) {
            if ((line.flags & MOVE_LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
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
    if ((sprite.info & MOVE_SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;
    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
    if ((sprite.info & MOVE_SPECIAL_TRACE_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite.info & MOVE_SPECIAL_TRACE_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
}

/* Return 1 blocked, 0 clear, -1 not ready, -2 deliberately unsupported special
 * crossing during calcPath's tile-origin legacy subpath. */
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
        if (type == MOVE_TYPE_SPECIAL_TRACE || type == MOVE_TYPE_SPECIAL_TRACE_2) {
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

/* Mirrors the already hardware-proven native cardinal LOS used by monster turn.
 * strictSpecial is used only by the recovered calcPath look-ahead because that
 * legacy helper traces from tile origins while the compact runtime owns tile
 * centers; rather than guess thin-plane crossing, fail closed for that rare
 * path cell until a dedicated special-plane movement corpus exists. */
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
        stepY = destY > sourceY ? MOVE_TILE_SIZE : -MOVE_TILE_SIZE;
        distance = (uint32_t)((destY > sourceY ? destY - sourceY : sourceY - destY) /
                              MOVE_TILE_SIZE);
    }
    else if (sourceY == destY && sourceX != destX) {
        stepX = destX > sourceX ? MOVE_TILE_SIZE : -MOVE_TILE_SIZE;
        distance = (uint32_t)((destX > sourceX ? destX - sourceX : sourceX - destX) /
                              MOVE_TILE_SIZE);
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

static PathResult calcPath(uint16_t attackerSprite,
                           uint16_t mask,
                           int srcX,
                           int srcY,
                           int destX,
                           int destY) {
    int visitDist[4] = {0, 0, 0, 0};
    int i;
    PathResult result;

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

static int probeNextByte(ProbeRng* rng, uint8_t* outValue) {
    int next;
    if (rng == NULL || outValue == NULL) return 0;
    next = rng->state.nextRand;
    /* DoomRPG_randNextByte() refills when nextRand+1 >= RANDTABLESIZE. A probe
     * has no matching live commit yet, so crossing that boundary would advance
     * hidden resetRand/_seed. Refuse it instead of pretending Random_t is a full
     * snapshot. */
    if (next < 0 || next + (int)sizeof(byte) >= RANDTABLESIZE) {
        rng->boundary = 1U;
        return 0;
    }
    *outValue = rng->state.randTable[next];
    rng->state.nextRand = next + (int)sizeof(byte);
    ++rng->calls;
    return 1;
}

static int aiGoalPlan(uint16_t attackerSprite,
                      uint8_t subtype,
                      int32_t srcX,
                      int32_t srcY,
                      int32_t destX,
                      int32_t destY,
                      ProbeRng* rng,
                      MovePlan* outPlan) {
    int sX;
    int sY;
    int dX;
    int dY;
    int closestPathDist = 9999;
    uint16_t mask = movementMaskForSubtype(subtype);
    uint8_t visitOrder[4];
    uint8_t visitCount = 0U;
    int clear;
    PathResult path;

    if (rng == NULL || outPlan == NULL || !centeredCoordinate(srcX) ||
        !centeredCoordinate(srcY)) return -1;
    memset(outPlan, 0, sizeof(*outPlan));
    sX = srcX >> 6;
    sY = srcY >> 6;
    dX = destX >> 6;
    dY = destY >> 6;
    outPlan->traceMask = mask;
    if (!tileIndexFor(srcX, srcY, &outPlan->sourceTile)) return -1;

    clear = cardinalTraceClear(srcX, srcY, srcX + MOVE_TILE_SIZE, srcY,
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

    clear = cardinalTraceClear(srcX, srcY, srcX - MOVE_TILE_SIZE, srcY,
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

    clear = cardinalTraceClear(srcX, srcY, srcX, srcY + MOVE_TILE_SIZE,
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

    clear = cardinalTraceClear(srcX, srcY, srcX, srcY - MOVE_TILE_SIZE,
                               attackerSprite, mask, 0);
    if (clear < 0) return clear;
    if (clear > 0) {
        path = calcPath(attackerSprite, mask, sX, sY - 1, dX, dY);
        if (path.status < 0) return path.status;
        /* Preserve the recovered legacy quirk: unlike west/south, the north
         * branch does not update closestPathDist when it becomes the new best. */
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
    case 0U: outPlan->deltaY = -MOVE_TILE_SIZE; break;
    case 1U: outPlan->deltaY = MOVE_TILE_SIZE; break;
    case 2U: outPlan->deltaX = MOVE_TILE_SIZE; break;
    case 3U: outPlan->deltaX = -MOVE_TILE_SIZE; break;
    default: return -1;
    }
    if (!tileIndexFor(srcX + outPlan->deltaX,
                      srcY + outPlan->deltaY, &outPlan->destTile)) return -1;
    return 1;
}

static int findCandidate(const EspPlayerViewState* playerView,
                         MovementCandidate* outCandidate,
                         uint32_t* outCandidates) {
    const EspNativeGameplayMonsterView* monsters = EspNativeGameplayMonsterState_view();
    uint32_t candidates = 0U;
    uint32_t i;

    if (outCandidate != NULL) memset(outCandidate, 0, sizeof(*outCandidate));
    if (outCandidates != NULL) *outCandidates = 0U;
    if (playerView == NULL || outCandidate == NULL || outCandidates == NULL ||
        monsters == NULL || monsters->records == NULL ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        !centeredCoordinate(playerView->destX) ||
        !centeredCoordinate(playerView->destY)) {
        return 0;
    }

    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* monster = &monsters->records[i];
        const EspNativeGameplayMonsterPositionRecord* position;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint8_t weaponId;
        int64_t dx;
        int64_t dy;
        uint32_t worldDistance;
        int attackRange;
        int los;

        if (monster->alive == 0U || monster->subtype >= 14U ||
            monster->subtype == MOVE_SUBTYPE_SPECIAL_AI) continue;
        if (!EspMapSpriteTopology_getEntity(monster->spriteIndex,
                                            &type, &subtype,
                                            &linkState, &linkOrder)) return 0;
        (void)linkOrder;
        if (type != MOVE_TYPE_ENEMY || subtype != monster->subtype ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) {
            continue;
        }
        position = EspNativeGameplayMonsterPosition_find(monster->spriteIndex);
        if (position == NULL ||
            position->tileIndex !=
                (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK)) {
            return 0;
        }

        weaponId = monsterAttacks[(uint32_t)monster->subtype * 2U +
                                  (monster->alternateAttack != 0U ? 1U : 0U)];
        if (weaponId >= MOVE_WEAPON_COUNT || movementWeapons[weaponId].valid == 0U) {
            continue;
        }
        dx = (int64_t)position->worldX - playerView->destX;
        dy = (int64_t)position->worldY - playerView->destY;
        if (dx != 0 && dy != 0) continue;
        if (dx == 0 && dy == 0) continue;
        worldDistance = (uint32_t)(dx * dx + dy * dy);
        attackRange = (1 + (int)movementWeapons[weaponId].rangeMin) * MOVE_TILE_SIZE;
        if (worldDistance > (uint32_t)(attackRange * attackRange)) continue;
        los = cardinalTraceClear((int32_t)position->worldX,
                                 (int32_t)position->worldY,
                                 playerView->destX, playerView->destY,
                                 monster->spriteIndex,
                                 MOVE_TRACE_ATTACK_MASK, 0);
        if (los < 0) return 0;
        if (los == 0) continue;

        ++candidates;
        if (candidates == 1U) {
            outCandidate->monster = monster;
            outCandidate->position = position;
            outCandidate->worldDistance = worldDistance;
            outCandidate->tileIndex = position->tileIndex;
            outCandidate->weaponId = weaponId;
        }
    }

    *outCandidates = candidates;
    return 1;
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterTurnView* turn = EspNativeGameplayMonsterTurn_view();
    const EspNativeGameplayMonsterPositionView* positions;

    if (turn == NULL || turn->active != 1U || turn->sourceArenaFNV1a == 0U ||
        !EspNativeGameplayMonsterPosition_ensure()) {
        return 0;
    }
    positions = EspNativeGameplayMonsterPosition_view();
    if (positions == NULL || positions->active != 1U ||
        positions->sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        return 0;
    }

    if (movementView.active == 0U ||
        movementView.sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        memset(&movementView, 0, sizeof(movementView));
        movementView.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        movementView.lastSpriteIndex = MOVE_NO_SPRITE;
        movementView.lastSourceTile = MOVE_NO_SPRITE;
        movementView.lastDestTile = MOVE_NO_SPRITE;
        movementView.lastPositionFNV1a = positions->stateFNV1a;
        movementView.active = 1U;
        printf("[MONSTERMOVE] READY arena=%08x ownerBytes=%u positionRecordBytes=%u mode=planner-probe+position-rollback trigger=MONSTERTURN-MOVE-DEFER aiGoal=legacy-cardinal calcPath=2-step traceMask=%04x subtypeMask=legacy rng=local-copy-boundary-fail-closed rendererPublish=deferred topologyRelink=deferred liveMove=no\n",
               (unsigned int)movementView.sourceArenaFNV1a,
               (unsigned int)positions->ownerBytes,
               (unsigned int)sizeof(EspNativeGameplayMonsterPositionRecord),
               (unsigned int)MOVE_TRACE_LEGACY_MASK);
    }
    return 1;
}

void EspNativeGameplayMonsterMovement_reset(void) {
    memset(&movementView, 0, sizeof(movementView));
    movementView.lastSpriteIndex = MOVE_NO_SPRITE;
    movementView.lastSourceTile = MOVE_NO_SPRITE;
    movementView.lastDestTile = MOVE_NO_SPRITE;
}

const EspNativeGameplayMonsterMovementView* EspNativeGameplayMonsterMovement_view(void) {
    return syncOwner() ? &movementView : NULL;
}

void EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterTurnView* turn;
    const EspPlayerViewState* playerView;
    const EspNativeGameplayTurnState* dispatch;
    MovementCandidate candidate;
    uint32_t candidates;
    uint32_t deferredCount;
    ProbeRng probeRng;
    uint8_t aiDecision;
    uint8_t closeDecision = 0U;
    uint8_t closeDecisionUsed = 0U;
    int i7;
    int i8;
    int32_t targetX;
    int32_t targetY;
    MovePlan plan;
    int planStatus;
    EspNativeGameplayMonsterPositionRecord positionBefore;
    EspNativeGameplayMonsterPositionRecord positionAfter;
    const EspNativeGameplayMonsterPositionRecord* positionFinal;
    uint32_t positionFNVBefore;
    uint32_t positionFNVCommitted;
    uint32_t positionFNVAfter;
    Random_t liveRandomBefore;
    int positionExact;
    int randomExact;

    if (doomRpg == NULL || !syncOwner()) return;
    turn = EspNativeGameplayMonsterTurn_view();
    if (turn == NULL) return;
    deferredCount = turn->movementDeferredTurns;
    if (deferredCount == movementView.observedMovementDeferredTurns) return;

    if (deferredCount != movementView.observedMovementDeferredTurns + 1U) {
        printf("[MONSTERMOVE] DEFER observed=%u current=%u cause=movement-turn-gap mutation=no rngConsumed=0\n",
               (unsigned int)movementView.observedMovementDeferredTurns,
               (unsigned int)deferredCount);
        movementView.observedMovementDeferredTurns = deferredCount;
        ++movementView.ambiguousGeometry;
        return;
    }
    movementView.observedMovementDeferredTurns = deferredCount;
    movementView.lastReason = turn->lastReason;
    ++movementView.probes;

    playerView = EspPlayerView_view();
    dispatch = EspNativeGameplayDispatch_view();
    if (playerView == NULL || dispatch == NULL || dispatch->active != 1U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle ||
        !((dispatch->viewStepX == MOVE_TILE_SIZE && dispatch->viewStepY == 0) ||
          (dispatch->viewStepX == -MOVE_TILE_SIZE && dispatch->viewStepY == 0) ||
          (dispatch->viewStepX == 0 && dispatch->viewStepY == MOVE_TILE_SIZE) ||
          (dispatch->viewStepX == 0 && dispatch->viewStepY == -MOVE_TILE_SIZE))) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] DEFER n=%u cause=player-orientation-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)deferredCount);
        return;
    }

    memset(&candidate, 0, sizeof(candidate));
    if (!findCandidate(playerView, &candidate, &candidates)) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] DEFER n=%u cause=candidate-trace-not-ready mutation=no rngConsumed=0\n",
               (unsigned int)deferredCount);
        return;
    }
    if (candidates != 1U || candidate.monster == NULL || candidate.position == NULL) {
        ++movementView.ambiguousGeometry;
        printf("[MONSTERMOVE] DEFER n=%u candidates=%u cause=attacker-order-ambiguous mutation=no rngConsumed=0\n",
               (unsigned int)deferredCount, (unsigned int)candidates);
        return;
    }

    liveRandomBefore = doomRpg->random;
    memset(&probeRng, 0, sizeof(probeRng));
    probeRng.state = liveRandomBefore;
    if (!probeNextByte(&probeRng, &aiDecision)) {
        ++movementView.rngBoundaryDeferred;
        printf("[MONSTERMOVE] RNG-BOUNDARY-DEFER n=%u sprite=%u nextRand=%d stage=ai-decision hiddenGenerator=untouched-by-movement-probe mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               liveRandomBefore.nextRand);
        return;
    }
    if (aiDecision < 217U) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] REPLAY-MISMATCH n=%u sprite=%u aiRand=%u expected=>=217 randomLive=untouched mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               (unsigned int)aiDecision);
        return;
    }

    i7 = (1 + (int)movementWeapons[candidate.weaponId].rangeMin) / 2;
    i8 = (i7 * MOVE_TILE_SIZE) * (i7 * MOVE_TILE_SIZE);
    targetX = playerView->destX + dispatch->viewStepX;
    targetY = playerView->destY + dispatch->viewStepY;
    if (candidate.worldDistance <= (uint32_t)i8 &&
        (candidate.position->worldX == (uint16_t)playerView->destX ||
         candidate.position->worldY == (uint16_t)playerView->destY)) {
        if (!probeNextByte(&probeRng, &closeDecision)) {
            ++movementView.rngBoundaryDeferred;
            printf("[MONSTERMOVE] RNG-BOUNDARY-DEFER n=%u sprite=%u nextRand=%d stage=close-bias rngCalls=%u hiddenGenerator=untouched-by-movement-probe mutation=no\n",
                   (unsigned int)deferredCount,
                   (unsigned int)candidate.monster->spriteIndex,
                   probeRng.state.nextRand,
                   (unsigned int)probeRng.calls);
            return;
        }
        closeDecisionUsed = 1U;
        if (closeDecision < 38U) {
            if ((int32_t)candidate.position->worldX < playerView->destX) targetX -= MOVE_TILE_SIZE;
            else if ((int32_t)candidate.position->worldX > playerView->destX) targetX += MOVE_TILE_SIZE;
            if ((int32_t)candidate.position->worldY < playerView->destY) targetY -= MOVE_TILE_SIZE;
            else if ((int32_t)candidate.position->worldY > playerView->destY) targetY += MOVE_TILE_SIZE;
        }
    }

    memset(&plan, 0, sizeof(plan));
    planStatus = aiGoalPlan(candidate.monster->spriteIndex,
                            candidate.monster->subtype,
                            (int32_t)candidate.position->worldX,
                            (int32_t)candidate.position->worldY,
                            targetX, targetY, &probeRng, &plan);
    if (planStatus == -3 || probeRng.boundary != 0U) {
        ++movementView.rngBoundaryDeferred;
        printf("[MONSTERMOVE] RNG-BOUNDARY-DEFER n=%u sprite=%u stage=visit-choice rngCalls=%u hiddenGenerator=untouched-by-movement-probe mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               (unsigned int)probeRng.calls);
        return;
    }
    if (planStatus == -2) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] DEFER n=%u sprite=%u cause=calcPath-special-plane-corpus-needed mask=%04x rngCalls=%u mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               (unsigned int)movementMaskForSubtype(candidate.monster->subtype),
               (unsigned int)probeRng.calls);
        return;
    }
    if (planStatus < 0) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] DEFER n=%u sprite=%u cause=planner-not-ready rngCalls=%u mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               (unsigned int)probeRng.calls);
        return;
    }
    if (planStatus == 0) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] NO-MOVE n=%u sprite=%u sourceTile=%u target=%d,%d mask=%04x visitCount=0 rngCalls=%u legacyFallbackAttack=deferred randomLive=untouched mutation=no\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex,
               (unsigned int)candidate.position->tileIndex,
               (int)targetX, (int)targetY,
               (unsigned int)movementMaskForSubtype(candidate.monster->subtype),
               (unsigned int)probeRng.calls);
        return;
    }

    positionFNVBefore = EspNativeGameplayMonsterPosition_fingerprint();
    if (!EspNativeGameplayMonsterPosition_prepareCardinalMove(
            candidate.monster->spriteIndex, plan.deltaX, plan.deltaY,
            &positionBefore, &positionAfter) ||
        positionBefore.tileIndex != plan.sourceTile ||
        positionAfter.tileIndex != plan.destTile ||
        !EspNativeGameplayMonsterPosition_commitPrepared(&positionBefore,
                                                         &positionAfter)) {
        ++movementView.collisionDeferred;
        printf("[MONSTERMOVE] DEFER n=%u sprite=%u cause=position-transaction-prepare-or-commit mutation=no-published rngLive=untouched\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex);
        return;
    }
    positionFNVCommitted = EspNativeGameplayMonsterPosition_fingerprint();
    if (!EspNativeGameplayMonsterPosition_rollbackPrepared(&positionAfter,
                                                           &positionBefore)) {
        printf("[MONSTERMOVE] FATAL-PROBE n=%u sprite=%u cause=position-rollback-failed liveMove=NO rendererPublish=deferred\n",
               (unsigned int)deferredCount,
               (unsigned int)candidate.monster->spriteIndex);
        return;
    }
    positionFNVAfter = EspNativeGameplayMonsterPosition_fingerprint();
    positionFinal = EspNativeGameplayMonsterPosition_find(candidate.monster->spriteIndex);
    positionExact = positionFinal != NULL &&
                    memcmp(positionFinal, &positionBefore,
                           sizeof(positionBefore)) == 0 &&
                    positionFNVAfter == positionFNVBefore;
    randomExact = memcmp(&doomRpg->random, &liveRandomBefore,
                         sizeof(liveRandomBefore)) == 0;

    ++movementView.plannedMoves;
    ++movementView.rollbackMoves;
    movementView.lastSpriteIndex = candidate.monster->spriteIndex;
    movementView.lastSourceTile = plan.sourceTile;
    movementView.lastDestTile = plan.destTile;
    movementView.lastPositionFNV1a = positionFNVAfter;

    printf("[MONSTERMOVE] PROBE n=%u reason=%u sprite=%u subtype=%u weapon=%u sourceTile=%u source=%u,%u target=%d,%d destTile=%u delta=%d,%d aiRand=%u closeRand=%s%u visitCount=%u tieRand=%u choice=%u mask=%04x rngCalls=%u rngBoundary=no randomLiveUntouched=%s positionFNV=%08x->%08x->%08x positionRollback=%s rendererPublish=deferred topologyRelink=deferred liveMove=no mutation=no\n",
           (unsigned int)deferredCount,
           (unsigned int)turn->lastReason,
           (unsigned int)candidate.monster->spriteIndex,
           (unsigned int)candidate.monster->subtype,
           (unsigned int)candidate.weaponId,
           (unsigned int)plan.sourceTile,
           (unsigned int)positionBefore.worldX,
           (unsigned int)positionBefore.worldY,
           (int)targetX, (int)targetY,
           (unsigned int)plan.destTile,
           (int)plan.deltaX, (int)plan.deltaY,
           (unsigned int)aiDecision,
           closeDecisionUsed != 0U ? "value/" : "unused/",
           (unsigned int)closeDecision,
           (unsigned int)plan.visitCount,
           (unsigned int)plan.tieRand,
           (unsigned int)plan.choice,
           (unsigned int)plan.traceMask,
           (unsigned int)probeRng.calls,
           randomExact ? "yes" : "NO",
           (unsigned int)positionFNVBefore,
           (unsigned int)positionFNVCommitted,
           (unsigned int)positionFNVAfter,
           positionExact ? "yes" : "NO");
}

void __real_EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* doomRpg);
void __real_EspNativeGameplayMonsterRetaliation_reset(void);

/* Compose the movement recovery immediately after the already hardware-proven
 * retaliation layer. It observes only turn counters and never changes the live
 * player/RNG/render/topology state. */
void __wrap_EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* doomRpg) {
    __real_EspNativeGameplayMonsterRetaliation_service(doomRpg);
    EspNativeGameplayMonsterMovement_service(doomRpg);
}

void __wrap_EspNativeGameplayMonsterRetaliation_reset(void) {
    EspNativeGameplayMonsterMovement_reset();
    EspNativeGameplayMonsterPosition_reset();
    __real_EspNativeGameplayMonsterRetaliation_reset();
}
