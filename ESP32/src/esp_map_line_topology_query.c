#include <stdint.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_line_topology_query.h"
#include "esp_map_runtime.h"

#define MAP_WIDTH 32U
#define LINE_ENTITY_DEF_BASE 305U
#define LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL
#define LINE_GEOMETRY_AXIS_X 0x00000008UL
#define LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define LINE_ENTITY_NUDGE_X_NEG 0x00004000UL
#define LEGACY_TRACE_MASK 0xf287U

typedef char EspMapLineTopologyRef_must_be_16_bytes[
    sizeof(EspMapLineTopologyRef) == 16U ? 1 : -1];

static int entityTypeInTraceMask(uint8_t type) {
    return type < 16U && (LEGACY_TRACE_MASK & (1U << type)) != 0U;
}

/* Exact line-entity placement recovered for Game_loadMapEntities(): apply the
 * +/-3 render geometry nudge, take the midpoint, then apply the mutually
 * exclusive +/-1 link-side nudge before mapping to the 32x32 tile grid. */
static int lineEntityTile(const EspMapLine* line, uint16_t* outTile) {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t x;
    int32_t y;
    uint32_t tileX;
    uint32_t tileY;

    if (line == NULL || outTile == NULL) return 0;
    x1 = (int32_t)line->x1;
    y1 = (int32_t)line->y1;
    x2 = (int32_t)line->x2;
    y2 = (int32_t)line->y2;

    if ((line->flags & LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3;
            x2 += 3;
        }
        else if ((line->flags & LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3;
            x2 -= 3;
        }
    }
    else if ((line->flags & LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3;
            y2 += 3;
        }
        else if ((line->flags & LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3;
            y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    if (x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MAP_WIDTH || tileY >= MAP_WIDTH) return 0;
    *outTile = (uint16_t)((tileY * MAP_WIDTH) + tileX);
    return 1;
}

int EspMapLineTopologyQuery_findLinkedOnTile(
    uint16_t tileIndex,
    EspMapLineTopologyRef* outLine) {
    const EspMapRuntimeView* runtime;
    uint32_t i;

    if (outLine == NULL) return -1;
    memset(outLine, 0, sizeof(*outLine));
    outLine->lineIndex = ESP_MAP_LINE_TOPOLOGY_NO_LINE;

    if (tileIndex >= 1024U || !EspMapRuntime_isLoaded() ||
        !EspMapLineState_isReady() || !EspEntityDefTypeCatalog_isReady()) {
        return -1;
    }

    runtime = EspMapRuntime_view();
    if (runtime == NULL) return -1;

    i = runtime->lineCount;
    while (i > 0U) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t locked;
        uint8_t type;
        int hasDefinition;

        --i;
        if (!EspMapLineState_getOpen(i, &open) ||
            !EspMapLineState_getLocked(i, &locked) ||
            !EspMapRuntime_getLine(i, &line)) {
            return -1;
        }
        if (open != 0U) continue;

        lookup = LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getType((uint16_t)lookup, &type);
        if (!hasDefinition) {
            if ((line.flags & LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            type = 0U;
        }
        if (!entityTypeInTraceMask(type)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile != tileIndex) continue;

        outLine->flags = line.flags;
        outLine->lineIndex = (uint16_t)i;
        outLine->tileIndex = lineTile;
        outLine->texture = line.texture;
        outLine->entityDefTile =
            lookup < 65536U ? (uint16_t)lookup : 0xffffU;
        outLine->entityType = type;
        outLine->open = open;
        outLine->locked = locked;
        outLine->linked = 1U;
        return 1;
    }

    return 0;
}
