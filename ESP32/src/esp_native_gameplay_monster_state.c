#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomRPG.h"

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_monster_state.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_MAX_COUNT 1024U
#define ENEMY_DEF_TEMP_CAPACITY 64U
#define ENTITY_TYPE_ENEMY 1U
#define ENTITY_SUBTYPE_COUNT 14U
#define MAP_SPRITE_TILE_FLAG 0x00040000UL
#define MAP_SPRITE_DEF_MASK 511U
#define MAP_SPRITE_TILE_DEF_BASE 305U
#define DOG_WITNESS_SPRITE 179U

typedef struct EnemyTemplate_s {
    uint16_t maxHealth;
    uint16_t maxArmor;
    uint8_t defense;
    uint8_t strength;
    uint8_t agility;
    uint8_t accuracy;
} EnemyTemplate;

typedef struct EnemyDefTemp_s {
    uint32_t parm;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t reserved;
} EnemyDefTemp;

static const EnemyTemplate enemyTemplates[ENTITY_SUBTYPE_COUNT] = {
    {5U,   4U,   13U, 13U, 13U, 13U},
    {7U,   3U,   12U, 14U, 12U, 12U},
    {10U,  5U,   14U,  7U, 13U, 12U},
    {9U,   4U,   13U, 14U, 13U, 13U},
    {6U,   3U,   14U,  6U, 15U, 15U},
    {10U,  5U,   14U, 16U, 12U, 13U},
    {13U,  7U,   12U, 10U, 13U, 15U},
    {15U,  8U,   14U, 12U, 12U, 12U},
    {13U,  8U,   16U, 14U, 15U, 15U},
    {20U, 10U,   16U, 14U, 15U, 15U},
    {18U,  9U,   15U, 17U, 14U, 16U},
    {25U, 15U,   16U, 15U, 15U, 15U},
    {600U, 400U, 45U, 30U, 35U, 40U},
    {400U, 250U, 30U, 35U, 40U, 55U}
};

static EspNativeGameplayMonsterRecord* monsterRecords;
static EspNativeGameplayMonsterView monsterView;

_Static_assert(sizeof(EspNativeGameplayMonsterRecord) == 16U,
               "native monster record must remain 16 bytes");
_Static_assert(sizeof(EnemyDefTemp) == 8U,
               "temporary enemy def record must remain 8 bytes");

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint8_t clampByte(int32_t value) {
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1MaxHealth(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p1MaxArmor(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

static uint32_t packParam1(uint8_t health,
                           uint8_t maxHealth,
                           uint8_t armor,
                           uint8_t maxArmor) {
    return (uint32_t)health |
           ((uint32_t)maxHealth << 8) |
           ((uint32_t)armor << 16) |
           ((uint32_t)maxArmor << 24);
}

static uint32_t packParam2(uint8_t defense,
                           uint8_t strength,
                           uint8_t agility,
                           uint8_t accuracy) {
    return (uint32_t)defense |
           ((uint32_t)strength << 8) |
           ((uint32_t)agility << 16) |
           ((uint32_t)accuracy << 24);
}

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

static uint32_t randomFNV(const Random_t* random) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    uint32_t next;

    if (random == NULL) return 0U;
    for (i = 0U; i < RANDTABLESIZE; ++i) {
        hash = fnvByte(hash, random->randTable[i]);
    }
    next = (uint32_t)random->nextRand;
    return fnv32(hash, next);
}

static uint32_t recordsFNV(void) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    const EspNativeGameplayMonsterRecord* record;

    if (monsterRecords == NULL) return 0U;
    for (i = 0U; i < monsterView.count; ++i) {
        record = &monsterRecords[i];
        hash = fnv32(hash, record->param1);
        hash = fnv32(hash, record->param2);
        hash = fnv16(hash, record->spriteIndex);
        hash = fnv16(hash, record->defTile);
        hash = fnvByte(hash, record->subtype);
        hash = fnvByte(hash, record->mType);
        hash = fnvByte(hash, record->alternateAttack);
        hash = fnvByte(hash, record->alive);
    }
    return hash;
}

static int32_t scaledRandomValue(DoomRPG_t* doomRpg,
                                 int32_t parm,
                                 uint32_t* rngCalls) {
    int32_t i2;
    int32_t i3;
    int32_t randomByte;

    i2 = (int32_t)(((int64_t)parm * 256LL) / 1024LL);
    i3 = (int32_t)(((int64_t)i2 * 512LL) >> 8);
    randomByte = DoomRPG_randNextInt(&doomRpg->random) & 255;
    ++(*rngCalls);
    return parm + (int32_t)(((int64_t)i3 * randomByte) >> 8) - i2;
}

static uint8_t scaledStat(uint8_t base, int32_t scale) {
    int32_t value = (int32_t)(((int64_t)base * 256LL * scale) >> 16);
    return clampByte(value);
}

static void setupEnemyRecord(DoomRPG_t* doomRpg,
                             EspNativeGameplayMonsterRecord* record,
                             int32_t parm,
                             uint32_t* rngCalls) {
    const EnemyTemplate* enemy = &enemyTemplates[record->subtype];
    uint8_t baseHealth = clampByte((int32_t)enemy->maxHealth);
    uint8_t baseArmor = clampByte((int32_t)enemy->maxArmor);
    uint8_t health;
    uint8_t armor;
    uint8_t defense;
    uint8_t strength;
    uint8_t agility;
    uint8_t accuracy;
    int32_t healthScale;
    int32_t defenseScale;
    int32_t strengthScale;
    int32_t agilityScale;
    int32_t accuracyScale;

    /* Exact CombatEntity_setupEnemy() random-call order. */
    healthScale = scaledRandomValue(doomRpg, parm, rngCalls);
    defenseScale = scaledRandomValue(doomRpg, parm, rngCalls);
    strengthScale = scaledRandomValue(doomRpg, parm, rngCalls);
    agilityScale = scaledRandomValue(doomRpg, parm, rngCalls);
    accuracyScale = scaledRandomValue(doomRpg, parm, rngCalls);

    health = scaledStat(baseHealth, healthScale);
    armor = scaledStat(baseArmor, defenseScale);
    defense = scaledStat(enemy->defense, defenseScale);
    strength = scaledStat(enemy->strength, strengthScale);
    agility = scaledStat(enemy->agility, agilityScale);
    accuracy = scaledStat(enemy->accuracy, accuracyScale);

    record->param1 = packParam1(health, health, armor, armor);
    record->param2 = packParam2(defense, strength, agility, accuracy);

    /* Exact Entity_initspawn() post-setup orientation/attack-variant draw. */
    record->alternateAttack =
        (uint8_t)(DoomRPG_randNextByte(&doomRpg->random) & 1U);
    ++(*rngCalls);
}

static const EnemyDefTemp* findEnemyDef(const EnemyDefTemp* defs,
                                        uint32_t count,
                                        uint16_t tileIndex) {
    uint32_t i;
    for (i = 0U; i < count; ++i) {
        if (defs[i].tileIndex == tileIndex) return &defs[i];
    }
    return NULL;
}

static void logWitness(void) {
    const EspNativeGameplayMonsterRecord* record =
        EspNativeGameplayMonsterState_find(DOG_WITNESS_SPRITE);

    if (record == NULL) {
        printf("[MONSTERSTATE] WITNESS sprite=%u status=missing\n",
               (unsigned int)DOG_WITNESS_SPRITE);
        return;
    }

    printf("[MONSTERSTATE] WITNESS sprite=%u defTile=%u subtype=%u mType=%u hp=%u/%u armor=%u/%u def=%u str=%u agi=%u acc=%u alt=%u alive=%u\n",
           (unsigned int)record->spriteIndex,
           (unsigned int)record->defTile,
           (unsigned int)record->subtype,
           (unsigned int)record->mType,
           (unsigned int)p1Health(record->param1),
           (unsigned int)p1MaxHealth(record->param1),
           (unsigned int)p1Armor(record->param1),
           (unsigned int)p1MaxArmor(record->param1),
           (unsigned int)p2Defense(record->param2),
           (unsigned int)p2Strength(record->param2),
           (unsigned int)p2Agility(record->param2),
           (unsigned int)p2Accuracy(record->param2),
           (unsigned int)record->alternateAttack,
           (unsigned int)record->alive);
}

void EspNativeGameplayMonsterState_reset(void) {
    if (monsterRecords != NULL) heap_caps_free(monsterRecords);
    monsterRecords = NULL;
    memset(&monsterView, 0, sizeof(monsterView));
    monsterView.witnessSpriteIndex = ESP_NATIVE_GAMEPLAY_MONSTER_NO_SPRITE;
}

int EspNativeGameplayMonsterState_ensure(DoomRPG_t* doomRpg) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    EnemyDefTemp* defs = NULL;
    EspAssetPackEntry entityDefsEntry;
    Random_t randomBefore;
    uint8_t header[2];
    uint8_t source[ENTITY_DEF_RECORD_BYTES];
    uint32_t sourceDefCount;
    uint32_t enemyDefCount = 0U;
    uint32_t expectedEnemies;
    uint32_t built = 0U;
    uint32_t rngCalls = 0U;
    uint32_t i;
    uint32_t lookup;
    uint16_t defTile;
    uint16_t linkState;
    uint16_t linkOrder;
    uint8_t type;
    uint8_t subtype;
    uint8_t openedHere = 0U;
    EspMapSprite sprite;
    const EnemyDefTemp* def;

    if (doomRpg == NULL || runtime == NULL || topology == NULL ||
        !EspMapRuntime_isLoaded() || !EspMapSpriteTopology_isReady() ||
        runtime->mapSpriteCount != topology->spriteCount ||
        topology->enemyCount == 0U ||
        topology->enemyCount > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) {
        return 0;
    }

    if (monsterRecords != NULL && monsterView.count == topology->enemyCount &&
        monsterView.sourceArenaFNV1a == runtime->arenaFNV1a) {
        return 1;
    }

    EspNativeGameplayMonsterState_reset();
    expectedEnemies = topology->enemyCount;
    randomBefore = doomRpg->random;

    defs = (EnemyDefTemp*)heap_caps_malloc(
        ENEMY_DEF_TEMP_CAPACITY * sizeof(EnemyDefTemp), MALLOC_CAP_8BIT);
    monsterRecords = (EspNativeGameplayMonsterRecord*)heap_caps_malloc(
        expectedEnemies * sizeof(EspNativeGameplayMonsterRecord), MALLOC_CAP_8BIT);
    if (defs == NULL || monsterRecords == NULL) goto fail;
    memset(defs, 0, ENEMY_DEF_TEMP_CAPACITY * sizeof(EnemyDefTemp));
    memset(monsterRecords, 0,
           expectedEnemies * sizeof(EspNativeGameplayMonsterRecord));

    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto fail;
        openedHere = 1U;
    }
    if (!EspAssetPack_findEntry("entities.db", &entityDefsEntry) ||
        (entityDefsEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspAssetPack_readRange(&entityDefsEntry, 0U, header, sizeof(header))) {
        goto fail;
    }

    sourceDefCount = readLe16(header);
    if (sourceDefCount == 0U || sourceDefCount > ENTITY_DEF_MAX_COUNT ||
        2U + sourceDefCount * ENTITY_DEF_RECORD_BYTES > entityDefsEntry.size) {
        goto fail;
    }

    for (i = 0U; i < sourceDefCount; ++i) {
        if (!EspAssetPack_readRange(&entityDefsEntry,
                                    2U + i * ENTITY_DEF_RECORD_BYTES,
                                    source, sizeof(source))) goto fail;
        if (source[2] != ENTITY_TYPE_ENEMY) continue;
        if (enemyDefCount >= ENEMY_DEF_TEMP_CAPACITY ||
            source[3] >= ENTITY_SUBTYPE_COUNT) goto fail;
        defs[enemyDefCount].tileIndex = readLe16(source);
        defs[enemyDefCount].subtype = source[3];
        defs[enemyDefCount].parm = readLe32(source + 4U);
        ++enemyDefCount;
    }

    /* Exact Game_loadMapEntities() ordering: ascending map-sprite index. */
    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) goto fail;
        (void)linkState;
        (void)linkOrder;
        if (type != ENTITY_TYPE_ENEMY) continue;
        if (built >= expectedEnemies || subtype >= ENTITY_SUBTYPE_COUNT ||
            !EspMapRuntime_getMapSprite(i, &sprite)) goto fail;

        lookup = sprite.info & MAP_SPRITE_DEF_MASK;
        if ((sprite.info & MAP_SPRITE_TILE_FLAG) != 0U) {
            lookup += MAP_SPRITE_TILE_DEF_BASE;
        }
        if (lookup > UINT16_MAX) goto fail;
        defTile = (uint16_t)lookup;
        def = findEnemyDef(defs, enemyDefCount, defTile);
        if (def == NULL || def->subtype != subtype) goto fail;

        monsterRecords[built].spriteIndex = (uint16_t)i;
        monsterRecords[built].defTile = defTile;
        monsterRecords[built].subtype = subtype;
        monsterRecords[built].mType = subtype;
        monsterRecords[built].alive = 1U;
        setupEnemyRecord(doomRpg, &monsterRecords[built],
                         (int32_t)def->parm, &rngCalls);
        ++built;
    }

    if (built != expectedEnemies || rngCalls != built * 6U) goto fail;

    if (openedHere) {
        EspAssetPack_close();
        openedHere = 0U;
    }
    heap_caps_free(defs);
    defs = NULL;

    monsterView.records = monsterRecords;
    monsterView.count = built;
    monsterView.ownerBytes = built * sizeof(EspNativeGameplayMonsterRecord);
    monsterView.sourceArenaFNV1a = runtime->arenaFNV1a;
    monsterView.rngFNVBefore = randomFNV(&randomBefore);
    monsterView.rngFNVAfter = randomFNV(&doomRpg->random);
    monsterView.rngCalls = rngCalls;
    monsterView.witnessSpriteIndex =
        EspNativeGameplayMonsterState_find(DOG_WITNESS_SPRITE) != NULL
            ? DOG_WITNESS_SPRITE
            : ESP_NATIVE_GAMEPLAY_MONSTER_NO_SPRITE;
    monsterView.stateFNV1a = recordsFNV();

    printf("[MONSTERSTATE] READY arena=%08x enemies=%u ownerBytes=%u recordBytes=%u enemyDefs=%u rngCalls=%u rng=%08x->%08x stateFNV=%08x noLegacyEntity=yes packOpen=%u\n",
           (unsigned int)monsterView.sourceArenaFNV1a,
           (unsigned int)monsterView.count,
           (unsigned int)monsterView.ownerBytes,
           (unsigned int)sizeof(EspNativeGameplayMonsterRecord),
           (unsigned int)enemyDefCount,
           (unsigned int)monsterView.rngCalls,
           (unsigned int)monsterView.rngFNVBefore,
           (unsigned int)monsterView.rngFNVAfter,
           (unsigned int)monsterView.stateFNV1a,
           (unsigned int)EspAssetPack_isOpen());
    logWitness();
    return 1;

fail:
    if (openedHere) EspAssetPack_close();
    if (defs != NULL) heap_caps_free(defs);
    doomRpg->random = randomBefore;
    EspNativeGameplayMonsterState_reset();
    printf("[MONSTERSTATE] FAILED rollbackRng=yes packOpen=%u\n",
           (unsigned int)EspAssetPack_isOpen());
    return 0;
}

int EspNativeGameplayMonsterState_isReady(void) {
    return monsterRecords != NULL && monsterView.count != 0U;
}

const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void) {
    return EspNativeGameplayMonsterState_isReady() ? &monsterView : NULL;
}

EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_findMutable(
    uint16_t spriteIndex) {
    uint32_t lo = 0U;
    uint32_t hi = monsterView.count;

    if (!EspNativeGameplayMonsterState_isReady()) return NULL;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        uint16_t current = monsterRecords[mid].spriteIndex;
        if (current == spriteIndex) return &monsterRecords[mid];
        if (current < spriteIndex) lo = mid + 1U;
        else hi = mid;
    }
    return NULL;
}

const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(
    uint16_t spriteIndex) {
    return EspNativeGameplayMonsterState_findMutable(spriteIndex);
}

/* Keep initialization/reset in front of the already hardware-proven action
 * engine without changing that engine's door/dialog/fire routing. */
int __real_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg);
void __real_EspNativeGameplayActionEngine_reset(void);

int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg) {
    if (!EspNativeGameplayMonsterState_ensure(doomRpg)) return 0;
    return __real_EspNativeGameplayActionEngine_service(doomRpg);
}

void __wrap_EspNativeGameplayActionEngine_reset(void) {
    EspNativeGameplayMonsterState_reset();
    __real_EspNativeGameplayActionEngine_reset();
}
