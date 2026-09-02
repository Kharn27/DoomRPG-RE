#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TURN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TURN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayMonsterTurnReason_e {
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_NONE = 0,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE = 1,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE = 2,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK = 3
} EspNativeGameplayMonsterTurnReason;

typedef struct EspNativeGameplayMonsterTurnView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t scheduledTurns;
    uint32_t probes;
    uint32_t attackProbes;
    uint32_t noAttackTurns;
    uint32_t ambiguousTurns;
    uint32_t movementDeferredTurns;
    uint32_t observedPlayerAttacks;
    uint16_t lastAttackerSpriteIndex;
    uint8_t lastReason;
    uint8_t active;
} EspNativeGameplayMonsterTurnView;

void EspNativeGameplayMonsterTurn_reset(void);
const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void);

#ifdef __cplusplus
}
#endif

#endif
