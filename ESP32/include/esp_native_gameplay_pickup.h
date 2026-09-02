#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H

#include <stdint.h>

#include "esp_map_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compact post-move Entity_touched() ownership.
 *
 * Production support is deliberately bounded to the generic legacy eType=5
 * weapon-pickup family. One bit per resident sprite records world removal and a
 * tiny weapon-selection overlay owns the only player-facing mutation currently
 * needed by the renderer. World-item/inventory/ammo pickup families are counted
 * by the corpus but remain fail-closed until their compact player-state owners
 * exist.
 */
void EspNativeGameplayPickup_reset(void);
void EspNativeGameplayPickup_logCorpus(void);

/*
 * The pickup implementation historically owned the linker wrapper around
 * EspMapRuntime_getMapSprite() so consumed weapon sprites could be hidden.
 * Keep that implementation as a private chain leaf: a generic presentation
 * overlay now composes combat visual-state requirements in front of it without
 * duplicating pickup ownership or bypassing its consumed-bit overlay.
 */
int EspNativeGameplayPickup_getMapSprite(uint32_t index,
                                         EspMapSprite* outSprite);
#define __wrap_EspMapRuntime_getMapSprite EspNativeGameplayPickup_getMapSprite

#ifdef __cplusplus
}
#endif

#endif
