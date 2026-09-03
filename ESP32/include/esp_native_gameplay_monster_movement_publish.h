#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PUBLISH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PUBLISH_H

#include <stdint.h>

#include "esp_native_gameplay_monster_position.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;
struct Random_s;

typedef struct EspNativeGameplayMonsterMovementPublishResult_s {
    uint32_t positionFNVBefore;
    uint32_t positionFNVAfter;
    uint32_t topologyFNVBefore;
    uint32_t topologyFNVAfter;
    uint16_t spriteIndex;
    uint16_t sourceTile;
    uint16_t destTile;
    uint8_t rngCalls;
    uint8_t committed;
    uint8_t boundaryClosed;
    uint8_t recoveryRendered;
} EspNativeGameplayMonsterMovementPublishResult;

/* Clear the one-service capture before invoking the proven movement planner. */
void EspNativeGameplayMonsterMovementPublish_beginCycle(void);

/* Called only by the activation-gated position prepare wrapper. */
void EspNativeGameplayMonsterMovementPublish_capturePrepared(
    const EspNativeGameplayMonsterPositionRecord* before,
    const EspNativeGameplayMonsterPositionRecord* after);

/* True only after a monster position has entered the live renderer projection. */
int EspNativeGameplayMonsterMovementPublish_isProjected(uint16_t spriteIndex);

/*
 * Publish a planner move only after the existing movement service has completed
 * its exact commit+rollback probe. The live transaction replays the planner RNG,
 * commits position + topology, redraws through the native sprite-position
 * overlay, and rolls every mutable owner back on failure.
 */
int EspNativeGameplayMonsterMovementPublish_afterProbe(
    struct DoomRPG_s* doomRpg,
    const char* trigger,
    const struct Random_s* boundarySaved,
    uint8_t boundaryPrepared,
    uint32_t plannedMovesBefore,
    EspNativeGameplayMonsterMovementPublishResult* outResult);

void EspNativeGameplayMonsterMovementPublish_reset(void);

#ifdef __cplusplus
}
#endif

#endif
