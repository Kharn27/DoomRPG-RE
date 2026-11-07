#ifndef DOOMRPG_ESP32_NATIVE_STARTUP_H
#define DOOMRPG_ESP32_NATIVE_STARTUP_H

#include <stdint.h>
#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Native death-only reload of a clean PAK sector. Queue on gameplay service,
 * execute after that service returns on the outer loop. Copies only 52 B of
 * player state, never a pointer to the dying map. No implicit SD save write. */
int EspNativeStartup_queueDeathRoute(
    uint8_t targetMapId, const EspNativeGameplayPlayerState* player);
void EspNativeStartup_serviceDeathRoute(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif
#endif
