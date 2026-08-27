#ifndef DOOMRPG_ESP32_PLAYER_SPAWN_STATE_H
#define DOOMRPG_ESP32_PLAYER_SPAWN_STATE_H

#include <stdint.h>

#include "esp_bsp_reader.h"
#include "esp_map_committed_transition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_SPAWN_MAP_WIDTH 32U
#define ESP_PLAYER_SPAWN_TILE_COUNT 1024U
#define ESP_PLAYER_SPAWN_TILE_SIZE 64U
#define ESP_PLAYER_SPAWN_TILE_CENTER 32U
#define ESP_PLAYER_SPAWN_VIEW_Z 36U
#define ESP_PLAYER_SPAWN_VIEW_Z_OLD 4U

#define ESP_PLAYER_SPAWN_SOURCE_HEADER 1U
#define ESP_PLAYER_SPAWN_SOURCE_OVERRIDE 2U

#define ESP_PLAYER_SPAWN_LOAD_FRESH_MAP 0U

typedef enum EspPlayerSpawnStatus_e {
    ESP_PLAYER_SPAWN_INVALID = 0,
    ESP_PLAYER_SPAWN_NOT_COMMITTED = 1,
    ESP_PLAYER_SPAWN_UNSUPPORTED_CONTEXT = 2,
    ESP_PLAYER_SPAWN_TARGET_MISMATCH = 3,
    ESP_PLAYER_SPAWN_RUNTIME_MISMATCH = 4,
    ESP_PLAYER_SPAWN_SPAWN_INVALID = 5,
    ESP_PLAYER_SPAWN_OK = 6
} EspPlayerSpawnStatus;

/*
 * Pointer-free projection of recovered Game_spawnPlayer() placement writes.
 *
 * For a fresh map (loadType == 0, gameIsLoaded == 0), spawnParam == 0 falls
 * back to the BSP header spawnIndex/spawnDirection. A non-zero spawnParam
 * overrides tile X/Y/angle with the recovered bit layout and would be cleared
 * by legacy Game_spawnPlayer(); overrideUsed records that semantic while
 * sourceSpawnParam preserves the durable input value.
 *
 * worldX/worldY are both the projected view and destination coordinates.
 * angle is both projected viewAngle and destAngle. viewZ/viewZOld mirror the
 * fixed legacy placement values. The three pending flags deliberately stop
 * before checkFacingEntity(), Player_setup() and initial tile execution.
 *
 * This state owns no pointers and performs no Game/Player/Render/DoomCanvas
 * mutation, no PAK I/O and no allocation.
 */
typedef struct EspPlayerSpawnState_s {
    uint32_t sourceSpawnParam;
    uint16_t tileIndex;
    uint16_t worldX;
    uint16_t worldY;
    uint8_t tileX;
    uint8_t tileY;
    uint8_t angle;
    uint8_t viewZ;
    uint8_t viewZOld;
    uint8_t spawnSource;
    uint8_t loadType;
    uint8_t overrideUsed;
    uint8_t facingRefreshPending;
    uint8_t playerSetupPending;
    uint8_t tileEnterPending;
    uint8_t active;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
} EspPlayerSpawnState;

void EspPlayerSpawn_reset(EspPlayerSpawnState* state);

/*
 * Prepare the initial startup-map placement from the already resident BSP.
 * This is the cinematic-intro -> startupMap path and intentionally has no
 * committed-transition prerequisite. It supports only the fresh-map context,
 * requires an exact resident/inventory match, and always uses the BSP header
 * spawn because no CHANGEMAP spawnParam exists at initial startup.
 */
EspPlayerSpawnStatus EspPlayerSpawn_prepareInitial(
    uint8_t targetMapId,
    const EspBspInventory* targetInventory,
    uint8_t loadType,
    uint8_t gameIsLoaded,
    EspPlayerSpawnState* outState);

/*
 * Prepare fresh-map player placement from one already COMMITTED transition.
 *
 * Only the ordinary map-load context is supported here: loadType must be 0 and
 * gameIsLoaded must be 0. Saved-game restoration is intentionally outside this
 * milestone and fails closed. The target inventory must match both the durable
 * committed-transition identity and the currently resident compact runtime.
 */
EspPlayerSpawnStatus EspPlayerSpawn_prepareCommitted(
    const EspMapCommittedTransitionState* transition,
    const EspBspInventory* targetInventory,
    uint8_t loadType,
    uint8_t gameIsLoaded,
    EspPlayerSpawnState* outState);

#ifdef __cplusplus
}
#endif

#endif
