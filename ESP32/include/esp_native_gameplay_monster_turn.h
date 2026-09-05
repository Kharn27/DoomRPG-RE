#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TURN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TURN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef enum EspNativeGameplayMonsterTurnReason_e {
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_NONE = 0,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE = 1,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE = 2,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK = 3,
    ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PASS_TURN = 4
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
int EspNativeGameplayMonsterTurn_requestPassTurn(uint32_t inputSequence);

/*
 * Resume the exact one-step legacy Entity_aiMoveToGoal() attack gate after a
 * live monster move has committed. This does not schedule a second monster
 * turn: it may append one attack probe to the current turn for the moved sprite.
 *
 * The current bounded family owns only legacy goal-count i==1 subtypes (1 and
 * 5). Subtypes 4 and 13 require three same-turn movement goals and therefore
 * remain fail-closed until native multi-step movement/interpolation is owned.
 */
int EspNativeGameplayMonsterTurn_postMoveGoal(struct DoomRPG_s* doomRpg,
                                              uint16_t spriteIndex,
                                              uint16_t sourceTile,
                                              uint16_t destTile);

const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void);

#ifdef __cplusplus
}
#endif

#endif
