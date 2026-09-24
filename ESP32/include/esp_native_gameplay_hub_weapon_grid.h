#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_H

#include <stdint.h>

#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The dedicated WPN page presents the nine conventional Doom RPG weapons in a
 * complete 3x3 arsenal. Weapon ids 9..11 are captured familiar forms and stay
 * intentionally outside this grid. */
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT 9U
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT 9U
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS 3U
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_ROWS 3U

int EspNativeGameplayHubWeaponGrid_paint(
    uint16_t* framebuffer,
    const EspNativeGameplayPlayerState* player,
    uint8_t selectedWeapon);
int EspNativeGameplayHubWeaponGrid_hitTest(int logicalX,
                                           int logicalY,
                                           uint8_t* outWeaponId);
void EspNativeGameplayHubWeaponGrid_cellBounds(uint8_t slotId,
                                                uint8_t* outLeft,
                                                uint8_t* outTop,
                                                uint8_t* outRight,
                                                uint8_t* outBottom);

#ifdef __cplusplus
}
#endif

#endif
