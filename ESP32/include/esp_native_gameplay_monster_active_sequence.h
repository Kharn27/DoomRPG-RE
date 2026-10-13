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
/* Checkpoint-only, once after the normal view/cache priming. Three other
 * cardinal BSP viewpoints are read-only previews (no world redraw/camera/RNG)
 * and may append truly BSP-admitted live monsters to the restored active set.
 * Fail closed without changing activation if any probe cannot be validated. */
int EspNativeGameplayMonsterActiveSequence_reprimeCheckpointSight(
    struct DoomRPG_s* doomRpg);

void EspNativeGameplayMonsterActiveSequence_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
