#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_runtime.h"
#include "esp_map_state.h"

#define ENTRANCE_TEXTURE_ID 7U
#define EVENT_TRIGGER_MASK 0x01f80000U
#define EVENT_TILE_MASK 0x000003ffU

/* Recovered line-nudge flags used before the legacy entrance midpoint test. */
#define LINE_FLAG_EAST_SOUTH 8U
#define LINE_FLAG_WEST_NORTH 16U
#define LINE_FLAG_VERTICAL 256U
#define LINE_FLAG_HORIZONTAL 512U

static uint8_t* stateTiles;
static EspMapStateView stateView;

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static int entranceTileForLine(const EspMapLine* line, uint32_t* outTileIndex) {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t midX;
    int32_t midY;
    uint32_t tileX;
    uint32_t tileY;

    if (line == NULL || outTileIndex == NULL) {
        return 0;
    }

    x1 = (int32_t)line->x1;
    y1 = (int32_t)line->y1;
    x2 = (int32_t)line->x2;
    y2 = (int32_t)line->y2;

    /*
     * Reference Render_beginLoadMapData mutates the line coordinates by +/-3
     * before it computes the texture-7 entrance cell. Reproduce only that
     * recovered runtime semantic here; the immutable line accessor stays raw.
     */
    if ((line->flags & LINE_FLAG_HORIZONTAL) != 0U) {
        if ((line->flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & LINE_FLAG_WEST_NORTH) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & LINE_FLAG_VERTICAL) != 0U) {
        if ((line->flags & LINE_FLAG_EAST_SOUTH) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & LINE_FLAG_WEST_NORTH) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    midX = x1 + ((x2 - x1) / 2);
    midY = y1 + ((y2 - y1) / 2);
    if (midX < 0 || midY < 0) {
        return 0;
    }

    tileX = (uint32_t)midX >> 6;
    tileY = (uint32_t)midY >> 6;
    if (tileX >= ESP_MAP_STATE_WIDTH || tileY >= ESP_MAP_STATE_HEIGHT) {
        return 0;
    }

    *outTileIndex = (tileY * ESP_MAP_STATE_WIDTH) + tileX;
    return 1;
}

void EspMapState_reset(void) {
    if (stateTiles != NULL) {
        heap_caps_free(stateTiles);
        stateTiles = NULL;
    }
    memset(&stateView, 0, sizeof(stateView));
}

int EspMapState_buildFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t eventValue;
    uint32_t tileIndex;
    uint32_t i;
    uint8_t blockValue;
    uint8_t before;
    int ok = 0;

    if (runtime == NULL || runtime->arena == NULL || runtime->arenaBytes == 0U) {
        printf("[MAPSTATE] FAILED native runtime unavailable\n");
        return 0;
    }

    EspMapState_reset();

    stateTiles = (uint8_t*)heap_caps_malloc(ESP_MAP_STATE_BYTES,
                                             MALLOC_CAP_8BIT);
    if (stateTiles == NULL) {
        printf("[MAPSTATE] FAILED allocation bytes=%u\n",
               (unsigned int)ESP_MAP_STATE_BYTES);
        return 0;
    }
    memset(stateTiles, 0, ESP_MAP_STATE_BYTES);

    stateView.tileFlags = stateTiles;
    stateView.tileCount = ESP_MAP_STATE_TILE_COUNT;

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapRuntime_getBlockCell(i, &blockValue) || blockValue > 3U) {
            printf("[MAPSTATE] FAILED block cell=%u\n", (unsigned int)i);
            goto done;
        }
        stateTiles[i] = blockValue;
        ++stateView.baseCounts[blockValue];
    }

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            printf("[MAPSTATE] FAILED line index=%u\n", (unsigned int)i);
            goto done;
        }
        if (line.texture != ENTRANCE_TEXTURE_ID) {
            continue;
        }

        ++stateView.entranceLineRefs;
        if (!entranceTileForLine(&line, &tileIndex)) {
            printf("[MAPSTATE] FAILED entrance line=%u coords=%u,%u-%u,%u flags=%08x\n",
                   (unsigned int)i,
                   (unsigned int)line.x1,
                   (unsigned int)line.y1,
                   (unsigned int)line.x2,
                   (unsigned int)line.y2,
                   (unsigned int)line.flags);
            goto done;
        }

        before = stateTiles[tileIndex];
        stateTiles[tileIndex] = (uint8_t)(before | ESP_MAP_TILE_ENTRANCE);
        if ((before & ESP_MAP_TILE_ENTRANCE) == 0U) {
            ++stateView.entranceCells;
        }
    }

    for (i = 0U; i < runtime->eventCount; ++i) {
        if (!EspMapRuntime_getEvent(i, &eventValue)) {
            printf("[MAPSTATE] FAILED event index=%u\n", (unsigned int)i);
            goto done;
        }
        if ((eventValue & EVENT_TRIGGER_MASK) == 0U) {
            continue;
        }

        ++stateView.eventRefs;
        tileIndex = eventValue & EVENT_TILE_MASK;
        if (tileIndex >= ESP_MAP_STATE_TILE_COUNT) {
            printf("[MAPSTATE] FAILED event tile index=%u value=%08x\n",
                   (unsigned int)tileIndex,
                   (unsigned int)eventValue);
            goto done;
        }

        before = stateTiles[tileIndex];
        stateTiles[tileIndex] = (uint8_t)(before | ESP_MAP_TILE_EVENTS);
        if ((before & ESP_MAP_TILE_EVENTS) == 0U) {
            ++stateView.eventCells;
        }
    }

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if ((stateTiles[i] & ESP_MAP_TILE_VISITED) != 0U) {
            printf("[MAPSTATE] FAILED visited synthesized tile=%u flags=%02x\n",
                   (unsigned int)i,
                   (unsigned int)stateTiles[i]);
            goto done;
        }
    }

    stateView.stateFNV1a = fnv1a32(stateTiles, ESP_MAP_STATE_BYTES);

    printf("[MAPSTATE] READY bytes=%u fnv=%08x base=%u/%u/%u/%u entranceRefs=%u entranceCells=%u eventRefs=%u eventCells=%u visited=0\n",
           (unsigned int)ESP_MAP_STATE_BYTES,
           (unsigned int)stateView.stateFNV1a,
           (unsigned int)stateView.baseCounts[0],
           (unsigned int)stateView.baseCounts[1],
           (unsigned int)stateView.baseCounts[2],
           (unsigned int)stateView.baseCounts[3],
           (unsigned int)stateView.entranceLineRefs,
           (unsigned int)stateView.entranceCells,
           (unsigned int)stateView.eventRefs,
           (unsigned int)stateView.eventCells);

    ok = 1;

done:
    if (!ok) {
        EspMapState_reset();
    }
    return ok;
}

int EspMapState_isReady(void) {
    return stateTiles != NULL &&
           stateView.tileFlags == stateTiles &&
           stateView.tileCount == ESP_MAP_STATE_TILE_COUNT;
}

const EspMapStateView* EspMapState_view(void) {
    return EspMapState_isReady() ? &stateView : NULL;
}

int EspMapState_getTileFlags(uint32_t tileIndex, uint8_t* outFlags) {
    if (!EspMapState_isReady() || outFlags == NULL ||
        tileIndex >= ESP_MAP_STATE_TILE_COUNT) {
        return 0;
    }

    *outFlags = stateTiles[tileIndex];
    return 1;
}

int EspMapState_setVisited(uint32_t tileIndex, uint8_t visited) {
    uint8_t before;
    uint8_t after;

    if (!EspMapState_isReady() || visited > 1U ||
        tileIndex >= ESP_MAP_STATE_TILE_COUNT) {
        return 0;
    }

    before = stateTiles[tileIndex];
    if (visited != 0U) after = (uint8_t)(before | ESP_MAP_TILE_VISITED);
    else after = (uint8_t)(before & (uint8_t)~ESP_MAP_TILE_VISITED);
    if (after == before) return 1;

    stateTiles[tileIndex] = after;
    stateView.stateFNV1a = fnv1a32(stateTiles, ESP_MAP_STATE_BYTES);
    return 1;
}
