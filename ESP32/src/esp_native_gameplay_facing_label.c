#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_native_gameplay_facing_label.h"
#include "esp_player_view_state.h"

#define FACING_TRACE_MASK 0x0001f6ffUL
#define FACING_TRACE_STEPS 3U
#define FACING_TILE_SIZE 64
#define FACING_TILE_CENTER 32
#define FACING_MAP_WIDTH 32U
#define FACING_ENTITY_TYPE_HUD_HIDDEN 9U

#define FACING_SPRITE_DEF_MASK 511U
#define FACING_SPRITE_DEF_TILE_FLAG 0x00040000UL
#define FACING_SPRITE_DEF_TILE_BASE 305U

#define FACING_SPECIAL_TRACE_FLAG 0x00020000UL
#define FACING_SPECIAL_Y_MASK 0x00180000UL
#define FACING_SPECIAL_X_MASK 0x00600000UL

#define FACING_LINE_DEF_BASE 305U
#define FACING_LINE_FALLBACK_FLAGS 0x00000018UL
#define FACING_LINE_GEOMETRY_AXIS_X 0x00000008UL
#define FACING_LINE_GEOMETRY_AXIS_NEG 0x00000010UL
#define FACING_LINE_GEOMETRY_Y_NUDGE 0x00000100UL
#define FACING_LINE_GEOMETRY_X_NUDGE 0x00000200UL
#define FACING_LINE_ENTITY_NUDGE_Y_NEG 0x00000800UL
#define FACING_LINE_ENTITY_NUDGE_X_POS 0x00002000UL
#define FACING_LINE_ENTITY_NUDGE_Y_POS 0x00001000UL
#define FACING_LINE_ENTITY_NUDGE_X_NEG 0x00004000UL

typedef struct FacingCandidate_s {
    uint16_t spriteIndex;
    uint16_t lineIndex;
    uint16_t defTile;
    uint16_t tileIndex;
    uint8_t type;
    uint8_t subtype;
    uint8_t distance;
    uint8_t isLine;
    uint8_t found;
} FacingCandidate;

static EspNativeGameplayFacingLabelView facing;

static int typeInTraceMask(uint8_t type) {
    return type < 32U && (FACING_TRACE_MASK & (1UL << type)) != 0U;
}

static int cardinalStep(int32_t angle, int32_t* outX, int32_t* outY) {
    if (outX == NULL || outY == NULL) return 0;
    *outX = 0;
    *outY = 0;
    switch (angle & 255) {
    case 0:   *outX = FACING_TILE_SIZE; return 1;
    case 64:  *outY = -FACING_TILE_SIZE; return 1;
    case 128: *outX = -FACING_TILE_SIZE; return 1;
    case 192: *outY = FACING_TILE_SIZE; return 1;
    default: return 0;
    }
}

static int tileForPoint(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || x < 0 || y < 0) return 0;
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= FACING_MAP_WIDTH || tileY >= FACING_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * FACING_MAP_WIDTH + tileX);
    return 1;
}

static int lineEntityTile(const EspMapLine* line, uint16_t* outTile) {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    int32_t x;
    int32_t y;

    if (line == NULL || outTile == NULL) return 0;
    x1 = (int32_t)line->x1;
    y1 = (int32_t)line->y1;
    x2 = (int32_t)line->x2;
    y2 = (int32_t)line->y2;

    if ((line->flags & FACING_LINE_GEOMETRY_X_NUDGE) != 0U) {
        if ((line->flags & FACING_LINE_GEOMETRY_AXIS_X) != 0U) {
            x1 += 3; x2 += 3;
        }
        else if ((line->flags & FACING_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            x1 -= 3; x2 -= 3;
        }
    }
    else if ((line->flags & FACING_LINE_GEOMETRY_Y_NUDGE) != 0U) {
        if ((line->flags & FACING_LINE_GEOMETRY_AXIS_X) != 0U) {
            y1 += 3; y2 += 3;
        }
        else if ((line->flags & FACING_LINE_GEOMETRY_AXIS_NEG) != 0U) {
            y1 -= 3; y2 -= 3;
        }
    }

    x = x1 + ((x2 - x1) / 2);
    y = y1 + ((y2 - y1) / 2);
    if ((line->flags & FACING_LINE_ENTITY_NUDGE_Y_NEG) != 0U) --y;
    else if ((line->flags & FACING_LINE_ENTITY_NUDGE_X_POS) != 0U) ++x;
    else if ((line->flags & FACING_LINE_ENTITY_NUDGE_Y_POS) != 0U) ++y;
    else if ((line->flags & FACING_LINE_ENTITY_NUDGE_X_NEG) != 0U) --x;

    return tileForPoint(x, y, outTile);
}

static int findLineOnTile(uint16_t tile,
                          uint8_t distance,
                          FacingCandidate* out) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;

    if (runtime == NULL || out == NULL) return -1;
    i = runtime->lineCount;
    while (i > 0U) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t type = FACING_ENTITY_TYPE_HUD_HIDDEN;
        uint8_t subtype = 0U;
        uint16_t defTile = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;

        --i;
        if (!EspMapLineState_getOpen(i, &open) ||
            !EspMapRuntime_getLine(i, &line)) {
            return -1;
        }
        if (open != 0U) continue;

        lookup = FACING_LINE_DEF_BASE + (uint32_t)line.texture;
        if (lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
            EspEntityDefTypeCatalog_getTypeAndSubtype(
                (uint16_t)lookup, &type, &subtype)) {
            defTile = (uint16_t)lookup;
        }
        else {
            /* Legacy Game_loadMapEntities() gives lines flagged 8/16 the
             * generic entities[0] definition. That is eType 9: it blocks the
             * trace but Hud_drawTopBar deliberately suppresses its label. */
            if ((line.flags & FACING_LINE_FALLBACK_FLAGS) == 0U) continue;
            type = FACING_ENTITY_TYPE_HUD_HIDDEN;
            subtype = 0U;
        }
        if (!typeInTraceMask(type)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile != tile) continue;

        memset(out, 0, sizeof(*out));
        out->spriteIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
        out->lineIndex = (uint16_t)i;
        out->defTile = defTile;
        out->tileIndex = tile;
        out->type = type;
        out->subtype = subtype;
        out->distance = distance;
        out->isLine = 1U;
        out->found = 1U;
        return 1;
    }
    return 0;
}

static int specialSpriteCrosses(const EspMapSprite* sprite,
                                int32_t sourceX,
                                int32_t sourceY,
                                int32_t destX,
                                int32_t destY) {
    int32_t sprX;
    int32_t sprY;

    if (sprite == NULL ||
        (sprite->info & FACING_SPECIAL_TRACE_FLAG) == 0U) {
        return 0;
    }
    sprX = (int32_t)sprite->x;
    sprY = (int32_t)sprite->y;
    if ((sprite->info & FACING_SPECIAL_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite->info & FACING_SPECIAL_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
}

static int spriteDefTile(uint32_t spriteIndex,
                         const EspMapSprite* sprite,
                         uint8_t type,
                         uint8_t subtype,
                         uint16_t* outDefTile) {
    uint16_t transformed;
    uint32_t lookup;
    uint8_t checkType;
    uint8_t checkSubtype;

    if (sprite == NULL || outDefTile == NULL) return 0;
    if (EspNativeGameplayCrateState_effectiveDefTile(
            spriteIndex, &transformed)) {
        *outDefTile = transformed;
        return 1;
    }

    lookup = sprite->info & FACING_SPRITE_DEF_MASK;
    if ((sprite->info & FACING_SPRITE_DEF_TILE_FLAG) != 0U) {
        lookup += FACING_SPRITE_DEF_TILE_BASE;
    }
    if (lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
        EspEntityDefTypeCatalog_getTypeAndSubtype(
            (uint16_t)lookup, &checkType, &checkSubtype)) {
        *outDefTile = (uint16_t)lookup;
        return 1;
    }

    return EspEntityDefTypeCatalog_findTileIndex(type, subtype, outDefTile);
}

static int findSpriteOnTile(uint16_t tile,
                            uint16_t sourceTile,
                            uint8_t distance,
                            int32_t traceSourceX,
                            int32_t traceSourceY,
                            int32_t traceDestX,
                            int32_t traceDestY,
                            FacingCandidate* out) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t best = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint16_t bestDef = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    uint8_t bestType = 0xffU;
    uint8_t bestSubtype = 0xffU;
    uint32_t i;

    if (topology == NULL || out == NULL) return -1;
    for (i = 0U; i < topology->spriteCount; ++i) {
        EspMapSprite sprite;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t rawTile;
        uint16_t defTile;

        if (!EspMapSpriteTopology_getEntity(
                i, &type, &subtype, &linkState, &linkOrder)) {
            return -1;
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !typeInTraceMask(type)) {
            continue;
        }
        if (!EspMapRuntime_getMapSprite(i, &sprite)) return -1;

        if (type == 14U || type == 15U) {
            if (!specialSpriteCrosses(&sprite,
                                      traceSourceX, traceSourceY,
                                      traceDestX, traceDestY)) {
                continue;
            }
        }

        /* Legacy checkFacingEntity ignores ordinary sprite entities linked on
         * the player's source tile unless their physical sprite tile differs
         * from the trace-source tile. Type 14 and line entities are explicit
         * exceptions. */
        if (tile == sourceTile && type != 14U) {
            if (!tileForPoint((int32_t)sprite.x, (int32_t)sprite.y,
                              &rawTile)) {
                return -1;
            }
            if (rawTile == sourceTile) continue;
        }

        defTile = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
        (void)spriteDefTile(i, &sprite, type, subtype, &defTile);

        if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            linkOrder > bestOrder) {
            best = (uint16_t)i;
            bestOrder = linkOrder;
            bestDef = defTile;
            bestType = type;
            bestSubtype = subtype;
        }
    }

    if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) return 0;
    memset(out, 0, sizeof(*out));
    out->spriteIndex = best;
    out->lineIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    out->defTile = bestDef;
    out->tileIndex = tile;
    out->type = bestType;
    out->subtype = bestSubtype;
    out->distance = distance;
    out->isLine = 0U;
    out->found = 1U;
    return 1;
}

static int samePresentation(const EspNativeGameplayFacingLabelView* a,
                            const EspNativeGameplayFacingLabelView* b) {
    EspNativeGameplayFacingLabelView left;
    EspNativeGameplayFacingLabelView right;
    if (a == NULL || b == NULL) return 0;
    left = *a;
    right = *b;
    left.dirty = 0U;
    right.dirty = 0U;
    return memcmp(&left, &right, sizeof(left)) == 0;
}

void EspNativeGameplayFacingLabel_reset(void) {
    memset(&facing, 0, sizeof(facing));
    facing.spriteIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    facing.lineIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    facing.defTile = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    facing.tileIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    facing.type = 0xffU;
    facing.subtype = 0xffU;
    facing.dirty = 1U;
}

int EspNativeGameplayFacingLabel_refresh(const char* reason) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayFacingLabelView next;
    FacingCandidate candidate;
    int32_t stepX;
    int32_t stepY;
    int32_t sourceX;
    int32_t sourceY;
    int32_t destX;
    int32_t destY;
    uint16_t sourceTile;
    uint32_t distance;
    int found = 0;

    if (view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle ||
        !EspMapRuntime_isLoaded() || !EspMapSpriteTopology_isReady() ||
        !EspMapState_isReady() || !EspMapLineState_isReady() ||
        !EspEntityDefTypeCatalog_isReady() ||
        !cardinalStep(view->destAngle, &stepX, &stepY)) {
        return 0;
    }

    sourceX = view->destX + (stepX > 0 ? 31 : (stepX < 0 ? -31 : 0));
    sourceY = view->destY + (stepY > 0 ? 31 : (stepY < 0 ? -31 : 0));
    destX = sourceX + stepX * (int32_t)FACING_TRACE_STEPS;
    destY = sourceY + stepY * (int32_t)FACING_TRACE_STEPS;
    if (!tileForPoint(sourceX, sourceY, &sourceTile)) return 0;

    memset(&candidate, 0, sizeof(candidate));
    candidate.spriteIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    candidate.lineIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    candidate.defTile = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;

    for (distance = 0U; distance <= FACING_TRACE_STEPS; ++distance) {
        int32_t pointX = sourceX + stepX * (int32_t)distance;
        int32_t pointY = sourceY + stepY * (int32_t)distance;
        uint16_t tile;
        uint8_t tileFlags;
        int lineFound;
        int spriteFound;

        if (!tileForPoint(pointX, pointY, &tile)) break;

        /* Line entities are linked after map-sprite entities in legacy
         * Game_loadMapEntities(), so they occupy the head of a shared tile. */
        lineFound = findLineOnTile(tile, (uint8_t)distance, &candidate);
        if (lineFound < 0) return 0;
        if (lineFound > 0) {
            found = 1;
            break;
        }

        spriteFound = findSpriteOnTile(tile, sourceTile, (uint8_t)distance,
                                       sourceX, sourceY, destX, destY,
                                       &candidate);
        if (spriteFound < 0) return 0;
        if (spriteFound > 0) {
            found = 1;
            break;
        }

        if (!EspMapState_getTileFlags(tile, &tileFlags)) return 0;
        if ((tileFlags & ESP_MAP_TILE_WALL) != 0U) {
            /* Equivalent display result to the legacy type-9 wall sentinel:
             * stop the ray and leave the top-bar fallback empty. */
            break;
        }
    }

    memset(&next, 0, sizeof(next));
    next.spriteIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    next.lineIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    next.defTile = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    next.tileIndex = ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX;
    next.type = 0xffU;
    next.subtype = 0xffU;

    if (found) {
        next.spriteIndex = candidate.spriteIndex;
        next.lineIndex = candidate.lineIndex;
        next.defTile = candidate.defTile;
        next.tileIndex = candidate.tileIndex;
        next.type = candidate.type;
        next.subtype = candidate.subtype;
        next.distance = candidate.distance;
        next.isLine = candidate.isLine;
        next.active = 1U;

        if (candidate.type != FACING_ENTITY_TYPE_HUD_HIDDEN &&
            candidate.defTile != ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX &&
            EspEntityDefTypeCatalog_readName(
                candidate.defTile, next.name, sizeof(next.name))) {
            next.displayable = 1U;
        }
    }

    next.dirty = samePresentation(&facing, &next) ? facing.dirty : 1U;
    facing = next;

    printf("[FACINGLABEL] REFRESH reason=%s active=%u display=%u source=%s index=%u tile=%u distance=%u type=%u subtype=%u def=%u name=\"%s\" dirty=%u ownerBytes=%u traceSteps=%u\n",
           reason != NULL ? reason : "world",
           (unsigned int)facing.active,
           (unsigned int)facing.displayable,
           facing.isLine != 0U ? "line" :
               (facing.active != 0U ? "sprite" : "none"),
           (unsigned int)(facing.isLine != 0U
                              ? facing.lineIndex : facing.spriteIndex),
           (unsigned int)facing.tileIndex,
           (unsigned int)facing.distance,
           (unsigned int)facing.type,
           (unsigned int)facing.subtype,
           (unsigned int)facing.defTile,
           facing.name,
           (unsigned int)facing.dirty,
           (unsigned int)sizeof(facing),
           (unsigned int)FACING_TRACE_STEPS);
    return 1;
}

const EspNativeGameplayFacingLabelView* EspNativeGameplayFacingLabel_view(void) {
    return &facing;
}

int EspNativeGameplayFacingLabel_isDirty(void) {
    return facing.dirty != 0U;
}

void EspNativeGameplayFacingLabel_markPainted(void) {
    facing.dirty = 0U;
}
