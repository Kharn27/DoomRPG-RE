#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_weapon_control.h"

#define WEAPON_CONTROL_COUNT ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT

/* Legacy Combat.weaponInfo selection metadata. Slots 0..8 are the standard
 * player weapons owned by CombatMath. Slots 9..11 are the familiar weapons;
 * they consume no ammo but remain part of the historical circular selector. */
static const uint8_t selectionAmmoType[WEAPON_CONTROL_COUNT] = {
    0U, 0U, 1U, 2U, 1U, 2U, 4U, 3U, 4U, 5U, 5U, 5U
};
static const uint8_t selectionAmmoUsage[WEAPON_CONTROL_COUNT] = {
    0U, 1U, 1U, 1U, 3U, 2U, 3U, 1U, 15U, 0U, 0U, 0U
};

static int usable(const EspNativeGameplayPlayerState* player, uint8_t weapon) {
    uint8_t ammoType;
    uint8_t ammoUsage;
    if (player == NULL || weapon >= WEAPON_CONTROL_COUNT ||
        (player->weapons & (uint16_t)(1U << weapon)) == 0U) {
        return 0;
    }
    ammoType = selectionAmmoType[weapon];
    ammoUsage = selectionAmmoUsage[weapon];
    if (ammoType >= ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES) return 0;
    return ammoUsage == 0U || player->ammo[ammoType] > 0U;
}

EspNativeGameplayWeaponControlStatus EspNativeGameplayWeaponControl_prepare(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayWeaponControlResult* outResult) {
    const EspNativeGameplayPlayerState* player;
    int step;
    int cursor;
    uint8_t inspected = 0U;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (intent == NULL || outResult == NULL ||
        (intent->action != ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON &&
         intent->action != ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON)) {
        return ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_INVALID;
    }
    if (!EspNativeGameplayPlayerState_ensure()) {
        return ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_NOT_READY;
    }
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL || player->active != 1U ||
        player->weapon >= WEAPON_CONTROL_COUNT) {
        return ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_NOT_READY;
    }

    outResult->playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
    outResult->weapons = player->weapons;
    outResult->weaponBefore = player->weapon;
    outResult->weaponAfter = player->weapon;
    outResult->direction = intent->action == ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON
                               ? 1U : 0U;

    step = outResult->direction != 0U ? 1 : -1;
    cursor = (int)player->weapon;
    while (inspected < WEAPON_CONTROL_COUNT - 1U) {
        cursor += step;
        if (cursor >= (int)WEAPON_CONTROL_COUNT) cursor = 0;
        else if (cursor < 0) cursor = (int)WEAPON_CONTROL_COUNT - 1;
        ++inspected;
        if (usable(player, (uint8_t)cursor)) {
            outResult->weaponAfter = (uint8_t)cursor;
            outResult->ammoTypeAfter = selectionAmmoType[cursor];
            outResult->ammoAfter = player->ammo[outResult->ammoTypeAfter];
            outResult->inspected = inspected;
            return ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_PREPARED;
        }
    }

    outResult->ammoTypeAfter = selectionAmmoType[player->weapon];
    outResult->ammoAfter = player->ammo[outResult->ammoTypeAfter];
    outResult->inspected = inspected;
    return ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_UNCHANGED;
}

const char* EspNativeGameplayWeaponControl_statusName(
    EspNativeGameplayWeaponControlStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_PREPARED: return "PREPARED";
    case ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_UNCHANGED: return "UNCHANGED";
    case ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_NOT_READY: return "NOT_READY";
    default: return "UNKNOWN";
    }
}
