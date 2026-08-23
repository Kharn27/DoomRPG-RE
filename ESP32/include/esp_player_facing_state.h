#ifndef DOOMRPG_ESP32_PLAYER_FACING_STATE_H
#define DOOMRPG_ESP32_PLAYER_FACING_STATE_H

#include <stdint.h>

#include "esp_player_finish_rotation_tile.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PLAYER_FACING_TRACE_FLAGS 0x0001f6ffUL
#define ESP_PLAYER_FACING_NEAR_OFFSET 31
#define ESP_PLAYER_FACING_STEP_COUNT 3
#define ESP_PLAYER_FACING_NO_INDEX 0xffffU
#define ESP_PLAYER_FACING_NO_TILE 0xffffU

#define ESP_PLAYER_FACING_KIND_NONE 0U
#define ESP_PLAYER_FACING_KIND_SPRITE 1U
#define ESP_PLAYER_FACING_KIND_LINE 2U
#define ESP_PLAYER_FACING_KIND_WALL 3U

typedef enum EspPlayerFacingStatus_e {
    ESP_PLAYER_FACING_INVALID = 0,
    ESP_PLAYER_FACING_VIEW_INVALID = 1,
    ESP_PLAYER_FACING_INITIAL_INVALID = 2,
    ESP_PLAYER_FACING_ORIENTATION_INVALID = 3,
    ESP_PLAYER_FACING_SECOND_TILE_INVALID = 4,
    ESP_PLAYER_FACING_TOPOLOGY_INVALID = 5,
    ESP_PLAYER_FACING_UNSUPPORTED_CONTEXT = 6,
    ESP_PLAYER_FACING_UNSUPPORTED_ORDER = 7,
    ESP_PLAYER_FACING_STORAGE_ERROR = 8,
    ESP_PLAYER_FACING_TRACE_OVERFLOW = 9,
    ESP_PLAYER_FACING_VIEW_CONSUME_FAILED = 10,
    ESP_PLAYER_FACING_ALREADY_ACTIVE = 11,
    ESP_PLAYER_FACING_OK = 12
} EspPlayerFacingStatus;

/*
 * Compact pointer-free durable replacement for Player.facingEntity at the end
 * of recovered DoomCanvas_finishRotation(). `legacyIdentity` is deliberately a
 * normalized identity key, not a full Entity.info clone:
 *   sprite: (spriteIndex + 1)
 *   line:   (lineIndex + 1) | 0x00200000
 *   wall:   0 (legacy entities[0] sentinel)
 *   none:   0
 * The hit kind + index disambiguate wall/none without retaining Entity_t.
 */
typedef struct EspPlayerFacingState_s {
    int32_t traceStartX;
    int32_t traceStartY;
    int32_t traceEndX;
    int32_t traceEndY;
    uint32_t legacyIdentity;
    uint16_t hitIndex;
    uint16_t hitTile;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t kind;
    uint8_t entityType;
    uint8_t entitySubType;
    uint8_t traceEntityCount;
    uint8_t active;
} EspPlayerFacingState;

void EspPlayerFacing_reset(void);
int EspPlayerFacing_isReady(void);
const EspPlayerFacingState* EspPlayerFacing_view(void);

/*
 * Resolve only the currently proven angle-64 durable checkFacingEntity path.
 * The routine reads compact native map/topology/line state plus bounded ranges
 * of /entities.db from the ESP32 PAK for line-definition typing. It never
 * allocates, never calls legacy Game_trace(), and closes the PAK before return.
 * Invalid/unsupported input zeroes outState and performs no owner mutation.
 */
EspPlayerFacingStatus EspPlayerFacing_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    const EspPlayerOrientationState* orientation,
    const EspPlayerFinishRotationTileState* secondTile,
    EspPlayerFacingState* outState);

/*
 * Resolve and park the durable facing result, then consume only
 * PlayerView.facingRefreshPending. No resident map owner is mutated and
 * ST_PLAYING progression remains outside this boundary.
 */
EspPlayerFacingStatus EspPlayerFacing_route(void);

#ifdef __cplusplus
}
#endif

#endif
