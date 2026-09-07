#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_CONTENT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_CONTENT_H

#include <stdint.h>

#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES 17U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_VALUE_BYTES 12U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS 12U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS 5U
#define ESP_NATIVE_GAMEPLAY_HUB_CONTENT_MAX_ENTRIES 23U

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

typedef enum EspNativeGameplayHubInventoryEntryKind_e {
    ESP_NATIVE_GAMEPLAY_HUB_ENTRY_WEAPON = 1,
    ESP_NATIVE_GAMEPLAY_HUB_ENTRY_NOTEBOOK = 2,
    ESP_NATIVE_GAMEPLAY_HUB_ENTRY_ITEM = 3,
    ESP_NATIVE_GAMEPLAY_HUB_ENTRY_CREDITS = 4,
    ESP_NATIVE_GAMEPLAY_HUB_ENTRY_KEY = 5
} EspNativeGameplayHubInventoryEntryKind;

typedef struct EspNativeGameplayHubInventoryEntry_s {
    char name[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES];
    char value[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_VALUE_BYTES];
    uint8_t kind;
    uint8_t sourceId;
} EspNativeGameplayHubInventoryEntry;

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

/*
 * Read-only selectable Inventory projection in the original Menu_initMenu
 * content order, excluding Back/dividers because the native HUB already owns
 * those affordances separately:
 *
 *   owned weapons 0..11
 *   Notebook
 *   carried items 25..29
 *   Credits
 *   Green / Yellow / Blue / Red keys
 *
 * The count is allocation-free and requires no PAK I/O. entryAt() resolves only
 * the requested historical EntityDef name and preserves the caller's PAK-open
 * state. Weapon values follow the legacy menu contract: Axe shows "--" and
 * weapon ids 1..11 show ammo[weaponInfo[id].ammoType].
 */
uint8_t EspNativeGameplayHubContent_inventoryEntryCount(
    const EspNativeGameplayPlayerState* player);
int EspNativeGameplayHubContent_inventoryEntryAt(
    const EspNativeGameplayPlayerState* player,
    uint8_t entryIndex,
    EspNativeGameplayHubInventoryEntry* outEntry);
const char* EspNativeGameplayHubContent_inventoryKindName(uint8_t kind);

/* Strict first-open witness for the maximum 23-entry projection. It uses a
 * local synthetic PlayerState only; canonical gameplay ownership is never
 * mutated. Names still come from the real native EntityDef catalog/PAK. */
int EspNativeGameplayHubContent_probeInventoryList(void);

/* Strict first-open witness retained from the preceding milestone: prove all 12
 * weapon and five consumable historical EntityDef names can be reverse-resolved
 * from the compact catalog without retaining them. Preserves caller PAK state. */
int EspNativeGameplayHubContent_probeCatalog(void);

#ifdef __cplusplus
}
#endif

#endif
