#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_player_view_state.h"

/* This translation unit is the public linker-wrapper owner. The older action
 * and monster-state wrappers are private chain leaves renamed by their headers. */
#undef __wrap_EspMapSpriteTopology_getEntity
#undef __wrap_EspMapSpriteTopology_getVisualState
#undef __wrap_EspNativeGameplayActionEngine_service
#undef __wrap_EspNativeGameplayActionEngine_reset

#define MONSTER_TRACE_MASK 0x5687U
#define MONSTER_TRACE_TILES 8U
#define MONSTER_TYPE_ENEMY 1U
#define MONSTER_SUBTYPE_COUNT 14U
#define MONSTER_NO_SPRITE 0xffffU
#define MONSTER_PAIN_VISUAL 6U
#define MONSTER_DEATH_VISUAL 4U
#define MONSTER_PAIN_MS 250U

#define PLAYER_FRESH_ACCURACY 16
#define PLAYER_FRESH_STRENGTH 12
#define PLAYER_BERSERK_SCALE 256

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

/* These four legacy enemy families have death consequences beyond the generic
 * XP + corpse unlink + drop path (spawn/extra animation/boss scripts). Generic
 * hit/pain is still supported; only a lethal commit fails closed for now. */
#define SPECIAL_DEATH_MASK ((1U << 7) | (1U << 8) | (1U << 12) | (1U << 13))

typedef struct WeaponSpec_s {
    uint8_t strMin;
    uint8_t strMax;
    uint8_t rangeMin;
    uint8_t rangeMax;
    uint8_t ammoType;
    uint8_t ammoUsage;
    uint8_t damage;
    uint16_t resourceId;
} WeaponSpec;

typedef struct MonsterTarget_s {
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t distance;
    uint32_t worldDistance;
} MonsterTarget;

typedef struct MonsterCombatPending_s {
    uint32_t sequence;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint32_t worldDistance;
    uint8_t subtype;
    uint8_t distance;
    uint8_t weapon;
    uint8_t active;
} MonsterCombatPending;

typedef struct MonsterCombatOwner_s {
    EspNativeGameplayMonsterCombatView view;
    MonsterCombatPending pending;
    uint32_t painUntilMs;
} MonsterCombatOwner;

static const WeaponSpec playerWeapons[9] = {
    {3U, 12U, 0U, 70U, 0U, 0U, 25U, 5044U},
    {1U,  2U, 0U,100U, 0U, 1U,204U, 5045U},
    {6U,  7U, 5U, 80U, 1U, 1U,102U, 5046U},
    {6U, 10U, 2U, 80U, 2U, 1U,128U, 5047U},
    {3U,  6U, 3U, 90U, 1U, 3U,102U, 5048U},
    {12U,18U, 1U, 90U, 2U, 2U, 51U, 5049U},
    {6U,  8U, 4U, 90U, 4U, 3U,230U, 5050U},
    {15U,36U, 8U, 70U, 3U, 1U,128U, 5051U},
    {60U,105U,8U,100U, 4U,15U, 76U, 5052U}
};

static const uint16_t painSounds[MONSTER_SUBTYPE_COUNT] = {
    5085U, 5089U, 5085U, 5085U, 5099U, 5099U, 5099U,
    5110U, 5085U, 5117U, 5122U, 5099U, 5099U, 5137U
};

static const uint16_t deathSounds[MONSTER_SUBTYPE_COUNT][3] = {
    {5082U, 5084U, 5107U},
    {5090U,    0U,    0U},
    {5082U, 5084U, 5107U},
    {5076U, 5077U,    0U},
    {5101U,    0U,    0U},
    {5095U,    0U,    0U},
    {5102U,    0U,    0U},
    {5111U,    0U,    0U},
    {5113U,    0U,    0U},
    {5118U,    0U,    0U},
    {5123U,    0U,    0U},
    {5126U,    0U,    0U},
    {5129U,    0U,    0U},
    {5138U,    0U,    0U}
};

static MonsterCombatOwner combatOwner;

extern DoomRPG_t* doomRpg;

EspNativeGameplayActionStatus __real_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1MaxHealth(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p1MaxArmor(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

static uint32_t fnvByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv16(uint32_t hash, uint16_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t fnv32(uint32_t hash, uint32_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t currentMonsterFNV(void) {
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (monsters == NULL || monsters->records == NULL) return 0U;
    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* r = &monsters->records[i];
        hash = fnv32(hash, r->param1);
        hash = fnv32(hash, r->param2);
        hash = fnv16(hash, r->spriteIndex);
        hash = fnv16(hash, r->defTile);
        hash = fnvByte(hash, r->subtype);
        hash = fnvByte(hash, r->mType);
        hash = fnvByte(hash, r->alternateAttack);
        hash = fnvByte(hash, r->alive);
    }
    return hash;
}

static void expirePain(void) {
    if (combatOwner.view.painSpriteIndex == MONSTER_NO_SPRITE) return;
    if ((int32_t)(DoomRPG_GetUpTimeMS() - combatOwner.painUntilMs) >= 0) {
        combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.painUntilMs = 0U;
    }
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    uint32_t arena;

    if (monsters == NULL || monsters->records == NULL || monsters->count == 0U) {
        return 0;
    }
    arena = monsters->sourceArenaFNV1a;
    if (combatOwner.view.active == 0U ||
        combatOwner.view.sourceArenaFNV1a != arena) {
        memset(&combatOwner, 0, sizeof(combatOwner));
        combatOwner.view.sourceArenaFNV1a = arena;
        combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.view.weaponFamiliesOwned = 1U; /* ammo-free melee family */
        combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
        combatOwner.view.active = 1U;
        printf("[MONSTERCOMBAT] READY arena=%08x monsters=%u backend=type1-generic subtypes=0..13 weapons=table9 owned=ammo-free-melee specialDeathMask=%04x playerStats=fresh-native-16/12 legacyEntity=no\n",
               (unsigned int)arena,
               (unsigned int)monsters->count,
               (unsigned int)SPECIAL_DEATH_MASK);
    }
    expirePain();
    return 1;
}

void EspNativeGameplayMonsterCombat_reset(void) {
    memset(&combatOwner, 0, sizeof(combatOwner));
    combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
}

int EspNativeGameplayMonsterCombat_isReady(void) {
    return syncOwner();
}

const EspNativeGameplayMonsterCombatView* EspNativeGameplayMonsterCombat_view(void) {
    if (!syncOwner()) return NULL;
    combatOwner.view.pending = combatOwner.pending.active;
    combatOwner.view.pendingSpriteIndex = combatOwner.pending.active != 0U
                                              ? combatOwner.pending.spriteIndex
                                              : MONSTER_NO_SPRITE;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    return &combatOwner.view;
}

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

static int entityTypeInTraceMask(uint8_t type) {
    return type < 16U && (MONSTER_TRACE_MASK & (1U << type)) != 0U;
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
        if (!entityTypeInTraceMask(type)) continue;
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

static int findTraceSprite(uint16_t tile,
                           int32_t sourceX,
                           int32_t sourceY,
                           int32_t destX,
                           int32_t destY,
                           uint16_t* outSprite,
                           uint8_t* outType,
                           uint8_t* outSubtype) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t best = MONSTER_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint8_t bestType = 0xffU;
    uint8_t bestSubtype = 0xffU;
    uint32_t i;

    if (outSprite != NULL) *outSprite = MONSTER_NO_SPRITE;
    if (topology == NULL || outSprite == NULL || outType == NULL ||
        outSubtype == NULL) return -1;

    for (i = 0U; i < topology->spriteCount; ++i) {
        const EspNativeGameplayMonsterRecord* monster;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int specialBlocks;

        if (!EspNativeGameplayActionEngine_getEntity(
                i, &type, &subtype, &linkState, &linkOrder)) return -1;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !entityTypeInTraceMask(type)) {
            continue;
        }
        if (type == MONSTER_TYPE_ENEMY) {
            monster = EspNativeGameplayMonsterState_find((uint16_t)i);
            if (monster == NULL || monster->alive == 0U) continue;
        }
        if (type == 14U) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY,
                                                destX, destY);
            if (specialBlocks < 0) return -1;
            if (specialBlocks == 0) continue;
        }
        if (best == MONSTER_NO_SPRITE || linkOrder > bestOrder) {
            best = (uint16_t)i;
            bestOrder = linkOrder;
            bestType = type;
            bestSubtype = subtype;
        }
    }

    if (best == MONSTER_NO_SPRITE) return 0;
    *outSprite = best;
    *outType = bestType;
    *outSubtype = bestSubtype;
    return 1;
}

static int traceMonster(MonsterTarget* outTarget) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayTurnState* turn = EspNativeGameplayDispatch_view();
    int32_t sourceX;
    int32_t sourceY;
    uint32_t distance;

    if (outTarget == NULL || view == NULL || turn == NULL ||
        view->active != 1U || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        turn->active != 1U ||
        !((turn->viewStepX == TILE_SIZE && turn->viewStepY == 0) ||
          (turn->viewStepX == -TILE_SIZE && turn->viewStepY == 0) ||
          (turn->viewStepX == 0 && turn->viewStepY == TILE_SIZE) ||
          (turn->viewStepX == 0 && turn->viewStepY == -TILE_SIZE))) {
        return -1;
    }

    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->spriteIndex = MONSTER_NO_SPRITE;
    sourceX = view->destX;
    sourceY = view->destY;

    for (distance = 1U; distance <= MONSTER_TRACE_TILES; ++distance) {
        int32_t destX = view->destX + turn->viewStepX * (int32_t)distance;
        int32_t destY = view->destY + turn->viewStepY * (int32_t)distance;
        uint16_t tile;
        uint16_t spriteIndex;
        uint8_t type;
        uint8_t subtype;
        uint8_t tileFlags;
        int lineBlock;
        int spriteBlock;

        if (!tileIndexFor(destX, destY, &tile)) return 0;
        if (!EspMapState_getTileFlags(tile, &tileFlags)) return -1;
        if ((tileFlags & ESP_MAP_TILE_WALL) != 0U) return 0;

        lineBlock = closedLineBlocksTile(tile);
        if (lineBlock < 0) return -1;
        if (lineBlock > 0) return 0;

        spriteBlock = findTraceSprite(tile, sourceX, sourceY, destX, destY,
                                      &spriteIndex, &type, &subtype);
        if (spriteBlock < 0) return -1;
        if (spriteBlock > 0) {
            if (type != MONSTER_TYPE_ENEMY) return 0;
            outTarget->spriteIndex = spriteIndex;
            outTarget->tileIndex = tile;
            outTarget->subtype = subtype;
            outTarget->distance = (uint8_t)distance;
            outTarget->worldDistance =
                (uint32_t)(distance * TILE_SIZE) *
                (uint32_t)(distance * TILE_SIZE);
            return 1;
        }
        sourceX = destX;
        sourceY = destY;
    }
    return 0;
}

static int weaponRangeMinToDist(const WeaponSpec* weapon) {
    int range;
    if (weapon == NULL) return 0;
    range = (int)weapon->rangeMin * 64;
    return (range * range) + 4096;
}

static int calculateHit(DoomRPG_t* runtime,
                        const WeaponSpec* weapon,
                        const EspNativeGameplayMonsterRecord* target,
                        uint32_t worldDistance,
                        uint8_t* outRandHit,
                        int* outCalcHit,
                        int* outCritLimit,
                        uint32_t* ioRngCalls) {
    int agility;
    int calcHit;
    int decHit;
    int distance;
    int distStep;
    int dist2;
    uint8_t randHit;

    if (runtime == NULL || weapon == NULL || target == NULL ||
        outRandHit == NULL || outCalcHit == NULL || outCritLimit == NULL ||
        ioRngCalls == NULL) return -1;
    agility = p2Agility(target->param2);
    if (agility <= 0) return -1;

    calcHit = (((((PLAYER_FRESH_ACCURACY << 16) / (agility << 8)) * 128) >> 8) +
               (((int)weapon->rangeMax << 16) / 51200));
    dist2 = (int)worldDistance - weaponRangeMinToDist(weapon);
    *outRandHit = 0U;
    *outCalcHit = calcHit;
    *outCritLimit = (calcHit << 8) / 5120;

    if (dist2 > 0 && weapon->rangeMin == 0U) return 0;
    if (weapon->ammoType != 2U) {
        distance = 4096;
        distStep = 2;
        decHit = (calcHit * 76) >> 8;
        while (distance < dist2) {
            calcHit -= decHit;
            ++distStep;
            distance = (distStep * 64) * (distStep * 64);
        }
        *outCalcHit = calcHit;
        *outCritLimit = (calcHit << 8) / 5120;
    }

    randHit = DoomRPG_randNextByte(&runtime->random);
    ++(*ioRngCalls);
    *outRandHit = randHit;
    if ((int)randHit >= calcHit) return 0;
    return (int)randHit < *outCritLimit ? 2 : 1;
}

static int weaponDamageMultiplier(uint8_t weaponIndex,
                                  const WeaponSpec* weapon,
                                  uint8_t mType) {
    uint8_t ammoType;
    if (weapon == NULL) return 256;
    ammoType = weapon->ammoType;

    if (mType != 0U) {
        switch (mType) {
        case 1U:
            if (ammoType == 2U) return 409;
            break;
        case 2U:
            if (ammoType == 4U) return 512;
            if (weaponIndex == 0U) return 204;
            break;
        case 3U:
            if (ammoType == 2U) return 512;
            if (ammoType == 4U) return 165;
            break;
        case 4U:
            if (weaponIndex == 0U) return 191;
            if (ammoType == 0U) return 2550;
            break;
        case 5U:
            if (ammoType == 2U) return 382;
            if (ammoType == 3U) return 191;
            break;
        case 6U:
            if (weaponIndex == 0U) return 768;
            if (ammoType == 5U) return 140;
            break;
        case 7U:
            if (ammoType == 5U) return 637;
            if (ammoType == 1U) return 165;
            break;
        case 8U:
            if (weaponIndex == 0U) return 768;
            if (ammoType == 2U) return 140;
            break;
        case 9U:
            if (ammoType == 3U) return 512;
            if (ammoType == 5U) return 165;
            break;
        case 10U:
            if (ammoType == 0U) return 1530;
            if (ammoType == 1U) return 165;
            break;
        case 11U:
            if (ammoType == 2U) return 512;
            break;
        case 12U:
            if (ammoType == 3U) return 382;
            break;
        case 13U:
            if (ammoType == 4U) return 382;
            break;
        default:
            break;
        }
        return 256;
    }
    return weaponIndex == 0U ? 768 : 204;
}

static int calculateDamage(DoomRPG_t* runtime,
                           uint8_t weaponIndex,
                           const WeaponSpec* weapon,
                           const EspNativeGameplayMonsterRecord* target,
                           int attackScale,
                           uint32_t worldDistance,
                           uint8_t* outRandStrength,
                           int* outDamage,
                           int* outArmorDamage,
                           uint32_t* ioRngCalls) {
    int randStr;
    int strength;
    int defense;
    int multiplier;
    int calDmg;
    int calDmgArm;
    int distance;
    int distStep;
    int decDmg;
    int weaponRoll;

    if (runtime == NULL || weapon == NULL || target == NULL ||
        outRandStrength == NULL || outDamage == NULL ||
        outArmorDamage == NULL || ioRngCalls == NULL) return 0;
    defense = p2Defense(target->param2) << 8;
    if (defense <= 0) return 0;

    randStr = DoomRPG_randNextByte(&runtime->random);
    ++(*ioRngCalls);
    *outRandStrength = (uint8_t)randStr;
    strength = PLAYER_FRESH_STRENGTH << 16;
    multiplier = weaponDamageMultiplier(weaponIndex, weapon, target->mType);
    weaponRoll = ((int)weapon->strMin << 8) +
                 ((randStr * (((int)weapon->strMax -
                               (int)weapon->strMin) << 8)) >> 8);

    calDmg = (((((((weaponRoll * (strength / defense)) >> 8) *
                  attackScale) >> 8) * multiplier) >> 8));

    distance = 4096;
    distStep = 2;
    decDmg = (calDmg * 76) >> 8;
    while (distance < (int)worldDistance - weaponRangeMinToDist(weapon)) {
        calDmg -= decDmg;
        ++distStep;
        distance = (distStep * 64) * (distStep * 64);
    }
    if (calDmg < 256) calDmg = 256;
    else if (calDmg > 255744) calDmg = 255744;

    calDmgArm = (calDmg * (int)weapon->damage) >> 8;
    *outArmorDamage = (calDmgArm + 128) >> 8;
    *outDamage = ((calDmg - calDmgArm) + 128) >> 8;
    return 1;
}

static int monsterExp(const EspNativeGameplayMonsterRecord* record) {
    if (record == NULL) return 0;
    return ((((int)p2Defense(record->param2) +
              (int)p2Strength(record->param2)) * 5) +
            (((int)p2Agility(record->param2) +
              (int)p2Accuracy(record->param2)) * 3) +
            (((int)p1MaxHealth(record->param1) +
              (int)p1MaxArmor(record->param1)) * 5) + 49) / 50;
}

static uint16_t consumePainSound(DoomRPG_t* runtime,
                                 uint8_t subtype,
                                 uint32_t* ioRngCalls) {
    if (runtime == NULL || ioRngCalls == NULL ||
        subtype >= MONSTER_SUBTYPE_COUNT || painSounds[subtype] == 0U) {
        return 0U;
    }
    (void)DoomRPG_randNextByte(&runtime->random); /* modulo 1 in legacy table */
    ++(*ioRngCalls);
    return painSounds[subtype];
}

static uint16_t consumeDeathSound(DoomRPG_t* runtime,
                                  uint8_t subtype,
                                  uint32_t* ioRngCalls) {
    uint32_t count = 0U;
    uint8_t pick;
    if (runtime == NULL || ioRngCalls == NULL || subtype >= MONSTER_SUBTYPE_COUNT) {
        return 0U;
    }
    while (count < 3U && deathSounds[subtype][count] != 0U) ++count;
    if (count == 0U) return 0U;
    pick = DoomRPG_randNextByte(&runtime->random);
    ++(*ioRngCalls);
    return deathSounds[subtype][pick % count];
}

static void setPainResult(EspNativeGameplayMonsterRecord* record,
                          int damage,
                          int armorDamage,
                          int* outHealthAfter,
                          int* outArmorAfter) {
    uint32_t p;
    int armor;
    int health;
    int healthDamage;

    p = record->param1;
    armor = p1Armor(p);
    health = p1Health(p);
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

    record->param1 = (record->param1 & 0xff00ff00U) |
                     (uint32_t)(uint8_t)health |
                     ((uint32_t)(uint8_t)armor << 16);
    if (outHealthAfter != NULL) *outHealthAfter = health;
    if (outArmorAfter != NULL) *outArmorAfter = armor;
}

static int specialDeath(uint8_t subtype) {
    return subtype < 16U && (SPECIAL_DEATH_MASK & (1U << subtype)) != 0U;
}

static void logFrame(const MonsterCombatPending* pending,
                     const char* phase,
                     const EspNativeGameplayFrameStats* frame) {
    if (pending == NULL || phase == NULL || frame == NULL) return;
    printf("[MONSTERCOMBAT] FRAME seq=%u sprite=%u subtype=%u weapon=%u phase=%s frame=%08x worldUs=%u spriteUs=%u hudUs=%u presentUs=%u totalUs=%u presented=%u\n",
           (unsigned int)pending->sequence,
           (unsigned int)pending->spriteIndex,
           (unsigned int)pending->subtype,
           (unsigned int)pending->weapon,
           phase,
           (unsigned int)frame->frameAfterFNV,
           (unsigned int)frame->worldMicros,
           (unsigned int)frame->spriteMicros,
           (unsigned int)frame->hudMicros,
           (unsigned int)frame->presentMicros,
           (unsigned int)frame->totalMicros,
           (unsigned int)frame->finalPresented);
}

static int servicePending(DoomRPG_t* runtime) {
    const EspPlayerViewState* playerView = EspPlayerView_view();
    EspNativeGameplayMonsterRecord* target;
    EspNativeGameplayMonsterRecord targetBefore;
    MonsterCombatOwner ownerBefore;
    MonsterCombatPending pending;
    Random_t randomBefore;
    EspNativeGameplayFrameStats attackFrame;
    EspNativeGameplayFrameStats rollbackFrame;
    EspNativeGameplayFrameStats settleFrame;
    const WeaponSpec* weapon;
    uint32_t rngCalls = 0U;
    uint32_t stateBeforeFNV;
    uint32_t stateAfterFNV;
    uint32_t dropRoll = 0U;
    uint8_t randHit = 0U;
    uint8_t randStrength = 0U;
    uint16_t consequenceSound = 0U;
    int calcHit = 0;
    int critLimit = 0;
    int hitType;
    int damage = 0;
    int armorDamage = 0;
    int healthBefore;
    int armorBefore;
    int healthAfter;
    int armorAfter;
    int xp = 0;
    int lethal = 0;

    if (runtime == NULL || runtime->render == NULL || playerView == NULL ||
        playerView->active != 1U || combatOwner.pending.active == 0U ||
        !syncOwner()) return 0;

    pending = combatOwner.pending;
    if (pending.weapon >= (uint8_t)(sizeof(playerWeapons) / sizeof(playerWeapons[0]))) {
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        return 1;
    }
    weapon = &playerWeapons[pending.weapon];
    target = EspNativeGameplayMonsterState_findMutable(pending.spriteIndex);
    if (target == NULL || target->alive == 0U ||
        target->subtype != pending.subtype || target->subtype >= MONSTER_SUBTYPE_COUNT) {
        printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u reason=stale-target mutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        return 1;
    }

    targetBefore = *target;
    ownerBefore = combatOwner;
    randomBefore = runtime->random;
    stateBeforeFNV = currentMonsterFNV();
    healthBefore = p1Health(target->param1);
    armorBefore = p1Armor(target->param1);
    healthAfter = healthBefore;
    armorAfter = armorBefore;

    if (!EspNativeGameplayWeapon_armAttack(pending.weapon)) {
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=weapon-attack-arm mutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
        return 0;
    }

    hitType = calculateHit(runtime, weapon, target, pending.worldDistance,
                           &randHit, &calcHit, &critLimit, &rngCalls);
    if (hitType < 0) {
        runtime->random = randomBefore;
        EspNativeGameplayWeapon_cancelAttack();
        combatOwner = ownerBefore;
        combatOwner.pending.active = 0U;
        printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=hit-contract rngRollback=yes mutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
        return 1;
    }

    if (hitType != 0) {
        int attackScale = hitType == 2 ? (PLAYER_BERSERK_SCALE * 512 >> 8)
                                       : PLAYER_BERSERK_SCALE;
        if (!calculateDamage(runtime, pending.weapon, weapon, target,
                             attackScale, pending.worldDistance,
                             &randStrength, &damage, &armorDamage, &rngCalls)) {
            runtime->random = randomBefore;
            EspNativeGameplayWeapon_cancelAttack();
            combatOwner = ownerBefore;
            combatOwner.pending.active = 0U;
            printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=damage-contract rngRollback=yes mutation=no\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex);
            return 1;
        }
        setPainResult(target, damage, armorDamage, &healthAfter, &armorAfter);
        lethal = healthAfter <= 0;

        if (lethal && specialDeath(target->subtype)) {
            *target = targetBefore;
            runtime->random = randomBefore;
            EspNativeGameplayWeapon_cancelAttack();
            combatOwner = ownerBefore;
            combatOwner.pending.active = 0U;
            printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u subtype=%u reason=special-death-family prospectiveDamage=%d+%d hp=%d->0 rngRollback=yes mutation=no\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)targetBefore.subtype,
                   damage,
                   armorDamage,
                   healthBefore);
            return 1;
        }

        if (lethal) {
            target->alive = 0U;
            consequenceSound = consumeDeathSound(runtime, target->subtype, &rngCalls);
            dropRoll = (uint32_t)DoomRPG_randNextInt(&runtime->random);
            ++rngCalls;
            xp = monsterExp(&targetBefore);
            combatOwner.view.deferredXp += (uint32_t)xp;
            ++combatOwner.view.kills;
            combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
            combatOwner.painUntilMs = 0U;
        }
        else {
            consequenceSound = consumePainSound(runtime, target->subtype, &rngCalls);
            combatOwner.view.painSpriteIndex = pending.spriteIndex;
            combatOwner.painUntilMs = DoomRPG_GetUpTimeMS() + MONSTER_PAIN_MS;
        }
        ++combatOwner.view.hits;
        if (hitType == 2) ++combatOwner.view.crits;
    }
    else {
        ++combatOwner.view.misses;
    }
    ++combatOwner.view.attacks;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    stateAfterFNV = combatOwner.view.currentMonsterFNV1a;

    printf("[MONSTERCOMBAT] ROLL seq=%u sprite=%u subtype=%u mType=%u weapon=%u distance=%u worldDist=%u randHit=%s%u calcHit=%d critLimit=%d result=%s randDamage=%s%u damage=%d armorDamage=%d rngCalls=%u\n",
           (unsigned int)pending.sequence,
           (unsigned int)pending.spriteIndex,
           (unsigned int)targetBefore.subtype,
           (unsigned int)targetBefore.mType,
           (unsigned int)pending.weapon,
           (unsigned int)pending.distance,
           (unsigned int)pending.worldDistance,
           rngCalls == 0U ? "unused/" : "value/",
           (unsigned int)randHit,
           calcHit,
           critLimit,
           hitType == 2 ? "CRIT" : (hitType == 1 ? "HIT" : "MISS"),
           hitType == 0 ? "unused/" : "value/",
           (unsigned int)randStrength,
           damage,
           armorDamage,
           (unsigned int)rngCalls);

    memset(&attackFrame, 0, sizeof(attackFrame));
    if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                           (uint8_t)playerView->viewAngle,
                                           &attackFrame)) {
        *target = targetBefore;
        runtime->random = randomBefore;
        combatOwner = ownerBefore;
        combatOwner.pending.active = 0U;
        EspNativeGameplayWeapon_cancelAttack();
        memset(&rollbackFrame, 0, sizeof(rollbackFrame));
        if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                               (uint8_t)playerView->viewAngle,
                                               &rollbackFrame)) {
            printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=render+rollback-render rngRollback=yes monsterRollback=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex);
            return 0;
        }
        printf("[MONSTERCOMBAT] ROLLBACK seq=%u sprite=%u rng=yes monster=yes frame=%08x\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex,
               (unsigned int)rollbackFrame.frameAfterFNV);
        return 1;
    }

    logFrame(&pending, "attack", &attackFrame);
    printf("[MONSTERCOMBAT] COMMIT seq=%u sprite=%u subtype=%u hp=%d->%d armor=%d->%d alive=%u->%u stateFNV=%08x->%08x visual=%s attackSound=%u-deferred consequenceSound=%u-deferred xp=%d-deferred xpDeferredTotal=%u dropRoll=%s%08x dropMaterialize=deferred turnAdvance=deferred AI=deferred rollback=closed\n",
           (unsigned int)pending.sequence,
           (unsigned int)pending.spriteIndex,
           (unsigned int)targetBefore.subtype,
           healthBefore,
           healthAfter,
           armorBefore,
           armorAfter,
           (unsigned int)targetBefore.alive,
           (unsigned int)target->alive,
           (unsigned int)stateBeforeFNV,
           (unsigned int)stateAfterFNV,
           lethal ? "death4+unlink" : (hitType == 0 ? "none" : "pain6/250ms"),
           pending.weapon == 0U ? 5136U : weapon->resourceId,
           (unsigned int)consequenceSound,
           xp,
           (unsigned int)combatOwner.view.deferredXp,
           lethal ? "value/" : "unused/",
           (unsigned int)dropRoll);

    memset(&settleFrame, 0, sizeof(settleFrame));
    if (EspNativeGameplayFrame_renderTurn(runtime->render,
                                          (uint8_t)playerView->viewAngle,
                                          &settleFrame)) {
        logFrame(&pending, "settle-idle", &settleFrame);
        printf("[MONSTERCOMBAT] ATTACK seq=%u weapon=%u frame=1->0 genericMonster=yes worldCommitted=yes\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.weapon);
    }
    else {
        EspNativeGameplayWeapon_cancelAttack();
        printf("[MONSTERCOMBAT] SETTLE-FAILED seq=%u sprite=%u worldCommitted=yes recovery=next-full-redraw\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
    }

    memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
    combatOwner.view.pending = 0U;
    combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    return 1;
}

EspNativeGameplayActionStatus __wrap_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status;
    const EspNativeGameplayHudState* hud;
    const EspNativeGameplayMonsterRecord* monster;
    MonsterTarget target;
    uint8_t weaponIndex;
    int traceStatus;

    status = __real_EspNativeGameplayActionEngine_executeSelect(intent, outResult);
    if ((status != ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT &&
         status != ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE) ||
        intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->pending != 1U || intent->active != 1U || !syncOwner() ||
        combatOwner.pending.active != 0U) {
        return status;
    }

    hud = EspNativeGameplayHud_view();
    if (hud == NULL || hud->active != 1U || hud->painted != 1U) return status;
    weaponIndex = hud->model.weapon;
    if (weaponIndex >= (uint8_t)(sizeof(playerWeapons) / sizeof(playerWeapons[0]))) {
        return status;
    }

    memset(&target, 0, sizeof(target));
    target.spriteIndex = MONSTER_NO_SPRITE;
    traceStatus = traceMonster(&target);
    if (traceStatus <= 0) return status;

    monster = EspNativeGameplayMonsterState_find(target.spriteIndex);
    if (monster == NULL || monster->alive == 0U ||
        monster->subtype != target.subtype) return status;

    if (playerWeapons[weaponIndex].ammoUsage != 0U) {
        printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u subtype=%u weapon=%u reason=ammo-owner-not-landed backend=generic weaponTable=yes monsterFamilyOwned=yes mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.subtype,
               (unsigned int)weaponIndex);
        return status;
    }

    memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
    combatOwner.pending.sequence = intent->sequence;
    combatOwner.pending.spriteIndex = target.spriteIndex;
    combatOwner.pending.tileIndex = target.tileIndex;
    combatOwner.pending.worldDistance = target.worldDistance;
    combatOwner.pending.subtype = target.subtype;
    combatOwner.pending.distance = target.distance;
    combatOwner.pending.weapon = weaponIndex;
    combatOwner.pending.active = 1U;
    combatOwner.view.pending = 1U;
    combatOwner.view.pendingSpriteIndex = target.spriteIndex;

    printf("[MONSTERCOMBAT] ARM seq=%u sprite=%u tile=%u subtype=%u mType=%u weapon=%u distance=%u hp=%u/%u armor=%u/%u def=%u agi=%u backend=generic-type1 rng=pending mutation=no rollback=armed\n",
           (unsigned int)intent->sequence,
           (unsigned int)target.spriteIndex,
           (unsigned int)target.tileIndex,
           (unsigned int)monster->subtype,
           (unsigned int)monster->mType,
           (unsigned int)weaponIndex,
           (unsigned int)target.distance,
           (unsigned int)p1Health(monster->param1),
           (unsigned int)p1MaxHealth(monster->param1),
           (unsigned int)p1Armor(monster->param1),
           (unsigned int)p1MaxArmor(monster->param1),
           (unsigned int)p2Defense(monster->param2),
           (unsigned int)p2Agility(monster->param2));
    return status;
}

int __wrap_EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayActionEngine_getVisualState(spriteIndex,
                                                       outVisualState)) {
        return 0;
    }
    if (outVisualState == NULL || !syncOwner()) return 1;
    monster = EspNativeGameplayMonsterState_find((uint16_t)spriteIndex);
    if (monster == NULL) return 1;
    if (monster->alive == 0U) {
        *outVisualState = (uint8_t)((*outVisualState & 0xf0U) |
                                    MONSTER_DEATH_VISUAL);
    }
    else if (combatOwner.view.painSpriteIndex == spriteIndex) {
        *outVisualState = (uint8_t)((*outVisualState & 0xf0U) |
                                    MONSTER_PAIN_VISUAL);
    }
    return 1;
}

int __wrap_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayActionEngine_getEntity(spriteIndex, outType, outSubType,
                                                  outLinkState, outLinkOrder)) {
        return 0;
    }
    if (outType == NULL || *outType != MONSTER_TYPE_ENEMY || !syncOwner()) {
        return 1;
    }
    monster = EspNativeGameplayMonsterState_find((uint16_t)spriteIndex);
    if (monster != NULL && monster->alive == 0U) {
        if (outLinkState != NULL) {
            *outLinkState &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                                         ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
        }
        if (outLinkOrder != NULL) *outLinkOrder = 0U;
    }
    return 1;
}

int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* runtime) {
    if (!EspNativeGameplayMonsterState_actionService(runtime)) return 0;
    if (!syncOwner()) return 0;
    if (combatOwner.pending.active != 0U) {
        return servicePending(runtime);
    }
    return 1;
}

void __wrap_EspNativeGameplayActionEngine_reset(void) {
    EspNativeGameplayMonsterCombat_reset();
    EspNativeGameplayMonsterState_actionReset();
}
