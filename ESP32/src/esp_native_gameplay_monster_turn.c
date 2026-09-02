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
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_view_state.h"

#define TURN_TRACE_MASK 0x5687U
#define TURN_MAP_WIDTH 32U
#define TURN_TILE_SIZE 64
#define TURN_TILE_CENTER 32
#define TURN_NO_SPRITE 0xffffU
#define TURN_TYPE_ENEMY 1U
#define TURN_TYPE_DESTRUCTIBLE 12U
#define TURN_TYPE_SPECIAL_TRACE 14U
#define TURN_SPECIAL_TRACE_ENTITY_FLAG 0x00020000UL
#define TURN_SPECIAL_TRACE_Y_MASK 0x00180000UL
#define TURN_SPECIAL_TRACE_X_MASK 0x00600000UL
#define TURN_LINE_ENTITY_DEF_BASE 305U
#define TURN_LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL
#define TURN_LINE_GEOMETRY_AXIS_X 0x00000008UL
#define TURN_LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define TURN_LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define TURN_LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define TURN_LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define TURN_LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define TURN_LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define TURN_LINE_ENTITY_NUDGE_X_NEG 0x00004000UL
#define TURN_SUBTYPE_SPECIAL_AI 10U
#define TURN_MONSTER_WEAPON_COUNT 19U
#define TURN_HIT_MISS 0U
#define TURN_HIT_NORMAL 1U
#define TURN_HIT_CRIT 2U

/* Exact CombatEntity.c subtype -> primary/alternate weapon table. */
static const uint8_t monsterAttacks[28] = {
    2U, 3U, 12U, 13U, 4U, 4U, 15U, 12U, 13U, 14U, 13U, 12U, 15U, 13U,
    15U, 14U, 7U, 12U, 7U, 3U, 15U, 15U, 16U, 17U, 7U, 17U, 12U, 13U
};

/* Exact Combat monsterWpInfo NUMSHOTS field, one value per subtype. */
static const uint8_t monsterShots[14] = {
    1U, 1U, 3U, 1U, 3U, 1U, 3U, 1U, 1U, 1U, 1U, 1U, 1U, 3U
};

typedef struct MonsterWeaponSpec_s {
    uint8_t strMin;
    uint8_t strMax;
    uint8_t rangeMin;
    uint8_t rangeMax;
    uint8_t armorSplit;
    uint8_t valid;
} MonsterWeaponSpec;

/* Only attack IDs reachable from monsterAttacks[] are populated. The values are
 * copied from Combat_init(); target-player weapon multiplier is always neutral
 * 256 because Player CombatEntity.mType is -1 in the legacy engine. */
static const MonsterWeaponSpec monsterWeapons[TURN_MONSTER_WEAPON_COUNT] = {
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {6U, 7U, 5U, 80U, 102U, 1U}, /* 2 pistol */
    {6U, 10U, 2U, 80U, 128U, 1U}, /* 3 shotgun */
    {3U, 6U, 3U, 90U, 102U, 1U}, /* 4 chaingun */
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {15U, 36U, 8U, 70U, 128U, 1U}, /* 7 rocket */
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {3U, 5U, 0U, 90U, 128U, 1U}, /* 12 melee 1 */
    {4U, 7U, 0U, 80U, 128U, 1U}, /* 13 melee 2 */
    {5U, 15U, 0U, 70U, 128U, 1U}, /* 14 melee 3 */
    {4U, 10U, 3U, 85U, 128U, 1U}, /* 15 missile */
    {10U, 20U, 3U, 75U, 128U, 1U}, /* 16 boss missile */
    {15U, 30U, 2U, 80U, 128U, 1U}, /* 17 rocket missile */
    {0U, 0U, 0U, 0U, 0U, 0U}
};

typedef struct MonsterTurnRoll_s {
    int32_t totalDamage;
    int32_t totalArmorDamage;
    int32_t firstCalcHit;
    int32_t firstCritLimit;
    uint32_t rngCalls;
    uint32_t missProjectileRngCalls;
    uint8_t firstRandHit;
    uint8_t firstRandDamage;
    uint8_t loops;
    uint8_t hitLoops;
    uint8_t gotCrit;
} MonsterTurnRoll;

typedef struct MonsterTurnCandidate_s {
    const EspNativeGameplayMonsterRecord* monster;
    uint32_t worldDistance;
    uint16_t tileIndex;
    uint8_t weaponId;
    uint8_t loops;
} MonsterTurnCandidate;

typedef struct MonsterTurnOwner_s {
    EspNativeGameplayMonsterTurnView view;
    int32_t lastViewX;
    int32_t lastViewY;
    int32_t lastViewAngle;
    uint32_t observedCombatAttacks;
    uint8_t viewBaseline;
    uint8_t combatBaseline;
    uint8_t reserved[2];
} MonsterTurnOwner;

static MonsterTurnOwner turnOwner;

void __real_EspNativeGameplayPlayerResources_sessionService(struct DoomRPG_s* doomRpg);
void __real_EspNativeGameplayPlayerResources_sessionReset(void);

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

static uint32_t fnvByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv32(uint32_t hash, uint32_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t randomFNV(const Random_t* random) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (random == NULL) return 0U;
    for (i = 0U; i < RANDTABLESIZE; ++i) {
        hash = fnvByte(hash, random->randTable[i]);
    }
    return fnv32(hash, (uint32_t)random->nextRand);
}

static const char* reasonName(uint8_t reason) {
    switch ((EspNativeGameplayMonsterTurnReason)reason) {
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE: return "MOVE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE: return "ROTATE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK: return "PLAYER_ATTACK";
    default: return "NONE";
    }
}

static int centeredCoordinate(int32_t value) {
    return value >= TURN_TILE_CENTER &&
           value <= (int32_t)(((TURN_MAP_WIDTH - 1U) * TURN_TILE_SIZE) + TURN_TILE_CENTER) &&
           (value & (TURN_TILE_SIZE - 1)) == TURN_TILE_CENTER;
}

static int tileCenter(uint16_t tile, int32_t* outX, int32_t* outY) {
    uint32_t x;
    uint32_t y;
    if (outX == NULL || outY == NULL || tile >= TURN_MAP_WIDTH * TURN_MAP_WIDTH) {
        return 0;
    }
    x = tile % TURN_MAP_WIDTH;
    y = tile / TURN_MAP_WIDTH;
    *outX = (int32_t)(x * TURN_TILE_SIZE + TURN_TILE_CENTER);
    *outY = (int32_t)(y * TURN_TILE_SIZE + TURN_TILE_CENTER);
    return 1;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tx;
    uint32_t ty;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) return 0;
    tx = (uint32_t)x >> 6;
    ty = (uint32_t)y >> 6;
    if (tx >= TURN_MAP_WIDTH || ty >= TURN_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(ty * TURN_MAP_WIDTH + tx);
    return 1;
}

static int typeInTraceMask(uint8_t type) {
    return type < 16U && (TURN_TRACE_MASK & (1U << type)) != 0U;
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

    if ((line->flags & TURN_LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & TURN_LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & TURN_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & TURN_LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & TURN_LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & TURN_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & TURN_LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & TURN_LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & TURN_LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & TURN_LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    if (x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= TURN_MAP_WIDTH || tileY >= TURN_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * TURN_MAP_WIDTH + tileX);
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
        lookup = TURN_LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        (void)subtype;
        if (!hasDefinition) {
            if ((line.flags & TURN_LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
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
    if ((sprite.info & TURN_SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;
    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
    if ((sprite.info & TURN_SPECIAL_TRACE_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite.info & TURN_SPECIAL_TRACE_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
}

static int blockingSpriteOnTile(uint16_t tile,
                                int32_t sourceX,
                                int32_t sourceY,
                                int32_t destX,
                                int32_t destY,
                                uint16_t attackerSprite) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t bestOrder = 0U;
    uint8_t bestType = 0xffU;
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
            !typeInTraceMask(type)) {
            continue;
        }
        if (type == TURN_TYPE_SPECIAL_TRACE) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY);
            if (specialBlocks < 0) return -1;
            if (specialBlocks == 0) continue;
        }
        if (!found || linkOrder > bestOrder) {
            found = 1;
            bestOrder = linkOrder;
            bestType = type;
        }
    }

    if (!found) return 0;
    /* Legacy Entity_aiThink targets a traced destructible instead of the player.
     * This first retaliation family owns only direct player attacks, so any
     * trace-mask blocker (including type12) fails closed here. */
    (void)bestType;
    return 1;
}

static int cardinalLosClear(int32_t sourceX,
                            int32_t sourceY,
                            int32_t destX,
                            int32_t destY,
                            uint16_t attackerSprite) {
    int32_t stepX = 0;
    int32_t stepY = 0;
    uint32_t distance;
    uint32_t i;

    if (!centeredCoordinate(sourceX) || !centeredCoordinate(sourceY) ||
        !centeredCoordinate(destX) || !centeredCoordinate(destY)) return 0;
    if (sourceX == destX && sourceY != destY) {
        stepY = destY > sourceY ? TURN_TILE_SIZE : -TURN_TILE_SIZE;
        distance = (uint32_t)((destY > sourceY ? destY - sourceY : sourceY - destY) /
                              TURN_TILE_SIZE);
    }
    else if (sourceY == destY && sourceX != destX) {
        stepX = destX > sourceX ? TURN_TILE_SIZE : -TURN_TILE_SIZE;
        distance = (uint32_t)((destX > sourceX ? destX - sourceX : sourceX - destX) /
                              TURN_TILE_SIZE);
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
        lineBlock = closedLineBlocksTile(tile);
        if (lineBlock != 0) return 0;
        spriteBlock = blockingSpriteOnTile(tile,
                                           x - stepX, y - stepY, x, y,
                                           attackerSprite);
        if (spriteBlock != 0) return 0;
    }
    return 1;
}

static int projectileMissConsumesRng(uint8_t weaponId) {
    return weaponId == 6U || weaponId == 7U || weaponId == 8U ||
           (weaponId >= 12U && weaponId <= 17U);
}

static int rangeMinToDist(const MonsterWeaponSpec* weapon) {
    int range = (int)weapon->rangeMin * TURN_TILE_SIZE;
    return (range * range) + 4096;
}

static int rollMonsterAttack(DoomRPG_t* doomRpg,
                             const EspNativeGameplayMonsterRecord* monster,
                             const EspNativeGameplayPlayerState* player,
                             uint8_t weaponId,
                             uint8_t loops,
                             MonsterTurnRoll* outRoll) {
    const MonsterWeaponSpec* weapon;
    uint8_t loop;
    int playerDefense;
    int playerAgility;

    if (outRoll != NULL) memset(outRoll, 0, sizeof(*outRoll));
    if (doomRpg == NULL || monster == NULL || player == NULL || outRoll == NULL ||
        weaponId >= TURN_MONSTER_WEAPON_COUNT || loops == 0U) return 0;
    weapon = &monsterWeapons[weaponId];
    if (weapon->valid == 0U) return 0;
    playerDefense = (int)p2Defense(player->param2) << 8;
    playerAgility = (int)p2Agility(player->param2) << 8;
    if (playerDefense <= 0 || playerAgility <= 0) return 0;

    outRoll->loops = loops;
    for (loop = 0U; loop < loops; ++loop) {
        int calcHit;
        int critLimit;
        int randHit;
        int hitType;

        calcHit = (((((int)p2Accuracy(monster->param2) << 16) /
                      playerAgility) * 128) >> 8) +
                  (((int)weapon->rangeMax << 16) / 51200);
        critLimit = (calcHit << 8) / 5120;
        randHit = DoomRPG_randNextByte(&doomRpg->random);
        ++outRoll->rngCalls;
        if (loop == 0U) {
            outRoll->firstRandHit = (uint8_t)randHit;
            outRoll->firstCalcHit = calcHit;
            outRoll->firstCritLimit = critLimit;
        }
        hitType = randHit < calcHit
                      ? (randHit < critLimit ? TURN_HIT_CRIT : TURN_HIT_NORMAL)
                      : TURN_HIT_MISS;

        if (hitType == TURN_HIT_MISS) {
            if (projectileMissConsumesRng(weaponId)) {
                (void)DoomRPG_randNextByte(&doomRpg->random);
                ++outRoll->rngCalls;
                ++outRoll->missProjectileRngCalls;
            }
            continue;
        }

        ++outRoll->hitLoops;
        if (loop == 0U && hitType == TURN_HIT_CRIT) outRoll->gotCrit = 1U;
        {
            int randStr = DoomRPG_randNextByte(&doomRpg->random);
            int strength = (int)p2Strength(monster->param2) << 16;
            int weaponRoll;
            int calDmg;
            int calDmgArm;
            int attackScale = outRoll->gotCrit != 0U ? 512 : 256;

            ++outRoll->rngCalls;
            if (loop == 0U) outRoll->firstRandDamage = (uint8_t)randStr;
            weaponRoll = ((int)weapon->strMin << 8) +
                         ((randStr * (((int)weapon->strMax -
                                      (int)weapon->strMin) << 8)) >> 8);
            calDmg = (((((((weaponRoll * (strength / playerDefense)) >> 8) *
                           attackScale) >> 8) * 256) >> 8));
            /* Combat_monsterSeq passes dist=0 to CombatEntity_calcDamage, so the
             * legacy distance-decay loop cannot execute. Keep the exact clamp and
             * armor split only. */
            if (calDmg < 256) calDmg = 256;
            else if (calDmg > 255744) calDmg = 255744;
            calDmgArm = (calDmg * (int)weapon->armorSplit) >> 8;
            outRoll->totalArmorDamage += (calDmgArm + 128) >> 8;
            outRoll->totalDamage += ((calDmg - calDmgArm) + 128) >> 8;
        }
    }
    return 1;
}

static void prospectivePlayerPain(const EspNativeGameplayPlayerState* player,
                                  int32_t damage,
                                  int32_t armorDamage,
                                  uint8_t* outHealth,
                                  uint8_t* outArmor) {
    int32_t health;
    int32_t armor;
    int32_t healthDamage;
    if (outHealth == NULL || outArmor == NULL || player == NULL) return;
    health = p1Health(player->param1);
    armor = p1Armor(player->param1);
    healthDamage = damage;
    if (armor < armorDamage) {
        healthDamage += armorDamage - armor;
        armor = 0;
    }
    else {
        armor -= armorDamage;
    }
    health -= healthDamage;
    if (health < 0) health = 0;
    *outHealth = (uint8_t)health;
    *outArmor = (uint8_t)armor;
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterView* monsters = EspNativeGameplayMonsterState_view();
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t arena;

    if (monsters == NULL || monsters->records == NULL || monsters->count == 0U ||
        runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->arenaFNV1a != monsters->sourceArenaFNV1a ||
        !EspMapSpriteTopology_isReady() || !EspMapState_isReady() ||
        !EspMapLineState_isReady() || !EspNativeGameplayPlayerState_ensure()) {
        return 0;
    }
    arena = runtime->arenaFNV1a;
    if (turnOwner.view.active == 0U || turnOwner.view.sourceArenaFNV1a != arena) {
        memset(&turnOwner, 0, sizeof(turnOwner));
        turnOwner.view.sourceArenaFNV1a = arena;
        turnOwner.view.lastAttackerSpriteIndex = TURN_NO_SPRITE;
        turnOwner.view.active = 1U;
        printf("[MONSTERTURN] READY arena=%08x ownerBytes=%u mode=probe+rollback schedule=MOVE+ROTATE+PLAYER_ATTACK attackFamily=stationary-cardinal-generic traceMask=%04x playerDamage=prospective movementPositions=deferred activationOrder=fail-closed subtype10AI=deferred mutation=no\n",
               (unsigned int)arena,
               (unsigned int)sizeof(turnOwner),
               (unsigned int)TURN_TRACE_MASK);
    }
    return 1;
}

static int findCandidate(const EspPlayerViewState* playerView,
                         MonsterTurnCandidate* outCandidate,
                         uint32_t* outCandidates,
                         uint32_t* outSpecialDeferred) {
    const EspNativeGameplayMonsterView* monsters = EspNativeGameplayMonsterState_view();
    uint32_t candidates = 0U;
    uint32_t specialDeferred = 0U;
    uint32_t i;

    if (outCandidate != NULL) memset(outCandidate, 0, sizeof(*outCandidate));
    if (outCandidates != NULL) *outCandidates = 0U;
    if (outSpecialDeferred != NULL) *outSpecialDeferred = 0U;
    if (playerView == NULL || outCandidate == NULL || outCandidates == NULL ||
        outSpecialDeferred == NULL || monsters == NULL || monsters->records == NULL ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        !centeredCoordinate(playerView->destX) ||
        !centeredCoordinate(playerView->destY)) {
        return 0;
    }

    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* monster = &monsters->records[i];
        const MonsterWeaponSpec* weapon;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t tile;
        uint8_t weaponId;
        int32_t monsterX;
        int32_t monsterY;
        int64_t dx;
        int64_t dy;
        uint32_t worldDistance;
        int attackRange;

        if (monster->alive == 0U || monster->subtype >= 14U) continue;
        if (!EspMapSpriteTopology_getEntity(monster->spriteIndex,
                                            &type, &subtype,
                                            &linkState, &linkOrder)) return 0;
        (void)linkOrder;
        if (type != TURN_TYPE_ENEMY || subtype != monster->subtype ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) {
            continue;
        }
        if (monster->subtype == TURN_SUBTYPE_SPECIAL_AI) {
            ++specialDeferred;
            continue;
        }

        weaponId = monsterAttacks[(uint32_t)monster->subtype * 2U +
                                  (monster->alternateAttack != 0U ? 1U : 0U)];
        if (weaponId >= TURN_MONSTER_WEAPON_COUNT ||
            monsterWeapons[weaponId].valid == 0U) continue;
        weapon = &monsterWeapons[weaponId];
        tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
        if (!tileCenter(tile, &monsterX, &monsterY)) return 0;
        dx = (int64_t)monsterX - playerView->destX;
        dy = (int64_t)monsterY - playerView->destY;
        if (dx != 0 && dy != 0) continue;
        if (dx == 0 && dy == 0) continue;
        worldDistance = (uint32_t)(dx * dx + dy * dy);
        attackRange = (1 + (int)weapon->rangeMin) * TURN_TILE_SIZE;
        if (worldDistance > (uint32_t)(attackRange * attackRange)) continue;
        if (!cardinalLosClear(monsterX, monsterY,
                              playerView->destX, playerView->destY,
                              monster->spriteIndex)) {
            continue;
        }

        ++candidates;
        if (candidates == 1U) {
            outCandidate->monster = monster;
            outCandidate->worldDistance = worldDistance;
            outCandidate->tileIndex = tile;
            outCandidate->weaponId = weaponId;
            outCandidate->loops = monsterShots[monster->subtype];
        }
    }

    *outCandidates = candidates;
    *outSpecialDeferred = specialDeferred;
    return 1;
}

static void runProbe(DoomRPG_t* doomRpg, uint8_t reason) {
    const EspPlayerViewState* playerView = EspPlayerView_view();
    const EspNativeGameplayPlayerState* player = EspNativeGameplayPlayerState_view();
    MonsterTurnCandidate candidate;
    MonsterTurnRoll roll;
    EspNativeGameplayPlayerState playerBefore;
    Random_t randomBefore;
    uint32_t randomFNVBefore;
    uint32_t randomFNVAfter;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;
    uint32_t candidates;
    uint32_t specialDeferred;
    uint32_t aiRngCalls = 0U;
    uint8_t aiDecision = 0U;
    uint8_t healthAfter;
    uint8_t armorAfter;
    int rngExact;
    int playerExact;

    ++turnOwner.view.probes;
    turnOwner.view.lastReason = reason;
    turnOwner.view.lastAttackerSpriteIndex = TURN_NO_SPRITE;

    if (doomRpg == NULL || playerView == NULL || player == NULL ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
        printf("[MONSTERTURN] PROBE reason=%s status=NOT_READY mutation=no\n",
               reasonName(reason));
        return;
    }

    memset(&candidate, 0, sizeof(candidate));
    if (!findCandidate(playerView, &candidate, &candidates, &specialDeferred)) {
        printf("[MONSTERTURN] PROBE reason=%s status=TRACE_NOT_READY mutation=no\n",
               reasonName(reason));
        return;
    }
    if (candidates == 0U) {
        ++turnOwner.view.noAttackTurns;
        printf("[MONSTERTURN] COMPLETE reason=%s candidates=0 specialAIDeferred=%u movementPositions=deferred activationOrder=not-needed mutation=no\n",
               reasonName(reason), (unsigned int)specialDeferred);
        return;
    }
    if (candidates != 1U || candidate.monster == NULL) {
        ++turnOwner.view.ambiguousTurns;
        printf("[MONSTERTURN] DEFER reason=%s candidates=%u specialAIDeferred=%u cause=activation/attacker-order-not-owned mutation=no rngConsumed=0\n",
               reasonName(reason),
               (unsigned int)candidates,
               (unsigned int)specialDeferred);
        return;
    }

    randomBefore = doomRpg->random;
    randomFNVBefore = randomFNV(&randomBefore);
    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();

    /* Exact Entity_aiThink immediate-attack gate for an already aligned/in-range
     * target. rangeMin==0 attacks immediately. Ranged families consume one AI
     * decision byte; >=217 enters movement/pathfinding, which remains fail-closed
     * until native mutable monster positions are owned. */
    if (((1U + (uint32_t)monsterWeapons[candidate.weaponId].rangeMin) / 2U) != 0U) {
        aiDecision = DoomRPG_randNextByte(&doomRpg->random);
        ++aiRngCalls;
        if (aiDecision >= 217U) {
            doomRpg->random = randomBefore;
            ++turnOwner.view.movementDeferredTurns;
            randomFNVAfter = randomFNV(&doomRpg->random);
            printf("[MONSTERTURN] MOVE-DEFER reason=%s sprite=%u subtype=%u tile=%u weapon=%u aiRand=%u threshold=217 movementPositions=not-owned rngCalls=%u rng=%08x->%08x rollback=yes mutation=no\n",
                   reasonName(reason),
                   (unsigned int)candidate.monster->spriteIndex,
                   (unsigned int)candidate.monster->subtype,
                   (unsigned int)candidate.tileIndex,
                   (unsigned int)candidate.weaponId,
                   (unsigned int)aiDecision,
                   (unsigned int)aiRngCalls,
                   (unsigned int)randomFNVBefore,
                   (unsigned int)randomFNVAfter);
            return;
        }
    }

    memset(&roll, 0, sizeof(roll));
    if (!rollMonsterAttack(doomRpg, candidate.monster, player,
                           candidate.weaponId, candidate.loops, &roll)) {
        doomRpg->random = randomBefore;
        printf("[MONSTERTURN] PROBE reason=%s sprite=%u status=ROLL_FAILED rollback=yes mutation=no\n",
               reasonName(reason),
               (unsigned int)candidate.monster->spriteIndex);
        return;
    }

    prospectivePlayerPain(player, roll.totalDamage, roll.totalArmorDamage,
                          &healthAfter, &armorAfter);
    doomRpg->random = randomBefore;
    randomFNVAfter = randomFNV(&doomRpg->random);
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
    rngExact = memcmp(&doomRpg->random, &randomBefore, sizeof(randomBefore)) == 0;
    playerExact = memcmp(EspNativeGameplayPlayerState_view(),
                         &playerBefore, sizeof(playerBefore)) == 0;

    ++turnOwner.view.attackProbes;
    turnOwner.view.lastAttackerSpriteIndex = candidate.monster->spriteIndex;
    printf("[MONSTERTURN] ATTACK-PROBE reason=%s sprite=%u subtype=%u mType=%u tile=%u weapon=%u alt=%u loops=%u hitLoops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d firstRandDamage=%u totalDamage=%d armorDamage=%d crit=%u aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u->%u armor=%u->%u lethal=%s playerFNV=%08x->%08x rng=%08x->%08x rngRollback=%s playerExact=%s activation=probe-only movementPositions=deferred mutation=no\n",
           reasonName(reason),
           (unsigned int)candidate.monster->spriteIndex,
           (unsigned int)candidate.monster->subtype,
           (unsigned int)candidate.monster->mType,
           (unsigned int)candidate.tileIndex,
           (unsigned int)candidate.weaponId,
           (unsigned int)candidate.monster->alternateAttack,
           (unsigned int)roll.loops,
           (unsigned int)roll.hitLoops,
           (unsigned int)roll.firstRandHit,
           (int)roll.firstCalcHit,
           (int)roll.firstCritLimit,
           (unsigned int)roll.firstRandDamage,
           (int)roll.totalDamage,
           (int)roll.totalArmorDamage,
           (unsigned int)roll.gotCrit,
           aiRngCalls != 0U ? "value/" : "unused/",
           (unsigned int)aiDecision,
           (unsigned int)(aiRngCalls + roll.rngCalls),
           (unsigned int)roll.rngCalls,
           (unsigned int)roll.missProjectileRngCalls,
           (unsigned int)p1Health(player->param1),
           (unsigned int)healthAfter,
           (unsigned int)p1Armor(player->param1),
           (unsigned int)armorAfter,
           healthAfter == 0U ? "deferred-player-death" : "no",
           (unsigned int)playerFNVBefore,
           (unsigned int)playerFNVAfter,
           (unsigned int)randomFNVBefore,
           (unsigned int)randomFNVAfter,
           rngExact ? "yes" : "NO",
           playerExact ? "yes" : "NO");
}

static void observeAndProbe(DoomRPG_t* doomRpg) {
    const EspPlayerViewState* playerView;
    const EspNativeGameplayMonsterCombatView* combat;
    uint8_t reason = ESP_NATIVE_GAMEPLAY_MONSTER_TURN_NONE;

    if (!syncOwner()) return;
    playerView = EspPlayerView_view();
    combat = EspNativeGameplayMonsterCombat_view();

    if (playerView != NULL && playerView->active == 1U &&
        playerView->viewX == playerView->destX &&
        playerView->viewY == playerView->destY &&
        playerView->viewAngle == playerView->destAngle) {
        if (turnOwner.viewBaseline == 0U) {
            turnOwner.lastViewX = playerView->viewX;
            turnOwner.lastViewY = playerView->viewY;
            turnOwner.lastViewAngle = playerView->viewAngle;
            turnOwner.viewBaseline = 1U;
        }
        else {
            if (playerView->viewX != turnOwner.lastViewX ||
                playerView->viewY != turnOwner.lastViewY) {
                reason = ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE;
            }
            else if (playerView->viewAngle != turnOwner.lastViewAngle) {
                reason = ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE;
            }
            turnOwner.lastViewX = playerView->viewX;
            turnOwner.lastViewY = playerView->viewY;
            turnOwner.lastViewAngle = playerView->viewAngle;
        }
    }

    if (combat != NULL && combat->active == 1U) {
        if (turnOwner.combatBaseline == 0U) {
            turnOwner.observedCombatAttacks = combat->attacks;
            turnOwner.combatBaseline = 1U;
        }
        else if (combat->attacks != turnOwner.observedCombatAttacks) {
            turnOwner.observedCombatAttacks = combat->attacks;
            turnOwner.view.observedPlayerAttacks = combat->attacks;
            reason = ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK;
        }
    }

    if (reason == ESP_NATIVE_GAMEPLAY_MONSTER_TURN_NONE) return;
    if (EspNativeGameplayDialog_isActive()) {
        printf("[MONSTERTURN] SKIP reason=%s dialog=active legacySkipTurn=yes mutation=no\n",
               reasonName(reason));
        return;
    }

    ++turnOwner.view.scheduledTurns;
    printf("[MONSTERTURN] SCHEDULE n=%u reason=%s player=%d,%d angle=%d playerFNV=%08x monsterFNV=%08x mode=probe rollback=required\n",
           (unsigned int)turnOwner.view.scheduledTurns,
           reasonName(reason),
           playerView != NULL ? (int)playerView->viewX : -1,
           playerView != NULL ? (int)playerView->viewY : -1,
           playerView != NULL ? (int)playerView->viewAngle : -1,
           (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
           combat != NULL ? (unsigned int)combat->currentMonsterFNV1a : 0U);
    runProbe(doomRpg, reason);
}

void EspNativeGameplayMonsterTurn_reset(void) {
    memset(&turnOwner, 0, sizeof(turnOwner));
    turnOwner.view.lastAttackerSpriteIndex = TURN_NO_SPRITE;
}

const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void) {
    return syncOwner() ? &turnOwner.view : NULL;
}

/* Insert the turn owner immediately after the already-proven player-resource /
 * action-engine session leaf. The outer gib-FX session wrapper still runs after
 * this function and retains its independent presentation-expiry ownership. */
void __wrap_EspNativeGameplayPlayerResources_sessionService(struct DoomRPG_s* doomRpg) {
    __real_EspNativeGameplayPlayerResources_sessionService(doomRpg);
    observeAndProbe((DoomRPG_t*)doomRpg);
}

void __wrap_EspNativeGameplayPlayerResources_sessionReset(void) {
    EspNativeGameplayMonsterTurn_reset();
    __real_EspNativeGameplayPlayerResources_sessionReset();
}
