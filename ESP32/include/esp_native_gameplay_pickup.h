#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H

#include <stdint.h>

#include "esp_map_runtime.h"
#include "esp_native_gameplay_hud.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Historical weapon-only pickup bring-up implementation.
 *
 * Generic player resources now own the public MOVE/HUD/session hooks. Keep the
 * legacy weapon-only helpers still compile, but their obsolete session
 * service/reset leaves are retired. The permanent resource session chain is
 * exclusively owned by PlayerResources and SessionCoordinator.
 */
void EspNativeGameplayPickup_reset(void);
void EspNativeGameplayPickup_logCorpus(void);

int EspNativeGameplayPickup_getMapSprite(uint32_t index,
                                         EspMapSprite* outSprite);
const EspNativeGameplayHudState* EspNativeGameplayPickup_hudView(void);
EspPlayerViewMoveStatus EspNativeGameplayPickup_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter);

#define __wrap_EspNativeGameplayHud_view EspNativeGameplayPickup_hudView
#define __wrap_EspPlayerView_commitPreparedMove \
    EspNativeGameplayPickup_commitPreparedMove

#ifdef __cplusplus
}
#endif

#endif
