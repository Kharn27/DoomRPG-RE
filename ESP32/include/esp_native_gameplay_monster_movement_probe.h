#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Permanent session composition seam for the bounded movement planner probe.
 * It only borrows a reserved post-refill RNG table when exactly one new
 * movement/no-attack producer turn is pending; otherwise it delegates directly
 * to the fail-closed movement service. */
void EspNativeGameplayMonsterMovementProbe_service(struct DoomRPG_s* doomRpg);
void EspNativeGameplayMonsterMovementProbe_reset(void);

#ifdef __cplusplus
}
#endif

#endif
