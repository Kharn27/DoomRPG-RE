#ifndef DOOMRPG_ESP32_MAP_STATE_H
#define DOOMRPG_ESP32_MAP_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_STATE_WIDTH 32U
#define ESP_MAP_STATE_HEIGHT 32U
#define ESP_MAP_STATE_TILE_COUNT (ESP_MAP_STATE_WIDTH * ESP_MAP_STATE_HEIGHT)
#define ESP_MAP_STATE_BYTES ESP_MAP_STATE_TILE_COUNT

/* Recovered Doom RPG automap/tile-state bits. */
#define ESP_MAP_TILE_WALL 0x01U
#define ESP_MAP_TILE_SECRET 0x02U
#define ESP_MAP_TILE_ENTRANCE 0x04U
#define ESP_MAP_TILE_EVENTS 0x08U
#define ESP_MAP_TILE_VISITED 0x10U

typedef struct EspMapStateView_s {
    const uint8_t* tileFlags;
    uint32_t tileCount;
    uint32_t stateFNV1a;

    uint32_t baseCounts[4];
    uint32_t entranceLineRefs;
    uint32_t entranceCells;
    uint32_t eventRefs;
    uint32_t eventCells;
} EspMapStateView;

/*
 * Own the smallest mutable spatial state derived from the immutable map arena.
 *
 * Initial flags reproduce the reference map-load semantics:
 *   - two packed block-map bits per tile (wall/secret),
 *   - BIT_AM_ENTRANCE from texture-7 lines after the recovered line nudge,
 *   - BIT_AM_EVENTS from qualifying tile-event records.
 *
 * BIT_AM_VISITED starts clear and is mutated explicitly by native gameplay /
 * load-state owners such as EV_GIVEMAP.
 */
void EspMapState_reset(void);
int EspMapState_buildFromRuntime(void);
int EspMapState_isReady(void);
const EspMapStateView* EspMapState_view(void);
int EspMapState_getTileFlags(uint32_t tileIndex, uint8_t* outFlags);

/* Set/clear only BIT_AM_VISITED while preserving every structural tile bit. */
int EspMapState_setVisited(uint32_t tileIndex, uint8_t visited);

#ifdef __cplusplus
}
#endif

#endif
