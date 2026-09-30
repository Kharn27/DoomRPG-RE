#ifndef ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVE_SEQUENCE_H
#define ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVE_SEQUENCE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Permanent movement-domain composition boundary. Expands one producer turn
 * across the ordered active-monster set, while each member still executes the
 * unchanged Movement planner/probe/publish transaction to completion before the
 * next member is selected. */
void EspNativeGameplayMonsterActiveSequence_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
