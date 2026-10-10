#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_hub_weapon_grid.h"

/* Compile and link against the production hitTest/cellBounds module. The
 * PAK painter is excluded by host linker gc-sections, not mocked. This proves
 * that all twelve original weapon IDs have non-overlapping, touch-addressable
 * tiles on the fixed 160x120 logical CYD screen, including familiar IDs 9..11. */
int main(void) {
    uint8_t weapon;
    uint8_t left, top, right, bottom;
    unsigned int slot;
    int x, y;

    assert(ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT ==
           ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT);
    assert(ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT == 12U);
    assert(ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS == 4U);
    assert(ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_ROWS == 3U);

    for (slot = 0U; slot < ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT;
         ++slot) {
        EspNativeGameplayHubWeaponGrid_cellBounds(
            (uint8_t)slot, &left, &top, &right, &bottom);
        assert(left == 2U + 39U * (slot % 4U));
        assert(right == left + 38U);
        assert(top == 35U + 28U * (slot / 4U));
        assert(bottom == top + 27U);
        assert(right <= 157U && bottom <= 118U);
        for (y = top; y <= bottom; ++y) {
            for (x = left; x <= right; ++x) {
                weapon = UINT8_MAX;
                assert(EspNativeGameplayHubWeaponGrid_hitTest(x, y, &weapon));
                assert(weapon == slot);
            }
        }
    }

    weapon = UINT8_MAX;
    assert(!EspNativeGameplayHubWeaponGrid_hitTest(1, 70, &weapon));
    assert(!EspNativeGameplayHubWeaponGrid_hitTest(158, 70, &weapon));
    assert(!EspNativeGameplayHubWeaponGrid_hitTest(40, 34, &weapon));
    assert(!EspNativeGameplayHubWeaponGrid_hitTest(40, 119, &weapon));
    assert(!EspNativeGameplayHubWeaponGrid_hitTest(40, 50, NULL));

    for (slot = 9U; slot < 12U; ++slot) {
        EspNativeGameplayHubWeaponGrid_cellBounds(
            (uint8_t)slot, &left, &top, &right, &bottom);
        assert(EspNativeGameplayHubWeaponGrid_hitTest(
            (left + right) / 2, (top + bottom) / 2, &weapon));
        assert(weapon == slot);
    }

    puts("hub_weapon_grid_native_host: 12/12 cells and familiar touches PASS");
    return 0;
}
