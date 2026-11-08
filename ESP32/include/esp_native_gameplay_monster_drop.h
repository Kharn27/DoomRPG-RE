#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_DROP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_DROP_H

#include <stdint.h>

#include "esp_native_gameplay_monster_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS 8U
#define ESP_NATIVE_GAMEPLAY_MONSTER_DROP_NO_SLOT 0xffU
/* Reserved native custom gSprite class, kept in the existing 8-slot V11
 * rotating dynamic-world owner. Not an entity definition or pickup.
 * Player_pain() emits Game_gsprite_alloc(20|21|22, 2, playerX, playerY)
 * when there is no captured legacy Entity_t pointer. */
#define ESP_NATIVE_GAMEPLAY_DOG_CORPSE_TYPE 0xfeU
#define ESP_NATIVE_GAMEPLAY_DOG_CORPSE_FIRST 20U
#define ESP_NATIVE_GAMEPLAY_DOG_CORPSE_LAST 22U

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
 * Pointer-free checkpoint image of the exact rotating pool. visibleCount and
 * owner-active are derived on restore; all records, including already-taken
 * slots, remain explicit so subsequent overwrite order stays exact.
 */
typedef struct EspNativeGameplayMonsterDropSnapshot_s {
    EspNativeGameplayMonsterDropRecord
        records[ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS];
    uint32_t sourceArenaFNV1a;
    uint32_t spawnSerial;
    uint32_t stateFNV1a;
    uint8_t nextSlot;
    uint8_t reserved[3];
} EspNativeGameplayMonsterDropSnapshot;

/*
 * Compact native equivalent of the legacy eight rotating drop entities.
 * Records are map-session mutable overlays; immutable BSP sprites remain
 * untouched. The bounded snapshot API is the permanent checkpoint boundary.
 */
void EspNativeGameplayMonsterDrop_reset(void);
int EspNativeGameplayMonsterDrop_ensure(void);
const EspNativeGameplayMonsterDropView* EspNativeGameplayMonsterDrop_view(void);

int EspNativeGameplayMonsterDrop_prepare(
    uint32_t dropRoll,
    const EspNativeGameplayMonsterRecord* monster,
    EspNativeGameplayMonsterDropSpawnPlan* outPlan);
/* Purely plan one persistent, non-pickup custom corpse sprite.
 * The source 20/21/22 logical ID doubles as subtype/defTile sentinel.
 * Coordinates must be centered on an actual 64-unit map tile. */
int EspNativeGameplayMonsterDrop_prepareDogCorpse(
    uint8_t customSpriteLogical,
    uint16_t playerWorldX, uint16_t playerWorldY,
    EspNativeGameplayMonsterDropSpawnPlan* outPlan);

int EspNativeGameplayMonsterDrop_commit(
    const EspNativeGameplayMonsterDropSpawnPlan* plan);

/* Pickup transaction visibility bit. taken=1 hides/removes the live drop;
 * taken=0 is the exact bounded rollback before a failed pickup redraw. */
int EspNativeGameplayMonsterDrop_setTaken(uint8_t slot, int taken);

int EspNativeGameplayMonsterDrop_snapshot(
    EspNativeGameplayMonsterDropSnapshot* outSnapshot);
int EspNativeGameplayMonsterDrop_snapshotShapeValid(
    const EspNativeGameplayMonsterDropSnapshot* snapshot,
    uint32_t sourceArenaFNV1a);
int EspNativeGameplayMonsterDrop_restore(
    const EspNativeGameplayMonsterDropSnapshot* snapshot);
uint32_t EspNativeGameplayMonsterDrop_fingerprint(void);

const char* EspNativeGameplayMonsterDrop_outcomeName(uint8_t outcome);

#ifdef __cplusplus
}
#endif

#endif
