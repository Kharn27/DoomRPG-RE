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

typedef enum EspNativeGameplayMonsterMemberProbeStatus_e {
    ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK = 1,
    ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED = 2,
    ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE = 3
} EspNativeGameplayMonsterMemberProbeStatus;

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
    /* Exact producer identity for the ranged >=217 movement branch.  This is
     * distinct from lastAttackerSpriteIndex because no attack probe exists
     * when the legacy AI chooses movement. */
    uint16_t lastMovementSpriteIndex;
    uint8_t lastReason;
    uint8_t active;
} EspNativeGameplayMonsterTurnView;

void EspNativeGameplayMonsterTurn_reset(void);
/* Explicit semantic request for non-monster player attacks (for example
 * destructibles) that still call legacy Game_advanceTurn(). The request is
 * observed after the action service returns, just like the existing combat
 * counter path, and can be cancelled while a caller still owns rollback. */
int EspNativeGameplayMonsterTurn_requestPlayerAttack(uint32_t inputSequence);
int EspNativeGameplayMonsterTurn_cancelPlayerAttack(uint32_t inputSequence);
int EspNativeGameplayMonsterTurn_requestPassTurn(uint32_t inputSequence);
int EspNativeGameplayMonsterTurn_requestBlockedAutomapMove(
    uint32_t inputSequence);

/*
 * Ordered active-list leaf. The turn scheduler calls this for exactly one
 * activated monster at a time. It performs only Entity_aiThink's immediate
 * attack gate against the settled player:
 *   NO_IMMEDIATE_ATTACK -> caller may run aiMoveToGoal
 *   RANGED_MOVE         -> caller runs the >=217 movement branch
 *   ATTACK_PUBLISHED    -> one rollback-exact probe is now in flight
 *
 * It never scans or chooses another monster.
 */
EspNativeGameplayMonsterMemberProbeStatus
EspNativeGameplayMonsterTurn_probeActiveMember(
    struct DoomRPG_s* doomRpg,
    uint16_t spriteIndex);

/*
 * Resume the exact legacy Entity_aiMoveToGoal() attack gate after live movement
 * has committed. Ordinary i==1 subtypes (1/5) are handled directly here;
 * i==3 subtypes (4/13) dispatch to ThreeGoalTurn, which may append one bounded
 * three-loop attack probe after its committed movement chain.
 */
int EspNativeGameplayMonsterTurn_postMoveGoal(struct DoomRPG_s* doomRpg,
                                              uint16_t spriteIndex,
                                              uint16_t sourceTile,
                                              uint16_t destTile);

/*
 * Permanent producer boundary used by ThreeGoalTurn once subtype 4/13 reaches
 * the exact post-move attack gate. It validates the committed destination,
 * cardinal trace, three-loop weapon contract and rollback-exact prospective
 * combat roll, then appends at most one undelivered attack probe to the current
 * monster turn. A second simultaneous attack remains fail-closed until ordered
 * multi-attacker delivery has its own owner.
 */
int EspNativeGameplayMonsterTurn_publishThreeGoalAttack(
    struct DoomRPG_s* doomRpg,
    uint16_t spriteIndex,
    uint16_t sourceTile,
    uint16_t destTile,
    uint8_t goalStep);

const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void);

#ifdef __cplusplus
}
#endif

#endif
