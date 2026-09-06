#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_MOVEMENT_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Permanent session composition seam. The public service enters the active-list
 * sequencer; each selected member then comes back through serviceMember so the
 * proven one-capture planner/publisher transaction closes before the next
 * monster is planned. */
void EspNativeGameplayMonsterMovementProbe_service(struct DoomRPG_s* doomRpg);

/* Run exactly one already-selected synthetic movement member through the raw
 * planner, RNG-boundary guard, live publication and post-move-goal hook. The
 * activation/counter overrides are owned by the caller and must remain active
 * for this call. outCommitted is 1 only when a live position/topology publish
 * closed successfully; a valid blocked/no-move probe returns with 0. */
int EspNativeGameplayMonsterMovementProbe_serviceMember(
    struct DoomRPG_s* doomRpg,
    const char* trigger,
    uint8_t* outCommitted);

void EspNativeGameplayMonsterMovementProbe_reset(void);

#ifdef __cplusplus
}
#endif

#endif
