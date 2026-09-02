#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT 100U
#define ESP_NATIVE_GAMEPLAY_MONSTER_NO_SPRITE 0xffffU

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

void EspNativeGameplayMonsterState_reset(void);
int EspNativeGameplayMonsterState_ensure(struct DoomRPG_s* doomRpg);
int EspNativeGameplayMonsterState_isReady(void);
const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void);
EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_findMutable(
    uint16_t spriteIndex);
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(
    uint16_t spriteIndex);

/*
 * The monster-state lifecycle historically owned the linker wrappers around
 * the action-engine service/reset. Keep those implementations as private chain
 * leaves so generic monster combat can compose its pending transaction in front
 * without duplicating map initialization or the proven action-engine service.
 */
int EspNativeGameplayMonsterState_actionService(struct DoomRPG_s* doomRpg);
void EspNativeGameplayMonsterState_actionReset(void);
#define __wrap_EspNativeGameplayActionEngine_service \
    EspNativeGameplayMonsterState_actionService
#define __wrap_EspNativeGameplayActionEngine_reset \
    EspNativeGameplayMonsterState_actionReset

#ifdef __cplusplus
}
#endif

#endif
