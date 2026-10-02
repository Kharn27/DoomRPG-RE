#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_DROP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_DROP_H

#include <stdint.h>

#include "esp_native_gameplay_monster_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS 8U
#define ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NO_SLOT 0xffU

typedef enum EspNativeGameplayMonsterDropOutcome_e {
    ESP_NATIVE_GAMEPLAY_MONSTER_DROP_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NONE = 1,
    ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN = 2
} EspNativeGameplayMonsterDropOutcome;

typedef struct EspNativeGameplayMonsterDropRecord_s {
    uint32_t spawnOrder;
    uint16_t tileIndex;
    uint16_t defTile;
    uint16_t worldX;
    uint16_t worldY;
    uint8_t type;
    uint8_t subtype;
    uint8_t active;
    uint8_t taken;
} EspNativeGameplayMonsterDropRecord;

typedef struct EspNativeGameplayMonsterDropView_s {
    const EspNativeGameplayMonsterDropRecord* records;
    uint32_t sourceArenaFNV1a;
    uint32_t spawnSerial;
    uint8_t nextSlot;
    uint8_t visibleCount;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayMonsterDropView;

typedef struct EspNativeGameplayMonsterDropSpawnPlan_s {
    EspNativeGameplayMonsterDropRecord record;
    uint32_t sourceArenaFNV1a;
    uint32_t dropRoll;
    uint32_t spawnSerialBefore;
    uint8_t slot;
    uint8_t nextSlotBefore;
    uint8_t outcome;
    uint8_t reserved;
} EspNativeGameplayMonsterDropSpawnPlan;

/*
 * Compact native equivalent of the legacy eight rotating drop entities.
 * Records are map-session mutable overlays; immutable BSP sprites remain
 * untouched. SAVE persistence is a separate checkpoint boundary.
 */
void EspNativeGameplayMonsterDrop_reset(void);
int EspNativeGameplayMonsterDrop_ensure(void);
const EspNativeGameplayMonsterDropView* EspNativeGameplayMonsterDrop_view(void);

int EspNativeGameplayMonsterDrop_prepare(
    uint32_t dropRoll,
    const EspNativeGameplayMonsterRecord* monster,
    EspNativeGameplayMonsterDropSpawnPlan* outPlan);
int EspNativeGameplayMonsterDrop_commit(
    const EspNativeGameplayMonsterDropSpawnPlan* plan);

/* Pickup transaction visibility bit. taken=1 hides/removes the live drop;
 * taken=0 is the exact bounded rollback before a failed pickup redraw. */
int EspNativeGameplayMonsterDrop_setTaken(uint8_t slot, int taken);

const char* EspNativeGameplayMonsterDrop_outcomeName(uint8_t outcome);

#ifdef __cplusplus
}
#endif

#endif
