#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_H

#include <stdint.h>

#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The original player and circular selector own 12 weapon IDs: nine ordinary
 * weapons (0..8) and three dog-familiar forms (9..11). The HUD grid must show
 * ALL 12 to make acquired collars selectable. A 4x3 board fits the original
 * 160x120 framebuffer without allocating a second page or touching gameplay. */
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS 4U
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
