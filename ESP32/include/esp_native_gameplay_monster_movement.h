#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef struct EspNativeGameplayMonsterMovementView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t observedMovementDeferredTurns;
    uint32_t probes;
    uint32_t plannedMoves;
    uint32_t rollbackMoves;
    uint32_t ambiguousGeometry;
    uint32_t collisionDeferred;
    uint32_t rngBoundaryDeferred;
    uint32_t lastPositionFNV1a;
    uint16_t lastSpriteIndex;
    uint16_t lastSourceTile;
    uint16_t lastDestTile;
    uint8_t lastReason;
    uint8_t active;
    uint8_t reserved[2];
} EspNativeGameplayMonsterMovementView;

void EspNativeGameplayMonsterMovement_reset(void);
const EspNativeGameplayMonsterMovementView* EspNativeGameplayMonsterMovement_view(void);
void EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
