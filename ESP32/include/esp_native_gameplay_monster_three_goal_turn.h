#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_THREE_GOAL_TURN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_THREE_GOAL_TURN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayMonsterThreeGoalTurnView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t observedChains;
    uint32_t continuationPlans;
    uint32_t continuationCommits;
    uint32_t shortcutStops;
    uint32_t noMoveStops;
    uint32_t deferredChains;
    uint16_t lastSpriteIndex;
    uint16_t lastTile;
    uint8_t lastGoalStep;
    uint8_t active;
    uint8_t reserved[2];
} EspNativeGameplayMonsterThreeGoalTurnView;

void EspNativeGameplayMonsterThreeGoalTurn_reset(void);
const EspNativeGameplayMonsterThreeGoalTurnView*
EspNativeGameplayMonsterThreeGoalTurn_view(void);

/*
 * Execute the bounded post-move continuation for legacy i=3 monster families
 * (subtypes 4/13). MonsterTurn owns ordinary-vs-three-goal dispatch and calls
 * this entry only after the first goal has already committed.
 */
int EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal(
    struct DoomRPG_s* doomRpg,
    uint16_t spriteIndex,
    uint16_t sourceTile,
    uint16_t destTile);

#ifdef __cplusplus
}
#endif

#endif
