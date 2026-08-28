#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PICKUP_H

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

#ifdef __cplusplus
}
#endif

#endif
