#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_runtime.h"
#include "esp_native_gameplay_monster_drop.h"
#include "esp_native_gameplay_monster_position.h"

typedef struct MonsterDropOwner_s {
    EspNativeGameplayMonsterDropRecord
        records[ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS];
    EspNativeGameplayMonsterDropView view;
} MonsterDropOwner;

static MonsterDropOwner drops;

_Static_assert(sizeof(EspNativeGameplayMonsterDropRecord) == 16U,
               "monster drop record must stay compact");

static int resolveDrop(uint32_t rnd,
                       uint8_t monsterSubtype,
                       int32_t monsterParm,
                       uint8_t* outType,
                       uint8_t* outSubtype) {
    uint32_t n;
    uint32_t n2;
    uint8_t n5;
    uint8_t n6;
    uint8_t type = 0xffU;
    uint8_t subtype = 0U;
    int b3;
    int b4;

    if (outType == NULL || outSubtype == NULL) return 0;
    *outType = 0xffU;
    *outSubtype = 0U;

    /* Exact early-out in Entity_spawnDropItem(). The historical 0x100000
     * per-entity drop-once bit is implicit because a native monster can only
     * transition alive -> dead once. */
    if (monsterSubtype == 1U || monsterSubtype == 12U ||
        monsterSubtype == 13U || (rnd & 0xffU) < 51U) {
        return 0;
    }

    n = rnd >> 8;
    n2 = n >> 1;
    n5 = (uint8_t)(n2 & 0xffU);
    n6 = (uint8_t)((n2 >> 8) & 0xffU);

    if ((n & 1U) != 0U) {
        type = 3U;
        if (n5 < 115U) {
            subtype = 20U;
        }
        else if (n5 < 230U) {
            subtype = 21U;
        }
        else {
            uint8_t bonus = 0U;
            if (monsterParm == 294) bonus = 3U;
            else if (monsterParm == 467) bonus = 8U;
            else if (monsterParm == 640) bonus = 13U;
            subtype = n5 < (uint8_t)(230U + bonus) ? 23U : 22U;
        }
    }
    else {
        b3 = n5 < 76U;
        b4 = n6 < 15U;
        switch (monsterSubtype) {
        case 0U:
            type = (uint8_t)(b4 ? 16U : 6U);
            subtype = (uint8_t)(b3 ? 2U : 1U);
            break;
        case 2U:
            type = (uint8_t)((b4 || b3) ? 16U : 6U);
            subtype = 1U;
            break;
        case 6U:
            type = 4U;
            subtype = (uint8_t)(b4 ? 26U : 25U);
            break;
        case 7U:
            type = 3U;
            subtype = 23U;
            break;
        case 8U:
            type = (uint8_t)(b4 ? 16U : 6U);
            subtype = 3U;
            break;
        case 9U:
            type = (uint8_t)(b4 ? 16U : 6U);
            subtype = 4U;
            break;
        case 10U:
            if (b3) {
                type = 4U;
                subtype = (uint8_t)(b4 ? 26U : 25U);
            }
            else {
                type = 3U;
                subtype = 23U;
            }
            break;
        case 11U:
            type = 16U;
            subtype = (uint8_t)(b3 ? 4U : 2U);
            break;
        default:
            break;
        }
    }

    if (type == 0xffU) return 0;
    *outType = type;
    *outSubtype = subtype;
    return 1;
}

void EspNativeGameplayMonsterDrop_reset(void) {
    memset(&drops, 0, sizeof(drops));
    drops.view.records = drops.records;
}

int EspNativeGameplayMonsterDrop_ensure(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    if (runtime == NULL || runtime->arenaFNV1a == 0U) return 0;
    if (drops.view.active == 0U ||
        drops.view.sourceArenaFNV1a != runtime->arenaFNV1a) {
        EspNativeGameplayMonsterDrop_reset();
        drops.view.sourceArenaFNV1a = runtime->arenaFNV1a;
        drops.view.active = 1U;
    }
    return 1;
}

const EspNativeGameplayMonsterDropView* EspNativeGameplayMonsterDrop_view(void) {
    return drops.view.active != 0U ? &drops.view : NULL;
}

int EspNativeGameplayMonsterDrop_prepare(
    uint32_t dropRoll,
    const EspNativeGameplayMonsterRecord* monster,
    EspNativeGameplayMonsterDropSpawnPlan* outPlan) {
    const EspNativeGameplayMonsterPositionRecord* position;
    int32_t parm = 0;
    uint8_t type;
    uint8_t subtype;
    uint16_t defTile;

    if (outPlan != NULL) memset(outPlan, 0, sizeof(*outPlan));
    if (monster == NULL || outPlan == NULL ||
        !EspNativeGameplayMonsterDrop_ensure()) {
        return 0;
    }

    outPlan->sourceArenaFNV1a = drops.view.sourceArenaFNV1a;
    outPlan->dropRoll = dropRoll;
    outPlan->spawnSerialBefore = drops.view.spawnSerial;
    outPlan->slot = ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NO_SLOT;
    outPlan->nextSlotBefore = drops.view.nextSlot;
    outPlan->outcome = ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NONE;

    /* No-drop filters do not need EntityDef or position lookup. */
    if (monster->subtype == 1U || monster->subtype == 12U ||
        monster->subtype == 13U || (dropRoll & 0xffU) < 51U) {
        return 1;
    }

    if (!EspEntityDefTypeCatalog_getParm(monster->defTile, &parm)) return 0;
    if (!resolveDrop(dropRoll, monster->subtype, parm, &type, &subtype)) {
        return 1;
    }
    if (!EspEntityDefTypeCatalog_findTileIndex(type, subtype, &defTile) ||
        !EspNativeGameplayMonsterPosition_ensure()) {
        return 0;
    }
    position = EspNativeGameplayMonsterPosition_find(monster->spriteIndex);
    if (position == NULL || position->tileIndex > 1023U) return 0;

    outPlan->slot = drops.view.nextSlot;
    outPlan->outcome = ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN;
    outPlan->record.spawnOrder = drops.view.spawnSerial + 1U;
    if (outPlan->record.spawnOrder == 0U) return 0;
    outPlan->record.tileIndex = position->tileIndex;
    outPlan->record.defTile = defTile;
    outPlan->record.worldX = position->worldX;
    outPlan->record.worldY = position->worldY;
    outPlan->record.type = type;
    outPlan->record.subtype = subtype;
    outPlan->record.active = 1U;
    outPlan->record.taken = 0U;
    return 1;
}

int EspNativeGameplayMonsterDrop_commit(
    const EspNativeGameplayMonsterDropSpawnPlan* plan) {
    EspNativeGameplayMonsterDropRecord* slot;
    int wasVisible;

    if (plan == NULL ||
        plan->outcome != ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN ||
        plan->slot >= ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS ||
        !EspNativeGameplayMonsterDrop_ensure() ||
        drops.view.sourceArenaFNV1a != plan->sourceArenaFNV1a ||
        drops.view.spawnSerial != plan->spawnSerialBefore ||
        drops.view.nextSlot != plan->nextSlotBefore ||
        plan->slot != drops.view.nextSlot ||
        plan->record.active != 1U || plan->record.taken != 0U ||
        plan->record.spawnOrder != drops.view.spawnSerial + 1U) {
        return 0;
    }

    slot = &drops.records[plan->slot];
    wasVisible = slot->active != 0U && slot->taken == 0U;
    *slot = plan->record;
    drops.view.spawnSerial = plan->record.spawnOrder;
    drops.view.nextSlot =
        (uint8_t)((plan->slot + 1U) % ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS);
    if (!wasVisible &&
        drops.view.visibleCount < ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS) {
        ++drops.view.visibleCount;
    }

    printf("[MONSTERDROP] COMMIT roll=%08x slot=%u overwriteVisible=%u type=%u subtype=%u def=%u tile=%u pos=%u,%u visible=%u next=%u ownerBytes=%u persistence=map-session-live/save-deferred\n",
           (unsigned int)plan->dropRoll,
           (unsigned int)plan->slot,
           (unsigned int)wasVisible,
           (unsigned int)slot->type,
           (unsigned int)slot->subtype,
           (unsigned int)slot->defTile,
           (unsigned int)slot->tileIndex,
           (unsigned int)slot->worldX,
           (unsigned int)slot->worldY,
           (unsigned int)drops.view.visibleCount,
           (unsigned int)drops.view.nextSlot,
           (unsigned int)sizeof(drops));
    return 1;
}

int EspNativeGameplayMonsterDrop_setTaken(uint8_t slotIndex, int taken) {
    EspNativeGameplayMonsterDropRecord* record;
    const uint8_t nextTaken = taken ? 1U : 0U;
    if (slotIndex >= ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS ||
        !EspNativeGameplayMonsterDrop_ensure()) {
        return 0;
    }
    record = &drops.records[slotIndex];
    if (record->active == 0U) return 0;
    if (record->taken == nextTaken) return 1;
    if (nextTaken != 0U) {
        if (drops.view.visibleCount == 0U) return 0;
        --drops.view.visibleCount;
    }
    else {
        if (drops.view.visibleCount >= ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS) {
            return 0;
        }
        ++drops.view.visibleCount;
    }
    record->taken = nextTaken;
    return 1;
}

const char* EspNativeGameplayMonsterDrop_outcomeName(uint8_t outcome) {
    switch ((EspNativeGameplayMonsterDropOutcome)outcome) {
    case ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NONE: return "none";
    case ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN: return "spawn";
    default: return "invalid";
    }
}
