#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H

#include <stdint.h>

#include "esp_native_gameplay_monster_turn.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_SPRITES 1024U
#define ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_BYTES \
    (ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_SPRITES / 8U)
#define ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_ORDER 64U

typedef struct EspNativeGameplayMonsterActivationSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint16_t activeOrder[ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_ORDER];
    uint8_t activeBits[ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_BYTES];
    uint16_t activatedCount;
    uint8_t activeOrderCount;
    uint8_t reserved0;
} EspNativeGameplayMonsterActivationSnapshot;

/*
 * Compact map-session equivalent of legacy Game_activate(). A monster becomes
 * active when the native BSP renderer admits its map sprite, and remains active
 * for the map session. First activation order is retained in one bounded compact
 * array so turn consumers can reproduce the legacy circular-list iteration
 * without importing Entity_t / EntityMonster_t pointer ownership.
 */
/* Explicit composition boundary for MonsterTurn -> activation filtering.
 * serviceTurn() owns deferred destructible-turn flush and producer/filter sync;
 * turnView() is side-effect-free and exposes only the last serviced filtered
 * view to downstream attack/movement consumers. */
int EspNativeGameplayMonsterActivation_serviceTurn(void);
const EspNativeGameplayMonsterTurnView*
EspNativeGameplayMonsterActivation_turnView(void);

int EspNativeGameplayMonsterActivation_observeVisible(uint16_t spriteIndex,
                                                      uint8_t subtype,
                                                      uint16_t tileIndex);
int EspNativeGameplayMonsterActivation_isActive(uint16_t spriteIndex);
uint32_t EspNativeGameplayMonsterActivation_count(void);
int EspNativeGameplayMonsterActivation_getOrdered(uint32_t ordinal,
                                                  uint16_t* outSpriteIndex);

void EspNativeGameplayMonsterActivation_reset(void);
int EspNativeGameplayMonsterActivation_snapshot(
    EspNativeGameplayMonsterActivationSnapshot* outSnapshot);
int EspNativeGameplayMonsterActivation_snapshotShapeValid(
    const EspNativeGameplayMonsterActivationSnapshot* snapshot,
    uint32_t expectedArenaFNV1a);
int EspNativeGameplayMonsterActivation_restoreSnapshot(
    const EspNativeGameplayMonsterActivationSnapshot* snapshot);

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
