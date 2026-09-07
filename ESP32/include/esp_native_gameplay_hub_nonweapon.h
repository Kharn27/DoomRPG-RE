#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_NONWEAPON_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_NONWEAPON_H

#include <stdint.h>

#include "esp_native_gameplay_hub_content.h"
#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read-only projection of the legacy Inventory content after the owned-weapon
 * prefix. Weapons now have a dedicated grid page; Notebook/items/Credits/keys
 * keep their original relative order without a second persistent list owner. */
uint8_t EspNativeGameplayHubNonWeapon_entryCount(
    const EspNativeGameplayPlayerState* player);
int EspNativeGameplayHubNonWeapon_entryAt(
    const EspNativeGameplayPlayerState* player,
    uint8_t entryIndex,
    EspNativeGameplayHubInventoryEntry* outEntry);

#ifdef __cplusplus
}
#endif

#endif
