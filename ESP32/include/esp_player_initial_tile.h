#ifndef DOOMRPG_ESP32_PLAYER_INITIAL_TILE_H
#define DOOMRPG_ESP32_PLAYER_INITIAL_TILE_H

#include <stdint.h>

#include "esp_player_fresh_map_state.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_INITIAL_TILE_BASE_FLAGS 0x0000040fUL
#define ESP_PLAYER_INITIAL_TILE_FACING_64_FLAG 0x10000000UL
#define ESP_PLAYER_INITIAL_TILE_NO_EVENT 0xffffU

typedef enum EspPlayerInitialTileStatus_e {
    ESP_PLAYER_INITIAL_TILE_INVALID = 0,
    ESP_PLAYER_INITIAL_TILE_VIEW_INVALID = 1,
    ESP_PLAYER_INITIAL_TILE_SETUP_INVALID = 2,
    ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_CONTEXT = 3,
    ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_ORDER = 4,
    ESP_PLAYER_INITIAL_TILE_EVENT_INVALID = 5,
    ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED = 6,
    ESP_PLAYER_INITIAL_TILE_EXEC_FAILED = 7,
    ESP_PLAYER_INITIAL_TILE_ALREADY_ACTIVE = 8,
    ESP_PLAYER_INITIAL_TILE_VIEW_CONSUME_FAILED = 9,
    ESP_PLAYER_INITIAL_TILE_OK = 10
} EspPlayerInitialTileStatus;

/*
 * Permanent pointer-free witness for the first recovered Game_executeTile()
 * call made by fresh-map Game_spawnPlayer() after Player_setup().
 *
 * This owner records only the call/dispatch semantics. Mutable event state and
 * MCODE_FLAG_REMOVE bits remain owned by EspMapScriptState. The immutable BSP
 * arena is never patched. finishRotation(), its second tile execution and the
 * durable facing query are explicitly outside this boundary.
 */
typedef struct EspPlayerInitialTileState_s {
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
} EspPlayerInitialTileState;

void EspPlayerInitialTile_reset(void);
int EspPlayerInitialTile_isReady(void);
const EspPlayerInitialTileState* EspPlayerInitialTile_view(void);

/*
 * Pure preflight for the initial fresh-map tile call. `playerKeys` is explicit
 * because key ownership has not yet moved into the active native player root.
 * `executionBlocked` mirrors the recovered early Game.f658b refusal gate.
 *
 * For the currently hardware-proven Junction path only destAngle==64 is owned;
 * other facing directions fail closed until their exact legacy mapping receives
 * a dedicated proof. Output is zeroed on refusal when non-NULL.
 *
 * An eligible opcode outside the deliberately tiny native 11/19/20 executor is
 * reported through the optional diagnostics and returns OPCODE_DEFERRED without
 * mutating script/player state.
 */
EspPlayerInitialTileStatus EspPlayerInitialTile_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerFreshMapState* freshMap,
    uint32_t playerKeys,
    uint8_t executionBlocked,
    EspPlayerInitialTileState* outState,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset);

/*
 * Execute the already-bounded initial tile dispatch against native script
 * state, then atomically consume only tileEnterPending. On any refusal/failure,
 * script state, the player/view owner and this owner remain unchanged.
 */
EspPlayerInitialTileStatus EspPlayerInitialTile_route(
    uint32_t playerKeys,
    uint8_t executionBlocked,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset);

#ifdef __cplusplus
}
#endif

#endif
