#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT 100U
#define ESP_NATIVE_GAMEPLAY_MONSTER_NO_SPRITE 0xffffU
#define ESP_NATIVE_GAMEPLAY_MONSTER_SNAPSHOT_RECORD_BYTES 16U

typedef struct EspNativeGameplayMonsterRecord_s {
    uint32_t param1;
    uint32_t param2;
    uint16_t spriteIndex;
    uint16_t defTile;
    uint8_t subtype;
    uint8_t mType;
    uint8_t alternateAttack;
    uint8_t alive;
} EspNativeGameplayMonsterRecord;

typedef struct EspNativeGameplayMonsterView_s {
    const EspNativeGameplayMonsterRecord* records;
    uint32_t count;
    uint32_t ownerBytes;
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint32_t rngFNVBefore;
    uint32_t rngFNVAfter;
    uint32_t rngCalls;
    uint16_t witnessSpriteIndex;
} EspNativeGameplayMonsterView;

/*
 * Save/checkpoint image of the permanent logical monster owner. The full fixed
 * capacity keeps the on-disk section version-stable while only count records
 * participate in the semantic fingerprint; unused tail records must be zero.
 */
typedef struct EspNativeGameplayMonsterStateSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint16_t count;
    uint16_t recordBytes;
    EspNativeGameplayMonsterRecord
        records[ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT];
} EspNativeGameplayMonsterStateSnapshot;

void EspNativeGameplayMonsterState_reset(void);
int EspNativeGameplayMonsterState_ensure(struct DoomRPG_s* doomRpg);
int EspNativeGameplayMonsterState_isReady(void);
const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void);
EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_findMutable(
    uint16_t spriteIndex);
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(
    uint16_t spriteIndex);

int EspNativeGameplayMonsterState_snapshot(
    EspNativeGameplayMonsterStateSnapshot* outSnapshot);
int EspNativeGameplayMonsterState_snapshotShapeValid(
    const EspNativeGameplayMonsterStateSnapshot* snapshot,
    uint32_t expectedArenaFNV1a);
int EspNativeGameplayMonsterState_stageRestore(
    const EspNativeGameplayMonsterStateSnapshot* snapshot);
/* Exact in-place rollback for a serialized native blast transaction. Never
 * stages a new allocation or touches immutable topology/position owners. */
int EspNativeGameplayMonsterState_restoreInPlace(
    const EspNativeGameplayMonsterStateSnapshot* snapshot);

/* Explicit MonsterState service/reset leaves called by MonsterCombat; retain
 * zero-enemy/no-allocation admission and the original ensure/RNG ordering. */
int EspNativeGameplayMonsterState_actionService(struct DoomRPG_s* doomRpg);
void EspNativeGameplayMonsterState_actionReset(void);

#ifdef __cplusplus
}
#endif

#endif
