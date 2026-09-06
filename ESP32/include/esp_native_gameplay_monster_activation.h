#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compact map-session equivalent of legacy Game_activate(). A monster becomes
 * active when the native BSP renderer admits its map sprite, and remains active
 * for the map session. First activation order is retained in one bounded compact
 * array so turn consumers can reproduce the legacy circular-list iteration
 * without importing Entity_t / EntityMonster_t pointer ownership.
 */
int EspNativeGameplayMonsterActivation_observeVisible(uint16_t spriteIndex,
                                                      uint8_t subtype,
                                                      uint16_t tileIndex);
int EspNativeGameplayMonsterActivation_isActive(uint16_t spriteIndex);
uint32_t EspNativeGameplayMonsterActivation_count(void);
int EspNativeGameplayMonsterActivation_getOrdered(uint32_t ordinal,
                                                  uint16_t* outSpriteIndex);

/* Internal composition controls used only while the multi-active movement
 * sequencer replays one legacy active-list member through the already-proven
 * single-candidate planner. They allocate nothing and never change activation
 * lifetime/order. */
void EspNativeGameplayMonsterActivation_selectOnly(uint16_t spriteIndex);
void EspNativeGameplayMonsterActivation_clearSelection(void);
void EspNativeGameplayMonsterActivation_overrideTurnCounters(
    uint32_t movementDeferredTurns,
    uint32_t noAttackTurns);
void EspNativeGameplayMonsterActivation_clearTurnCounterOverride(void);

#ifdef __cplusplus
}
#endif

#endif
