#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_ACTIVATION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_ACTIVATION_H

#include <stdint.h>

#include "esp_native_gameplay_monster_position.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Explicit movement composition boundary replacing the historical linker wrap
 * around EspNativeGameplayMonsterPosition_prepareCardinalMove().
 *
 * The lower-level position owner remains reusable. This API adds exactly the
 * movement-domain activation gate and publication capture required by the
 * planner transaction.
 */
int EspNativeGameplayMonsterMovementActivation_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter);

#ifdef __cplusplus
}
#endif

#endif
