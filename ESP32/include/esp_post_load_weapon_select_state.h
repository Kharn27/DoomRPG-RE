#ifndef DOOMRPG_ESP32_POST_LOAD_WEAPON_SELECT_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_WEAPON_SELECT_STATE_H

#include <stdint.h>

#include "esp_post_load_givemap_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadWeaponSelectStatus_e {
    ESP_POST_LOAD_WEAPON_SELECT_INVALID = 0,
    ESP_POST_LOAD_WEAPON_SELECT_GIVEMAP_INVALID = 1,
    ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_ORDER = 3,
    ESP_POST_LOAD_WEAPON_SELECT_WORLD_NOT_READY = 4,
    ESP_POST_LOAD_WEAPON_SELECT_WEAPON_INVALID = 5,
    ESP_POST_LOAD_WEAPON_SELECT_ALREADY_ACTIVE = 6,
    ESP_POST_LOAD_WEAPON_SELECT_OK = 7
} EspPostLoadWeaponSelectStatus;

/*
 * Exact caller-order marker for the fresh Junction load call:
 *
 *     Player_selectWeapon(player, player->weapon)
 *
 * Because requestedWeapon is the already-current weapon, legacy
 * Player_selectWeapon() skips DoomCanvas_updateViewTrue() and the final weapon
 * assignment is an identity write. This owner records only that semantic fact;
 * it deliberately does not introduce weapon inventory/ammo ownership.
 */
typedef struct EspPostLoadWeaponSelectState_s {
    uint8_t weaponBefore;
    uint8_t requestedWeapon;
    uint8_t weaponAfter;
    uint8_t viewInvalidationRequested;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t active;
} EspPostLoadWeaponSelectState;

void EspPostLoadWeaponSelect_reset(void);
int EspPostLoadWeaponSelect_isReady(void);
const EspPostLoadWeaponSelectState* EspPostLoadWeaponSelect_view(void);

/*
 * Pure translation of the exact current-weapon self-selection callsite.
 * currentWeapon is supplied as a scalar by the future native Player owner (the
 * temporary hardware probe samples legacy Player.weapon read-only). Invalid or
 * out-of-order input zeroes outState and performs no mutation.
 */
EspPostLoadWeaponSelectStatus EspPostLoadWeaponSelect_prepare(
    const EspPostLoadGiveMapState* giveMap,
    uint8_t currentWeapon,
    EspPostLoadWeaponSelectState* outState);

/* Park the exact self-selection semantic once. No allocation or legacy call. */
EspPostLoadWeaponSelectStatus EspPostLoadWeaponSelect_route(
    uint8_t currentWeapon);

#ifdef __cplusplus
}
#endif

#endif
