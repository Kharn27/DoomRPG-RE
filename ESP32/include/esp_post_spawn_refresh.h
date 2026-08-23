#ifndef DOOMRPG_ESP32_POST_SPAWN_REFRESH_H
#define DOOMRPG_ESP32_POST_SPAWN_REFRESH_H

#include <stdint.h>

#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_POST_SPAWN_FACING_NONE   0U
#define ESP_POST_SPAWN_FACING_WALL   1U
#define ESP_POST_SPAWN_FACING_LINE   2U
#define ESP_POST_SPAWN_FACING_SPRITE 3U
#define ESP_POST_SPAWN_NO_INDEX 0xffffU
#define ESP_POST_SPAWN_NO_TYPE  0xffU

#define ESP_POST_SPAWN_FACING_TRACE_FLAGS 0x0001f6ffUL

typedef enum EspPostSpawnRefreshStatus_e {
    ESP_POST_SPAWN_REFRESH_INVALID = 0,
    ESP_POST_SPAWN_REFRESH_NOT_READY = 1,
    ESP_POST_SPAWN_REFRESH_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_SPAWN_REFRESH_INDEX_NOT_READY = 3,
    ESP_POST_SPAWN_REFRESH_WORLD_MUTATED = 4,
    ESP_POST_SPAWN_REFRESH_QUERY_FAILED = 5,
    ESP_POST_SPAWN_REFRESH_ALREADY_ACTIVE = 6,
    ESP_POST_SPAWN_REFRESH_VIEW_CONSUME_FAILED = 7,
    ESP_POST_SPAWN_REFRESH_OK = 8
} EspPostSpawnRefreshStatus;

/*
 * Pointer-free result/owner for the two recovered operations immediately after
 * Game_spawnPlayer() placement:
 *
 *   Hud.isUpdate = true
 *   DoomCanvas_checkFacingEntity()
 *
 * The HUD write is routed as a durable native refresh intent; no presentation
 * is performed. Facing identity is represented by kind + compact line/sprite
 * index rather than an Entity_t pointer. Player_setup, initial tile-enter and
 * ST_PLAYING remain outside this owner.
 */
typedef struct EspPostSpawnRefreshState_s {
    int16_t rayStartX;
    int16_t rayStartY;
    int16_t rayEndX;
    int16_t rayEndY;

    uint16_t sourceTileIndex;
    uint16_t endTileIndex;
    uint16_t facingIndex;
    uint16_t facingTileIndex;

    uint8_t facingKind;
    uint8_t facingEntityType;
    uint8_t facingEntitySubType;
    uint8_t traceEntityCount;
    uint8_t tracedTileCount;
    uint8_t hudRefreshIntent;
    uint8_t hudRefreshRouted;
    uint8_t facingResolved;
    uint8_t active;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
} EspPostSpawnRefreshState;

void EspPostSpawnRefresh_reset(void);
int EspPostSpawnRefresh_isReady(void);
const EspPostSpawnRefreshState* EspPostSpawnRefresh_view(void);

/*
 * Pure allocation-free initial fresh-map refresh query. It reproduces the
 * cardinal Game_trace(..., 128767) + checkFacingEntity selection order from the
 * compact native map owners. The current milestone deliberately supports only
 * the fresh post-spawn state before any native line OPEN/CLOSE mutation.
 */
EspPostSpawnRefreshStatus EspPostSpawnRefresh_query(
    const EspPlayerViewState* playerView,
    EspPostSpawnRefreshState* outState);

/* Query the current live player/view owner, consume its HUD/facing pending bits
 * atomically, then park the durable post-spawn refresh owner. */
EspPostSpawnRefreshStatus EspPostSpawnRefresh_apply(void);

#ifdef __cplusplus
}
#endif

#endif
