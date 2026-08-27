#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;
struct EspPlayerViewState_s;

/* Post-move compact Entity_touched() bridge.  Current production support owns
 * the generic weapon-pickup family (legacy eType=5). Other pickup families are
 * inventoried and remain fail-closed until their compact player-state owners
 * exist. */
void EspNativeGameplayPickup_reset(void);
void EspNativeGameplayPickup_onServiceMove(
    struct DoomRPG_s* doomRpg,
    const struct EspPlayerViewState_s* beforeView,
    const struct EspPlayerViewState_s* afterView);
void EspNativeGameplayPickup_logCorpus(void);

#ifdef __cplusplus
}
#endif

#endif
