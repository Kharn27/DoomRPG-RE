#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_MOVE_COLLISION_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_MOVE_COLLISION_PROBE_H

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionMoveCollisionProbe_reset(void);
int Esp32JunctionMoveCollisionProbe_isActive(void);
void Esp32JunctionMoveCollisionProbe_service(struct DoomRPG_s* doomRpg);
void Esp32JunctionMoveCollisionProbe_observeConsumed(
    const EspNativeGameplayInputState* intent);

#ifdef __cplusplus
}
#endif

#endif
