#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_player_view_state.h"

#define DOG_PROBE_SPRITE 179U
#define DOG_PROBE_TILE 750U
#define DOG_PROBE_DEF_TILE 20U
#define DOG_PROBE_SUBTYPE 1U
#define DOG_PROBE_MTYPE 1U
#define DOG_PROBE_TRACE_MASK 0x5687U

#define DOG_PROBE_PLAYER_ACCURACY 16
#define DOG_PROBE_PLAYER_STRENGTH 12
#define DOG_PROBE_WEAPON_AXE 0U
#define DOG_PROBE_AXE_STR_MIN 3
#define DOG_PROBE_AXE_STR_MAX 12
#define DOG_PROBE_AXE_RANGE_MIN 0
#define DOG_PROBE_AXE_RANGE_MAX 70
#define DOG_PROBE_AXE_DAMAGE_SPLIT 25
#define DOG_PROBE_AXE_DAMAGE_MULTIPLIER 256
#define DOG_PROBE_WORLD_DISTANCE 4096
#define DOG_PROBE_EXPECTED_CALC_HIT 293
#define DOG_PROBE_EXPECTED_CRIT_LIMIT 14

#define TILE_SIZE 64
#define TILE_CENTER 32
#define MAP_WIDTH 32
#define MAP_MAX_CENTER (((MAP_WIDTH - 1) * TILE_SIZE) + TILE_CENTER)

typedef struct DogProbePending_s {
    uint32_t sequence;
    uint16_t frontTile;
    uint16_t spriteIndex;
    uint8_t active;
} DogProbePending;

static DogProbePending dogProbePending;

EspNativeGameplayActionStatus __real_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);
int __real_EspNativeGameplayMonsterState_ensure(DoomRPG_t* doomRpg);

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

static int cardinalStep(const EspNativeGameplayTurnState* turn) {
    return turn != NULL && turn->active == 1U &&
           ((turn->viewStepX == TILE_SIZE && turn->viewStepY == 0) ||
            (turn->viewStepX == -TILE_SIZE && turn->viewStepY == 0) ||
            (turn->viewStepX == 0 && turn->viewStepY == TILE_SIZE) ||
            (turn->viewStepX == 0 && turn->viewStepY == -TILE_SIZE));
}

static int typeInTraceMask(uint8_t type) {
    return type < 16U && (DOG_PROBE_TRACE_MASK & (1U << type)) != 0U;
}

static int witnessIsAdjacent(uint16_t* outTile) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayTurnState* turn = EspNativeGameplayDispatch_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t frontTile;
    uint16_t bestSprite = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint8_t bestType = 0xffU;
    uint8_t bestSubtype = 0xffU;
    uint32_t i;

    if (outTile != NULL) *outTile = 0xffffU;
    if (view == NULL || turn == NULL || topology == NULL ||
        view->active != 1U || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        !cardinalStep(turn) ||
        !tileIndexFor(view->destX + turn->viewStepX,
                      view->destY + turn->viewStepY, &frontTile)) {
        return 0;
    }
    if (frontTile != DOG_PROBE_TILE) return 0;

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            return 0;
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != frontTile ||
            !typeInTraceMask(type)) {
            continue;
        }
        if (bestSprite == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            linkOrder > bestOrder) {
            bestSprite = (uint16_t)i;
            bestOrder = linkOrder;
            bestType = type;
            bestSubtype = subtype;
        }
    }

    if (bestSprite != DOG_PROBE_SPRITE || bestType != ESP_MAP_ENTITY_TYPE_ENEMY ||
        bestSubtype != DOG_PROBE_SUBTYPE) {
        return 0;
    }
    if (outTile != NULL) *outTile = frontTile;
    return 1;
}

static int witnessRecordExact(const EspNativeGameplayMonsterRecord* record) {
    return record != NULL &&
           record->spriteIndex == DOG_PROBE_SPRITE &&
           record->defTile == DOG_PROBE_DEF_TILE &&
           record->subtype == DOG_PROBE_SUBTYPE &&
           record->mType == DOG_PROBE_MTYPE &&
           record->alternateAttack == 0U && record->alive == 1U &&
           p1Health(record->param1) == 6U &&
           p1MaxHealth(record->param1) == 6U &&
           p1Armor(record->param1) == 2U &&
           p1MaxArmor(record->param1) == 2U &&
           p2Defense(record->param2) == 10U &&
           p2Strength(record->param2) == 12U &&
           p2Agility(record->param2) == 10U &&
           p2Accuracy(record->param2) == 10U;
}

static int calculateHit(const EspNativeGameplayMonsterRecord* record,
                        uint8_t randHit,
                        int* outCalcHit,
                        int* outCritLimit) {
    int agility;
    int calcHit;
    int critLimit;

    if (record == NULL || outCalcHit == NULL || outCritLimit == NULL) return -1;
    agility = p2Agility(record->param2);
    if (agility <= 0) return -1;

    calcHit = (((((DOG_PROBE_PLAYER_ACCURACY << 16) /
                  (agility << 8)) * 128) >> 8) +
               ((DOG_PROBE_AXE_RANGE_MAX << 16) / 51200));
    critLimit = (calcHit << 8) / 5120;
    *outCalcHit = calcHit;
    *outCritLimit = critLimit;
    if ((int)randHit >= calcHit) return 0;
    return (int)randHit < critLimit ? 2 : 1;
}

static int calculateDamage(const EspNativeGameplayMonsterRecord* record,
                           uint8_t randStrength,
                           int critical,
                           int* outDamage,
                           int* outArmorDamage) {
    int defense;
    int strength;
    int weaponRoll;
    int attackScale;
    int calDmg;
    int calDmgArm;

    if (record == NULL || outDamage == NULL || outArmorDamage == NULL) return 0;
    defense = p2Defense(record->param2) << 8;
    strength = DOG_PROBE_PLAYER_STRENGTH << 16;
    if (defense <= 0) return 0;

    weaponRoll = (DOG_PROBE_AXE_STR_MIN << 8) +
                 (((int)randStrength *
                   ((DOG_PROBE_AXE_STR_MAX - DOG_PROBE_AXE_STR_MIN) << 8)) >> 8);
    attackScale = critical ? 512 : 256;
    calDmg = (((((((weaponRoll * (strength / defense)) >> 8) *
                  attackScale) >> 8) * DOG_PROBE_AXE_DAMAGE_MULTIPLIER) >> 8));

    if (calDmg < 256) calDmg = 256;
    else if (calDmg > 255744) calDmg = 255744;

    calDmgArm = (calDmg * DOG_PROBE_AXE_DAMAGE_SPLIT) >> 8;
    *outArmorDamage = (calDmgArm + 128) >> 8;
    *outDamage = ((calDmg - calDmgArm) + 128) >> 8;
    return 1;
}

static int dogExp(const EspNativeGameplayMonsterRecord* record) {
    if (record == NULL) return 0;
    return ((((int)p2Defense(record->param2) +
              (int)p2Strength(record->param2)) * 5) +
            (((int)p2Agility(record->param2) +
              (int)p2Accuracy(record->param2)) * 3) +
            (((int)p1MaxHealth(record->param1) +
              (int)p1MaxArmor(record->param1)) * 5) + 49) / 50;
}

static void runProbe(DoomRPG_t* doomRpg) {
    const EspNativeGameplayMonsterRecord* record;
    Random_t randomBefore;
    uint32_t rngBefore;
    uint32_t rngConsumed;
    uint32_t rngAfter;
    uint8_t randHit;
    uint8_t randDamage = 0U;
    int hitType;
    int calcHit = 0;
    int critLimit = 0;
    int damage = 0;
    int armorDamage = 0;
    int healthBefore;
    int armorBefore;
    int healthAfter;
    int armorAfter;
    int appliedHealthDamage;
    int lethal = 0;
    int xp = 0;
    uint32_t rngCalls = 1U;

    if (doomRpg == NULL || dogProbePending.active == 0U) return;
    record = EspNativeGameplayMonsterState_find(dogProbePending.spriteIndex);
    if (!witnessRecordExact(record)) {
        printf("[DOGCOMBATPROBE] FAILED seq=%u reason=witness-state-drift mutation=no rngConsumed=0\n",
               (unsigned int)dogProbePending.sequence);
        memset(&dogProbePending, 0, sizeof(dogProbePending));
        return;
    }

    randomBefore = doomRpg->random;
    rngBefore = randomFNV(&randomBefore);
    randHit = DoomRPG_randNextByte(&doomRpg->random);
    hitType = calculateHit(record, randHit, &calcHit, &critLimit);
    if (calcHit != DOG_PROBE_EXPECTED_CALC_HIT ||
        critLimit != DOG_PROBE_EXPECTED_CRIT_LIMIT || hitType < 0) {
        doomRpg->random = randomBefore;
        printf("[DOGCOMBATPROBE] FAILED seq=%u reason=hit-contract calc=%d critLimit=%d randHit=%u rngRollback=%s mutation=no\n",
               (unsigned int)dogProbePending.sequence,
               calcHit,
               critLimit,
               (unsigned int)randHit,
               memcmp(&doomRpg->random, &randomBefore, sizeof(randomBefore)) == 0
                   ? "yes" : "NO");
        memset(&dogProbePending, 0, sizeof(dogProbePending));
        return;
    }

    healthBefore = p1Health(record->param1);
    armorBefore = p1Armor(record->param1);
    healthAfter = healthBefore;
    armorAfter = armorBefore;

    if (hitType != 0) {
        randDamage = DoomRPG_randNextByte(&doomRpg->random);
        ++rngCalls;
        if (!calculateDamage(record, randDamage, hitType == 2,
                             &damage, &armorDamage)) {
            doomRpg->random = randomBefore;
            printf("[DOGCOMBATPROBE] FAILED seq=%u reason=damage-contract rngRollback=%s mutation=no\n",
                   (unsigned int)dogProbePending.sequence,
                   memcmp(&doomRpg->random, &randomBefore, sizeof(randomBefore)) == 0
                       ? "yes" : "NO");
            memset(&dogProbePending, 0, sizeof(dogProbePending));
            return;
        }

        appliedHealthDamage = damage;
        if (armorBefore < armorDamage) {
            appliedHealthDamage += armorDamage - armorBefore;
            armorAfter = 0;
        }
        else {
            armorAfter = armorBefore - armorDamage;
        }
        healthAfter = healthBefore - appliedHealthDamage;
        if (healthAfter < 0) healthAfter = 0;
        lethal = healthAfter <= 0;
        if (lethal) xp = dogExp(record);
    }

    rngConsumed = randomFNV(&doomRpg->random);
    doomRpg->random = randomBefore;
    rngAfter = randomFNV(&doomRpg->random);

    printf("[DOGCOMBATPROBE] ROLL seq=%u sprite=%u tile=%u weapon=0 distance=1 randHit=%u calcHit=%d critLimit=%d result=%s randDamage=%s%u rngCalls=%u\n",
           (unsigned int)dogProbePending.sequence,
           (unsigned int)dogProbePending.spriteIndex,
           (unsigned int)dogProbePending.frontTile,
           (unsigned int)randHit,
           calcHit,
           critLimit,
           hitType == 2 ? "CRIT" : (hitType == 1 ? "HIT" : "MISS"),
           hitType == 0 ? "unused/" : "value/",
           (unsigned int)randDamage,
           (unsigned int)rngCalls);
    printf("[DOGCOMBATPROBE] RESULT seq=%u hp=%d->%d armor=%d->%d damage=%d armorDamage=%d lethal=%s xp=%d-deferred visual=%s sound=5136-deferred drop=deferred turnAdvance=deferred rng=%08x->%08x->%08x rollbackRng=%s monsterExact=%s mutation=no\n",
           (unsigned int)dogProbePending.sequence,
           healthBefore,
           healthAfter,
           armorBefore,
           armorAfter,
           damage,
           armorDamage,
           lethal ? "yes" : "no",
           xp,
           lethal ? "death4-candidate" : (hitType == 0 ? "none" : "pain6-candidate"),
           (unsigned int)rngBefore,
           (unsigned int)rngConsumed,
           (unsigned int)rngAfter,
           memcmp(&doomRpg->random, &randomBefore, sizeof(randomBefore)) == 0
               ? "yes" : "NO",
           witnessRecordExact(record) ? "yes" : "NO");

    memset(&dogProbePending, 0, sizeof(dogProbePending));
}

EspNativeGameplayActionStatus __wrap_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status;
    const EspNativeGameplayHudState* hud;
    const EspNativeGameplayMonsterRecord* record;
    uint16_t frontTile;

    status = __real_EspNativeGameplayActionEngine_executeSelect(intent, outResult);
    if ((status != ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT &&
         status != ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE) ||
        intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->pending != 1U || intent->active != 1U ||
        dogProbePending.active != 0U ||
        !EspNativeGameplayMonsterState_isReady()) {
        return status;
    }

    hud = EspNativeGameplayHud_view();
    record = EspNativeGameplayMonsterState_find(DOG_PROBE_SPRITE);
    if (hud == NULL || hud->active != 1U || hud->painted != 1U ||
        hud->model.weapon != DOG_PROBE_WEAPON_AXE ||
        !witnessRecordExact(record) || !witnessIsAdjacent(&frontTile)) {
        return status;
    }

    memset(&dogProbePending, 0, sizeof(dogProbePending));
    dogProbePending.sequence = intent->sequence;
    dogProbePending.frontTile = frontTile;
    dogProbePending.spriteIndex = DOG_PROBE_SPRITE;
    dogProbePending.active = 1U;
    printf("[DOGCOMBATPROBE] ARM seq=%u sprite=%u tile=%u subtype=1 weapon=0 distance=1 hp=6 armor=2 def=10 agi=10 calcHit=293 critLimit=14 rng=pending mutation=no rollback=required\n",
           (unsigned int)intent->sequence,
           (unsigned int)DOG_PROBE_SPRITE,
           (unsigned int)frontTile);
    return status;
}

int __wrap_EspNativeGameplayMonsterState_ensure(DoomRPG_t* doomRpg) {
    int ready = __real_EspNativeGameplayMonsterState_ensure(doomRpg);
    if (!ready) {
        memset(&dogProbePending, 0, sizeof(dogProbePending));
        return 0;
    }
    if (dogProbePending.active != 0U) runProbe(doomRpg);
    return 1;
}
