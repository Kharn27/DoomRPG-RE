#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_POSITION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_POSITION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MONSTER_POSITION_NO_SPRITE 0xffffU

typedef struct EspNativeGameplayMonsterPositionRecord_s {
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint16_t worldX;
    uint16_t worldY;
} EspNativeGameplayMonsterPositionRecord;

typedef struct EspNativeGameplayMonsterPositionView_s {
    const EspNativeGameplayMonsterPositionRecord* records;
    uint32_t count;
    uint32_t ownerBytes;
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint8_t active;
    uint8_t reserved[3];
} EspNativeGameplayMonsterPositionView;

void EspNativeGameplayMonsterPosition_reset(void);
int EspNativeGameplayMonsterPosition_ensure(void);
const EspNativeGameplayMonsterPositionView* EspNativeGameplayMonsterPosition_view(void);
const EspNativeGameplayMonsterPositionRecord* EspNativeGameplayMonsterPosition_find(
    uint16_t spriteIndex);

/*
 * Permanent mutable spatial owner for native monsters. Prepare is pure and
 * accepts exactly one cardinal 64-unit tile-center move. Commit/rollback are
 * compare-and-swap-like so a probe can prove ownership without publishing a
 * position into renderer/topology state prematurely.
 */
int EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter);
int EspNativeGameplayMonsterPosition_commitPrepared(
    const EspNativeGameplayMonsterPositionRecord* expectedBefore,
    const EspNativeGameplayMonsterPositionRecord* preparedAfter);
int EspNativeGameplayMonsterPosition_rollbackPrepared(
    const EspNativeGameplayMonsterPositionRecord* expectedAfter,
    const EspNativeGameplayMonsterPositionRecord* restoreBefore);
uint32_t EspNativeGameplayMonsterPosition_fingerprint(void);

#ifdef __cplusplus
}
#endif

#endif
