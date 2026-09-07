#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_CONTENT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_CONTENT_H

#include <stdint.h>

#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES 17U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS 12U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS 5U

typedef struct EspNativeGameplayHubContent_s {
    char weaponName[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES];
    char itemName[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES];
    uint8_t weaponAmmoType;
    uint8_t weaponAmmoUsage;
    uint8_t weaponAmmoValue;
    uint8_t firstItemSlot;
    uint8_t firstItemCount;
    uint8_t hasItem;
} EspNativeGameplayHubContent;

/*
 * Read-only presentation projection for the native HUB. The result is caller
 * stack storage only; no names or second inventory owner are retained.
 *
 * Weapon names are resolved exactly like legacy EntityDef_find(5, weapon).
 * Item names are resolved exactly like EntityDef_find(4, 25 + slot).
 */
int EspNativeGameplayHubContent_snapshot(
    const EspNativeGameplayPlayerState* player,
    EspNativeGameplayHubContent* outContent);

/* Legacy store wording for the five real player ammo pools. Type 5 is the
 * familiar/no-ammo family and intentionally returns "--". */
const char* EspNativeGameplayHubContent_ammoLabel(uint8_t ammoType);

/* Strict first-open witness: prove all 12 weapon and five consumable historical
 * EntityDef names can be reverse-resolved from the compact catalog and read
 * from /entities.db without retaining them. Leaves the PAK closed. */
int EspNativeGameplayHubContent_probeCatalog(void);

#ifdef __cplusplus
}
#endif

#endif
