#ifndef DOOMRPG_ESP32_PLAYER_FINISH_ROTATION_TILE_H
#define DOOMRPG_ESP32_PLAYER_FINISH_ROTATION_TILE_H

#include <stdint.h>

#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_FINISH_ROTATION_TILE_FLAGS 0x10000400UL
#define ESP_PLAYER_FINISH_ROTATION_TILE_NO_EVENT 0xffffU

typedef enum EspPlayerFinishRotationTileStatus_e {
    ESP_PLAYER_FINISH_ROTATION_TILE_INVALID = 0,
    ESP_PLAYER_FINISH_ROTATION_TILE_VIEW_INVALID = 1,
    ESP_PLAYER_FINISH_ROTATION_TILE_INITIAL_INVALID = 2,
    ESP_PLAYER_FINISH_ROTATION_TILE_ORIENTATION_INVALID = 3,
    ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_CONTEXT = 4,
    ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_ORDER = 5,
    ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID = 6,
    ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED = 7,
    ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED = 8,
    ESP_PLAYER_FINISH_ROTATION_TILE_ALREADY_ACTIVE = 9,
    ESP_PLAYER_FINISH_ROTATION_TILE_OK = 10
} EspPlayerFinishRotationTileStatus;

/*
 * Pointer-free witness for only the second Game_executeTile() call inside the
 * recovered DoomCanvas_finishRotation() fresh-map path.
 *
 * The final durable checkFacingEntity() and ST_PLAYING progression are outside
 * this owner. Mutable event state and MCODE_FLAG_REMOVE bits stay in the
 * existing EspMapScriptState overlay; immutable BSP bytecode is never patched.
 */
typedef struct EspPlayerFinishRotationTileState_s {
    uint32_t inputFlags;
    uint16_t tileIndex;
    uint16_t eventIndex;

    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t eventFound;
    uint8_t eventState;
    uint8_t eventFlags;
    uint8_t eligibleCommands;
    uint8_t executedCommands;
    uint8_t removedCommands;
    uint8_t eventBlocked;
    uint8_t skipAdvanceTurn;
    uint8_t active;
    uint8_t reserved[2];
} EspPlayerFinishRotationTileState;

void EspPlayerFinishRotationTile_reset(void);
int EspPlayerFinishRotationTile_isReady(void);
const EspPlayerFinishRotationTileState* EspPlayerFinishRotationTile_view(void);

/*
 * Pure preflight for the second finishRotation tile dispatch. playerKeys is
 * explicit until key ownership moves into the permanent native player root;
 * executionBlocked mirrors the recovered Game.f658b early refusal gate.
 *
 * Only the currently proven angle-64 orientation is enabled. Unsupported
 * eligible opcodes are reported without mutating script/player owners.
 */
EspPlayerFinishRotationTileStatus EspPlayerFinishRotationTile_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    const EspPlayerOrientationState* orientation,
    uint32_t playerKeys,
    uint8_t executionBlocked,
    EspPlayerFinishRotationTileState* outState,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset);

/*
 * Execute the fully preflighted second tile dispatch against native script
 * state and park the new owner. PlayerView, InitialTile and Orientation owners
 * remain byte-for-byte unchanged. Script mutations are rolled back on failure.
 */
EspPlayerFinishRotationTileStatus EspPlayerFinishRotationTile_route(
    uint32_t playerKeys,
    uint8_t executionBlocked,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset);

#ifdef __cplusplus
}
#endif

#endif
