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
 * old implementation compiled as private chain leaves during migration so no
 * already-proven helper has to be rewritten in the same milestone. It should
 * not own permanent player resource state going forward.
 */
void EspNativeGameplayPickup_reset(void);
void EspNativeGameplayPickup_logCorpus(void);

int EspNativeGameplayPickup_getMapSprite(uint32_t index,
                                         EspMapSprite* outSprite);
const EspNativeGameplayHudState* EspNativeGameplayPickup_hudView(void);
EspPlayerViewMoveStatus EspNativeGameplayPickup_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter);
void EspNativeGameplayPickup_sessionService(struct DoomRPG_s* doomRpg);
void EspNativeGameplayPickup_sessionReset(void);

#define __wrap_EspMapRuntime_getMapSprite EspNativeGameplayPickup_getMapSprite
#define __wrap_EspNativeGameplayHud_view EspNativeGameplayPickup_hudView
#define __wrap_EspPlayerView_commitPreparedMove \
    EspNativeGameplayPickup_commitPreparedMove
#define __wrap_EspNativeGameplaySession_service \
    EspNativeGameplayPickup_sessionService
#define __wrap_EspNativeGameplaySession_reset EspNativeGameplayPickup_sessionReset

#ifdef __cplusplus
}
#endif

#endif
