#include <stddef.h>
#include <stdint.h>

#include "esp_native_gameplay_hub_nonweapon.h"

static uint8_t ownedWeaponCount(const EspNativeGameplayPlayerState* player) {
    uint16_t weapons;
    uint8_t count = 0U;
    uint8_t i;
    if (player == NULL || player->active != 1U) return 0U;
    weapons = (uint16_t)(player->weapons & 0x0fffU);
    for (i = 0U; i < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS; ++i) {
        if ((weapons & (uint16_t)(1U << i)) != 0U) ++count;
    }
    return count;
}

uint8_t EspNativeGameplayHubNonWeapon_entryCount(
    const EspNativeGameplayPlayerState* player) {
    uint8_t total;
    uint8_t weapons;
    if (player == NULL || player->active != 1U) return 0U;
    total = EspNativeGameplayHubContent_inventoryEntryCount(player);
    weapons = ownedWeaponCount(player);
    if (total == 0U || total < weapons) return 0U;
    return (uint8_t)(total - weapons);
}

int EspNativeGameplayHubNonWeapon_entryAt(
    const EspNativeGameplayPlayerState* player,
    uint8_t entryIndex,
    EspNativeGameplayHubInventoryEntry* outEntry) {
    uint8_t count;
    uint8_t weapons;
    if (player == NULL || outEntry == NULL || player->active != 1U) return 0;
    count = EspNativeGameplayHubNonWeapon_entryCount(player);
    weapons = ownedWeaponCount(player);
    if (count == 0U || entryIndex >= count) return 0;
    return EspNativeGameplayHubContent_inventoryEntryAt(
        player, (uint8_t)(weapons + entryIndex), outEntry);
}
