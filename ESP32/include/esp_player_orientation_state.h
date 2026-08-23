#ifndef DOOMRPG_ESP32_PLAYER_ORIENTATION_STATE_H
#define DOOMRPG_ESP32_PLAYER_ORIENTATION_STATE_H

#include <stdint.h>

#include "esp_player_initial_tile.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_ORIENTATION_ANGLE_64 64U
#define ESP_PLAYER_ORIENTATION_FIXED_ONE 65536L
#define ESP_PLAYER_ORIENTATION_STEP_SIZE 64L

typedef enum EspPlayerOrientationStatus_e {
    ESP_PLAYER_ORIENTATION_INVALID = 0,
    ESP_PLAYER_ORIENTATION_VIEW_INVALID = 1,
    ESP_PLAYER_ORIENTATION_TILE_INVALID = 2,
    ESP_PLAYER_ORIENTATION_UNSUPPORTED_CONTEXT = 3,
    ESP_PLAYER_ORIENTATION_UNSUPPORTED_ORDER = 4,
    ESP_PLAYER_ORIENTATION_ALREADY_ACTIVE = 5,
    ESP_PLAYER_ORIENTATION_OK = 6
} EspPlayerOrientationStatus;

/*
 * Pointer-free owner for only the orientation preparation at the start of
 * recovered DoomCanvas_finishRotation(). The second Game_executeTile() call and
 * the final durable checkFacingEntity() are deliberately outside this owner.
 *
 * The current hardware-proven fresh-map path owns only destAngle==64. Its
 * fixed-point values are exact consequences of the legacy 16.16 sin table:
 *   viewSin   =  65536
 *   viewCos   =      0
 *   viewStepX =      0
 *   viewStepY =    -64
 * Other angles fail closed until their exact native mapping is separately
 * promoted.
 */
typedef struct EspPlayerOrientationState_s {
    int32_t viewSin;
    int32_t viewCos;
    int32_t viewStepX;
    int32_t viewStepY;

    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t destAngle;
    uint8_t prepared;
    uint8_t active;
    uint8_t reserved[2];
} EspPlayerOrientationState;

void EspPlayerOrientation_reset(void);
int EspPlayerOrientation_isReady(void);
const EspPlayerOrientationState* EspPlayerOrientation_view(void);

/*
 * Pure translation of the post-initial-tile player/view state into the first
 * four writes performed by DoomCanvas_finishRotation(). Output is zeroed on
 * refusal when non-NULL. No global state is changed.
 */
EspPlayerOrientationStatus EspPlayerOrientation_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    EspPlayerOrientationState* outState);

/*
 * Park one permanent orientation owner from the live post-initial-tile state.
 * Player/view and resident-map owners are not mutated and no allocation or
 * storage I/O occurs.
 */
EspPlayerOrientationStatus EspPlayerOrientation_route(void);

#ifdef __cplusplus
}
#endif

#endif
