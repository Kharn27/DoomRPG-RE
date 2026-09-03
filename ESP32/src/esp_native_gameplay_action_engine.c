#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_destructible.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_native_indexed_bmp.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define ACTION_TRACE_MASK 0x5687U
#define ACTION_TRACE_TILES 8U
#define ACTION_MAX_SPRITES 1024U
#define ACTION_REMOVED_BYTES (ACTION_MAX_SPRITES / 8U)
#define ACTION_VISUAL_HIDDEN 0x80U

#define ACTION_ENTITY_ENEMY 1U
#define ACTION_ENTITY_HUMAN 2U
#define ACTION_ENTITY_FIRE 10U
#define ACTION_ENTITY_DESTRUCTIBLE 12U
#define ACTION_DESTRUCTIBLE_JAMMED_SUBTYPE 3U
#define ACTION_WEAPON_AXE 0U
#define ACTION_WEAPON_EXTINGUISHER 1U
#define ACTION_EXTINGUISHER_AMMO_TYPE 0U
#define ACTION_EXTINGUISHER_AMMO_USAGE 1U

/* Legacy Player_reset accuracy=16, Combat_calcHit() derives dummy agility=12
 * and the axe adds its range term: 170 + 89 = 259. randHit is one byte, so
 * the first supported jammed-door hit is guaranteed while still consuming the
 * exact RNG byte. Player-stat/level mutation remains deferred, so this bounded
 * route is invalidated before any native accuracy mutation can exist. */
#define ACTION_JAMMED_DOOR_CALC_HIT 259U

#define TILE_SIZE 64
#define TILE_CENTER 32
#define MAP_WIDTH 32
#define MAP_MAX_CENTER (((MAP_WIDTH - 1) * TILE_SIZE) + TILE_CENTER)

#define SPECIAL_TRACE_ENTITY_FLAG 0x00020000UL
#define SPECIAL_TRACE_Y_MASK 0x00180000UL
#define SPECIAL_TRACE_X_MASK 0x00600000UL
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

#define FEEDBACK_TOP_HEIGHT 20U
#define FEEDBACK_FONT_WIDTH 9U
#define FEEDBACK_FONT_HEIGHT 12U
#define FEEDBACK_FONT_ADVANCE 7
#define FEEDBACK_FONT_SOURCE_WIDTH 144U
#define FEEDBACK_FONT_SOURCE_HEIGHT 72U
#define FEEDBACK_TEXT_X 1
#define FEEDBACK_TEXT_Y 5
#define FEEDBACK_MAX_VISIBLE_CHARS 21U
#define FEEDBACK_TRANSPARENT 1U
#define FEEDBACK_OPAQUE 0U
#define FEEDBACK_DISPLAY_MS 1200U

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native action feedback requires the 160x120 logical framebuffer"
#endif

typedef EspNativeGameplayActionFeedback ActionFeedback;
#define ACTION_FEEDBACK_NONE ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE
#define ACTION_FEEDBACK_NOTHING ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NOTHING
#define ACTION_FEEDBACK_FIRE_CLEARED ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_FIRE_CLEARED
#define ACTION_FEEDBACK_DOOR_CLEARED ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DOOR_CLEARED
#define ACTION_FEEDBACK_PASS_TURN ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN

typedef enum ActionRoute_e {
    ACTION_ROUTE_INVALID = 0,
    ACTION_ROUTE_NOTHING = 1,
    ACTION_ROUTE_FIRE_CLEARED = 2,
    ACTION_ROUTE_HUMAN = 3,
    ACTION_ROUTE_ENEMY_DEFERRED = 4,
    ACTION_ROUTE_JAMMED_DOOR_CLEARED = 5,
    ACTION_ROUTE_DESTRUCTIBLE_DEFERRED = 6
} ActionRoute;

typedef struct ActionTarget_s {
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint16_t lineIndex;
    uint8_t type;
    uint8_t subtype;
    uint8_t distance;
    uint8_t isLine;
} ActionTarget;

typedef struct ActionPending_s {
    uint32_t sequence;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint16_t lineIndex;
    uint8_t route;
    uint8_t feedback;
    uint8_t type;
    uint8_t subtype;
    uint8_t weapon;
    uint8_t distance;
    uint8_t worldChanged;
    uint8_t active;
} ActionPending;

typedef struct ActionEngineState_s {
    uint8_t removedBits[ACTION_REMOVED_BYTES];
    ActionPending pending;
    EspNativeGameplayDestructibleResult destructibleUndo;
    uint32_t arenaFNV;
    uint32_t selects;
    uint32_t fireClears;
    uint32_t jammedDoorClears;
    uint32_t noUses;
    uint32_t combatDeferred;
    uint32_t destructibleDeferred;
    uint32_t deferredXp;
    uint32_t feedbackShownAtMs;
    uint16_t spriteCount;
    uint8_t targetMapId;
    uint8_t ready;
    uint8_t feedbackPending;
    uint8_t feedbackKind;
    uint8_t feedbackVisible;
    uint8_t feedbackVisibleKind;
} ActionEngineState;

typedef struct FeedbackScratch_s {
    EspNativeIndexedBmp bar;
    EspNativeIndexedBmp font;
} FeedbackScratch;

static ActionEngineState actionState;

EspNativeGameplayActionStatus __real_EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);
int __real_EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState);
int __real_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder);
int __real_Esp32PlatformVideo_present(void);

static uint32_t actionNowMs(void) {
    /* Keep legacy/ESP32 header boundaries clean: this recovered runtime clock
     * is already the canonical millisecond source for the native gameplay
     * service, so do not pull ESP-IDF timer headers into legacy C units. */
    return DoomRPG_GetUpTimeMS();
}

static int centeredCoordinate(int32_t value) {
    return value >= TILE_CENTER && value <= MAP_MAX_CENTER &&
           (value & (TILE_SIZE - 1)) == TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MAP_WIDTH || tileY >= MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * MAP_WIDTH + tileX);
    return 1;
}

static int entityTypeInTraceMask(uint8_t type) {
    return type < 16U && (ACTION_TRACE_MASK & (1U << type)) != 0U;
}

static int removed(uint32_t spriteIndex) {
    return actionState.ready == 1U && spriteIndex < actionState.spriteCount &&
           ((actionState.removedBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setRemoved(uint32_t spriteIndex, int value) {
    uint8_t mask;
    if (spriteIndex >= actionState.spriteCount ||
        spriteIndex >= ACTION_MAX_SPRITES) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    if (value) actionState.removedBits[spriteIndex >> 3] |= mask;
    else actionState.removedBits[spriteIndex >> 3] &= (uint8_t)~mask;
}

static int actionGetEntity(uint32_t spriteIndex,
                           uint8_t* outType,
                           uint8_t* outSubType,
                           uint16_t* outLinkState,
                           uint16_t* outLinkOrder) {
    if (!__real_EspMapSpriteTopology_getEntity(spriteIndex, outType, outSubType,
                                               outLinkState, outLinkOrder)) {
        return 0;
    }
    if (removed(spriteIndex) && outLinkState != NULL) {
        *outLinkState &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                                     ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
    }
    return 1;
}

static void logCorpus(void) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t fires = 0U;
    uint32_t humans = 0U;
    uint32_t enemies = 0U;
    uint32_t destructibles = 0U;
    uint32_t i;

    if (topology == NULL) return;
    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!actionGetEntity(i, &type, &subtype, &linkState, &linkOrder)) return;
        (void)subtype;
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) continue;
        if (type == ACTION_ENTITY_FIRE) ++fires;
        else if (type == ACTION_ENTITY_HUMAN) ++humans;
        else if (type == ACTION_ENTITY_ENEMY) ++enemies;
        else if (type == ACTION_ENTITY_DESTRUCTIBLE) ++destructibles;
    }

    printf("[ACTIONENGINE] READY map=%u arena=%08x sprites=%u ownerBytes=%u traceMask=%04x traceTiles=%u fires=%u humans=%u enemies=%u destructibles=%u eventFirst=yes feedbackMs=%u ammo=playerState monsterCombat=deferred jammedDoor3=axe-adjacent-owned otherDestructibles=deferred\n",
           (unsigned int)actionState.targetMapId,
           (unsigned int)actionState.arenaFNV,
           (unsigned int)actionState.spriteCount,
           (unsigned int)sizeof(actionState),
           (unsigned int)ACTION_TRACE_MASK,
           (unsigned int)ACTION_TRACE_TILES,
           (unsigned int)fires,
           (unsigned int)humans,
           (unsigned int)enemies,
           (unsigned int)destructibles,
           (unsigned int)FEEDBACK_DISPLAY_MS);
}

static int ensureOwner(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    const EspPlayerViewState* view = EspPlayerView_view();

    if (runtime == NULL || runtime->arenaFNV1a == 0U || topology == NULL ||
        view == NULL || view->active != 1U || view->targetMapId == 0U ||
        runtime->mapSpriteCount != topology->spriteCount ||
        runtime->mapSpriteCount > ACTION_MAX_SPRITES ||
        !EspMapState_isReady() || !EspMapLineState_isReady() ||
        !EspEntityDefTypeCatalog_isReady()) {
        return 0;
    }

    if (!actionState.ready || actionState.arenaFNV != runtime->arenaFNV1a ||
        actionState.spriteCount != runtime->mapSpriteCount ||
        actionState.targetMapId != view->targetMapId) {
        memset(&actionState, 0, sizeof(actionState));
        EspNativeGameplayWeapon_cancelAttack();
        actionState.arenaFNV = runtime->arenaFNV1a;
        actionState.spriteCount = (uint16_t)runtime->mapSpriteCount;
        actionState.targetMapId = view->targetMapId;
        actionState.ready = 1U;
        logCorpus();
    }
    return 1;
}

int EspNativeGameplayActionEngine_queueFeedback(
    EspNativeGameplayActionFeedback feedback) {
    if (feedback <= ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE ||
        feedback > ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN ||
        !ensureOwner() || actionState.pending.active != 0U ||
        actionState.feedbackPending != 0U) {
        return 0;
    }
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = (uint8_t)feedback;
    return 1;
}

int EspNativeGameplayActionEngine_cancelQueuedFeedback(
    EspNativeGameplayActionFeedback feedback) {
    if (actionState.feedbackPending == 0U ||
        actionState.feedbackKind != (uint8_t)feedback) {
        return 0;
    }
    actionState.feedbackPending = 0U;
    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
    return 1;
}

int __wrap_EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState) {
    if (!__real_EspMapSpriteTopology_getVisualState(spriteIndex,
                                                    outVisualState)) {
        return 0;
    }
    if (outVisualState != NULL && removed(spriteIndex)) {
        *outVisualState |= ACTION_VISUAL_HIDDEN;
    }
    return 1;
}

int __wrap_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder) {
    return actionGetEntity(spriteIndex, outType, outSubType,
                           outLinkState, outLinkOrder);
}

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
    *outTile = (uint16_t)(tileY * MAP_WIDTH + tileX);
    return 1;
}

static int findLinkedLineBlocker(uint16_t tile,
                                 uint16_t* outLineIndex,
                                 uint8_t* outType,
                                 uint8_t* outSubtype) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;

    if (runtime == NULL || outLineIndex == NULL || outType == NULL ||
        outSubtype == NULL) return -1;
    i = runtime->lineCount;
    while (i > 0U) {
        EspMapLine line;
        uint32_t lookup;
        uint16_t lineTile;
        uint8_t open;
        uint8_t type;
        uint8_t subtype;
        int hasDefinition;

        --i;
        if (!EspMapLineState_getOpen(i, &open)) return -1;
        if (open != 0U) continue;
        if (!EspMapRuntime_getLine(i, &line)) return -1;
        lookup = LINE_ENTITY_DEF_BASE + (uint32_t)line.texture;
        hasDefinition = lookup < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
                        EspEntityDefTypeCatalog_getTypeAndSubtype(
                            (uint16_t)lookup, &type, &subtype);
        if (!hasDefinition) {
            if ((line.flags & LINE_ENTITY_FALLBACK_FLAGS) == 0U) continue;
            type = 0U;
            subtype = 0xffU;
        }
        if (!entityTypeInTraceMask(type)) continue;
        if (!lineEntityTile(&line, &lineTile)) return -1;
        if (lineTile != tile) continue;
        *outLineIndex = (uint16_t)i;
        *outType = type;
        *outSubtype = subtype;
        return 1;
    }
    return 0;
}

static int specialEntityBlocks(uint32_t spriteIndex,
                               int32_t sourceX,
                               int32_t sourceY,
                               int32_t destX,
                               int32_t destY) {
    EspMapSprite sprite;
    int32_t sprX;
    int32_t sprY;

    if (!EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return -1;
    if ((sprite.info & SPECIAL_TRACE_ENTITY_FLAG) == 0U) return 0;
    sprX = (int32_t)sprite.x;
    sprY = (int32_t)sprite.y;
    if ((sprite.info & SPECIAL_TRACE_Y_MASK) != 0U) {
        return (sourceY <= sprY && destY > sprY) ||
               (sourceY >= sprY && destY < sprY);
    }
    if ((sprite.info & SPECIAL_TRACE_X_MASK) != 0U) {
        return (sourceX <= sprX && destX > sprX) ||
               (sourceX >= sprX && destX < sprX);
    }
    return 0;
}

static int findSpriteOnTile(uint16_t tile,
                            int32_t sourceX,
                            int32_t sourceY,
                            int32_t destX,
                            int32_t destY,
                            ActionTarget* outTarget) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint16_t best = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint8_t bestType = 0xffU;
    uint8_t bestSubtype = 0xffU;
    uint32_t i;

    if (topology == NULL || outTarget == NULL) return -1;
    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        int specialBlocks;
        if (!actionGetEntity(i, &type, &subtype, &linkState, &linkOrder)) {
            return -1;
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !entityTypeInTraceMask(type)) {
            continue;
        }
        if (type == 14U) {
            specialBlocks = specialEntityBlocks(i, sourceX, sourceY,
                                                destX, destY);
            if (specialBlocks < 0) return -1;
            if (specialBlocks == 0) continue;
        }
        if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            linkOrder > bestOrder) {
            best = (uint16_t)i;
            bestOrder = linkOrder;
            bestType = type;
            bestSubtype = subtype;
        }
    }

    if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) return 0;
    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->spriteIndex = best;
    outTarget->lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outTarget->tileIndex = tile;
    outTarget->type = bestType;
    outTarget->subtype = bestSubtype;
    return 1;
}

static int traceAction(ActionTarget* outTarget) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayTurnState* turn = EspNativeGameplayDispatch_view();
    int32_t sourceX;
    int32_t sourceY;
    uint32_t distance;

    if (outTarget == NULL || view == NULL || turn == NULL ||
        view->active != 1U || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        turn->active != 1U ||
        !((turn->viewStepX == TILE_SIZE && turn->viewStepY == 0) ||
          (turn->viewStepX == -TILE_SIZE && turn->viewStepY == 0) ||
          (turn->viewStepX == 0 && turn->viewStepY == TILE_SIZE) ||
          (turn->viewStepX == 0 && turn->viewStepY == -TILE_SIZE))) {
        return -1;
    }

    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outTarget->lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    sourceX = view->destX;
    sourceY = view->destY;

    for (distance = 1U; distance <= ACTION_TRACE_TILES; ++distance) {
        int32_t destX = view->destX + turn->viewStepX * (int32_t)distance;
        int32_t destY = view->destY + turn->viewStepY * (int32_t)distance;
        uint16_t tile;
        uint8_t tileFlags;
        uint16_t lineIndex;
        uint8_t lineType;
        uint8_t lineSubtype;
        int lineBlocker;
        int spriteBlocker;

        if (!tileIndexFor(destX, destY, &tile)) return 0;
        if (!EspMapState_getTileFlags(tile, &tileFlags)) return -1;
        if ((tileFlags & ESP_MAP_TILE_WALL) != 0U) return 0;

        lineBlocker = findLinkedLineBlocker(tile, &lineIndex, &lineType,
                                            &lineSubtype);
        if (lineBlocker < 0) return -1;
        if (lineBlocker > 0) {
            memset(outTarget, 0, sizeof(*outTarget));
            outTarget->spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
            outTarget->lineIndex = lineIndex;
            outTarget->tileIndex = tile;
            outTarget->type = lineType;
            outTarget->subtype = lineSubtype;
            outTarget->distance = (uint8_t)distance;
            outTarget->isLine = 1U;
            return 1;
        }

        spriteBlocker = findSpriteOnTile(tile, sourceX, sourceY,
                                         destX, destY, outTarget);
        if (spriteBlocker < 0) return -1;
        if (spriteBlocker > 0) {
            outTarget->distance = (uint8_t)distance;
            return 1;
        }
        sourceX = destX;
        sourceY = destY;
    }
    return 0;
}

static ActionRoute routeTarget(const ActionTarget* target, uint8_t weapon) {
    if (target == NULL) return ACTION_ROUTE_INVALID;
    if (target->type == ACTION_ENTITY_HUMAN) return ACTION_ROUTE_HUMAN;
    if (target->type == ACTION_ENTITY_FIRE) {
        return weapon == ACTION_WEAPON_EXTINGUISHER
                   ? ACTION_ROUTE_FIRE_CLEARED
                   : ACTION_ROUTE_NOTHING;
    }
    if (target->type == ACTION_ENTITY_ENEMY) return ACTION_ROUTE_ENEMY_DEFERRED;
    if (target->type == ACTION_ENTITY_DESTRUCTIBLE) {
        if (target->isLine != 0U &&
            target->subtype == ACTION_DESTRUCTIBLE_JAMMED_SUBTYPE &&
            weapon == ACTION_WEAPON_AXE && target->distance == 1U) {
            return ACTION_ROUTE_JAMMED_DOOR_CLEARED;
        }
        return ACTION_ROUTE_DESTRUCTIBLE_DEFERRED;
    }
    return ACTION_ROUTE_NOTHING;
}

static const char* routeName(ActionRoute route) {
    switch (route) {
    case ACTION_ROUTE_NOTHING: return "NOTHING_TO_USE";
    case ACTION_ROUTE_FIRE_CLEARED: return "FIRE_CLEARED";
    case ACTION_ROUTE_HUMAN: return "HUMAN_NO_FIRE";
    case ACTION_ROUTE_ENEMY_DEFERRED: return "ENEMY_COMBAT_DEFERRED";
    case ACTION_ROUTE_JAMMED_DOOR_CLEARED: return "JAMMED_DOOR_CLEARED";
    case ACTION_ROUTE_DESTRUCTIBLE_DEFERRED:
        return "DESTRUCTIBLE_COMBAT_DEFERRED";
    default: return "INVALID";
    }
}

static const char* feedbackText(uint8_t feedback) {
    if (feedback == ACTION_FEEDBACK_NOTHING) return "Nothing to use";
    if (feedback == ACTION_FEEDBACK_FIRE_CLEARED) return "Fire cleared!";
    if (feedback == ACTION_FEEDBACK_DOOR_CLEARED) return "Door cleared!";
    if (feedback == ACTION_FEEDBACK_PASS_TURN) return "Turn passed.";
    return NULL;
}

static int drawGlyph(const EspNativeIndexedBmp* font,
                     uint16_t* framebuffer,
                     uint8_t c,
                     int x,
                     int y,
                     EspNativeIndexedBmpStats* stats) {
    uint8_t glyph;
    if (c < 33U || c > 127U) return 0;
    glyph = (uint8_t)(c - 33U);
    return EspNativeIndexedBmp_blit(
               font, framebuffer,
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
               (uint16_t)(FEEDBACK_FONT_WIDTH * (glyph & 0x0fU)),
               (uint16_t)(FEEDBACK_FONT_HEIGHT * (glyph >> 4)),
               FEEDBACK_FONT_WIDTH, FEEDBACK_FONT_HEIGHT,
               (int16_t)x, (int16_t)y,
               FEEDBACK_TRANSPARENT, stats) == ESP_NATIVE_INDEXED_BMP_OK;
}

static int paintFeedback(uint8_t feedback) {
    FeedbackScratch scratch;
    EspNativeIndexedBmpStats stats;
    const char* text = feedbackText(feedback);
    uint16_t* framebuffer;
    size_t framebufferBytes;
    size_t visible = 0U;
    size_t i;
    int x = FEEDBACK_TEXT_X;
    int ok = 0;

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                                DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t) ||
        EspAssetPack_isOpen()) {
        return 0;
    }

    memset(&scratch, 0, sizeof(scratch));
    memset(&stats, 0, sizeof(stats));
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    if (EspNativeIndexedBmp_open("k.bmp", &scratch.bar, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        scratch.bar.width != 20U || scratch.bar.height != FEEDBACK_TOP_HEIGHT ||
        EspNativeIndexedBmp_tile(
            &scratch.bar, framebuffer,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            0, 0, DOOMRPG_LOGICAL_WIDTH, FEEDBACK_TOP_HEIGHT,
            FEEDBACK_OPAQUE, &stats) != ESP_NATIVE_INDEXED_BMP_OK) {
        goto done;
    }

    if (text != NULL) {
        size_t length;
        if (EspNativeIndexedBmp_open("a.bmp", &scratch.font, &stats) !=
                ESP_NATIVE_INDEXED_BMP_OK ||
            scratch.font.width != FEEDBACK_FONT_SOURCE_WIDTH ||
            scratch.font.height != FEEDBACK_FONT_SOURCE_HEIGHT) {
            goto done;
        }

        length = strlen(text);
        visible = length > FEEDBACK_MAX_VISIBLE_CHARS
                      ? FEEDBACK_MAX_VISIBLE_CHARS
                      : length;
        for (i = 0U; i < visible; ++i) {
            uint8_t c = (uint8_t)text[i];
            if (c == ' ') {
                x += FEEDBACK_FONT_ADVANCE;
                continue;
            }
            if (!drawGlyph(&scratch.font, framebuffer, c,
                           x, FEEDBACK_TEXT_Y, &stats)) {
                goto done;
            }
            x += FEEDBACK_FONT_ADVANCE;
        }

        printf("[ACTIONFEEDBACK] PAINT kind=%u text=\"%s\" chars=%u reads=%u bytes=%u present=caller durationMs=%u\n",
               (unsigned int)feedback,
               text,
               (unsigned int)visible,
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead,
               (unsigned int)FEEDBACK_DISPLAY_MS);
    }
    else {
        printf("[ACTIONFEEDBACK] CLEAR mode=topbar-only reads=%u bytes=%u present=caller\n",
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead);
    }
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    return ok;
}

int __wrap_Esp32PlatformVideo_present(void) {
    uint8_t feedback = ACTION_FEEDBACK_NONE;
    int hadFeedback = 0;
    int ok;

    if (actionState.feedbackPending != 0U) {
        feedback = actionState.feedbackKind;
        if (!paintFeedback(feedback)) {
            printf("[ACTIONFEEDBACK] FAILED kind=%u\n", (unsigned int)feedback);
            return 0;
        }
        hadFeedback = 1;
    }

    ok = __real_Esp32PlatformVideo_present();
    if (!ok) return 0;

    if (hadFeedback) {
        actionState.feedbackPending = 0U;
        actionState.feedbackKind = ACTION_FEEDBACK_NONE;
        if (feedback != ACTION_FEEDBACK_NONE) {
            actionState.feedbackVisible = 1U;
            actionState.feedbackVisibleKind = feedback;
            actionState.feedbackShownAtMs = actionNowMs();
        }
        else {
            actionState.feedbackVisible = 0U;
            actionState.feedbackVisibleKind = ACTION_FEEDBACK_NONE;
            actionState.feedbackShownAtMs = 0U;
        }
    }
    /* A plain external present does not prove that the top message bar was
     * repainted. Keep the feedback lease alive until its explicit timeout (or
     * until another feedback paint replaces it), otherwise touch/move presents
     * can strand the already-painted text forever. */
    return 1;
}

static int serviceFeedbackExpiry(void) {
    uint32_t now;
    uint32_t elapsed;
    uint8_t kind;

    if (actionState.feedbackVisible == 0U) return 1;
    now = actionNowMs();
    elapsed = now - actionState.feedbackShownAtMs;
    if (elapsed < FEEDBACK_DISPLAY_MS) return 1;

    /* Touch feedback owns a strict framebuffer snapshot until its 120 ms lease
     * is restored. Do not mutate the top bar underneath that lease: the next
     * resident service restores the touch overlay first, then this expiry may
     * safely repaint/present the message bar. */
    if (EspNativeGameplayControls_isActive()) return 1;

    kind = actionState.feedbackVisibleKind;
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
    if (!__wrap_Esp32PlatformVideo_present()) return 0;
    printf("[ACTIONFEEDBACK] EXPIRE kind=%u elapsedMs=%u targetMs=%u restored=topbar-only\n",
           (unsigned int)kind,
           (unsigned int)elapsed,
           (unsigned int)FEEDBACK_DISPLAY_MS);
    return 1;
}

EspNativeGameplayActionStatus __wrap_EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status =
        __real_EspNativeGameplayAction_executeSelect(intent, outResult);
    ActionTarget target;
    ActionRoute route;
    const EspNativeGameplayHudState* hud;
    int traceStatus;
    uint8_t weapon;

    if (status != ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT &&
        status != ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE) {
        return status;
    }
    if (!ensureOwner() || intent == NULL || outResult == NULL ||
        actionState.pending.active != 0U) {
        printf("[ACTIONENGINE] DEFER seq=%u reason=owner-or-pending status=%s\n",
               intent != NULL ? (unsigned int)intent->sequence : 0U,
               EspNativeGameplayAction_statusName(status));
        return status;
    }

    hud = EspNativeGameplayHud_view();
    if (hud == NULL || hud->active != 1U || hud->painted != 1U) {
        printf("[ACTIONENGINE] DEFER seq=%u reason=hud-not-ready\n",
               (unsigned int)intent->sequence);
        return status;
    }
    weapon = hud->model.weapon;
    memset(&target, 0, sizeof(target));
    target.spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    target.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    traceStatus = traceAction(&target);
    if (traceStatus < 0) {
        printf("[ACTIONENGINE] DEFER seq=%u reason=trace-not-ready\n",
               (unsigned int)intent->sequence);
        return status;
    }

    if (traceStatus == 0) {
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.route = ACTION_ROUTE_NOTHING;
        actionState.pending.feedback = ACTION_FEEDBACK_NOTHING;
        actionState.pending.weapon = weapon;
        actionState.pending.active = 1U;
        ++actionState.noUses;
        ++actionState.selects;
        printf("[ACTIONENGINE] ROUTE seq=%u weapon=%u target=none distance=0 route=NOTHING_TO_USE feedback=screen turnAdvance=deferred\n",
               (unsigned int)intent->sequence,
               (unsigned int)weapon);
        return status;
    }

    /* DoomCanvas traces eight tiles, but the legacy extinguisher has rangeMin=0.
     * CombatEntity_calcHit therefore rejects any target whose squared world
     * distance exceeds 4096: in this cardinal trace only distance==1 can hit.
     * Until generic miss/turn combat is owned, recognize farther fire and fail
     * closed instead of removing it at range. */
    if (target.type == ACTION_ENTITY_FIRE &&
        weapon == ACTION_WEAPON_EXTINGUISHER && target.distance > 1U) {
        ++actionState.selects;
        ++actionState.combatDeferred;
        printf("[ACTIONENGINE] TRACE seq=%u weapon=%u distance=%u tile=%u target=sprite index=%u line=%u type=%u subtype=%u route=FIRE_RANGE_DEFERRED\n",
               (unsigned int)intent->sequence,
               (unsigned int)weapon,
               (unsigned int)target.distance,
               (unsigned int)target.tileIndex,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.lineIndex,
               (unsigned int)target.type,
               (unsigned int)target.subtype);
        printf("[ACTIONENGINE] BACKEND-DEFER seq=%u sprite=%u family=fire-combat reason=legacy-range-miss+turn-not-owned distance=%u mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.distance);
        return status;
    }

    route = routeTarget(&target, weapon);
    printf("[ACTIONENGINE] TRACE seq=%u weapon=%u distance=%u tile=%u target=%s index=%u line=%u type=%u subtype=%u route=%s\n",
           (unsigned int)intent->sequence,
           (unsigned int)weapon,
           (unsigned int)target.distance,
           (unsigned int)target.tileIndex,
           target.isLine != 0U ? "line" : "sprite",
           (unsigned int)target.spriteIndex,
           (unsigned int)target.lineIndex,
           (unsigned int)target.type,
           (unsigned int)target.subtype,
           routeName(route));

    ++actionState.selects;
    if (route == ACTION_ROUTE_FIRE_CLEARED &&
        target.spriteIndex != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE &&
        !removed(target.spriteIndex)) {
        if (!EspNativeGameplayPlayerState_ensure() ||
            EspNativeGameplayPlayerState_ammo(ACTION_EXTINGUISHER_AMMO_TYPE) <
                ACTION_EXTINGUISHER_AMMO_USAGE) {
            printf("[ACTIONENGINE] NOAMMO seq=%u sprite=%u weapon=%u ammoType=%u need=%u have=%u mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon,
                   (unsigned int)ACTION_EXTINGUISHER_AMMO_TYPE,
                   (unsigned int)ACTION_EXTINGUISHER_AMMO_USAGE,
                   (unsigned int)EspNativeGameplayPlayerState_ammo(
                       ACTION_EXTINGUISHER_AMMO_TYPE));
            return status;
        }
        setRemoved(target.spriteIndex, 1);
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = target.spriteIndex;
        actionState.pending.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = (uint8_t)route;
        actionState.pending.feedback = ACTION_FEEDBACK_FIRE_CLEARED;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        actionState.pending.worldChanged = 1U;
        actionState.pending.active = 1U;
        ++actionState.fireClears;
        printf("[ACTIONENGINE] ARM seq=%u sprite=%u effect=fire-remove overlayBytes=%u xp=2-deferred ammoUsage=1-pending sound=5045-deferred attackFrame=pending redraw=pending rollback=armed\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)ACTION_REMOVED_BYTES);
    }
    else if (route == ACTION_ROUTE_JAMMED_DOOR_CLEARED) {
        EspNativeGameplayDestructibleResult preflight;
        EspNativeGameplayDestructibleStatus destructibleStatus;
        memset(&preflight, 0, sizeof(preflight));
        destructibleStatus = EspNativeGameplayDestructible_preflightLineDeath(
            target.tileIndex, target.lineIndex, &preflight);
        if (destructibleStatus != ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK) {
            ++actionState.destructibleDeferred;
            printf("[ACTIONENGINE] BACKEND-DEFER seq=%u line=%u family=jammed-door reason=death-event-preflight-%s mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.lineIndex,
                   EspNativeGameplayDestructible_statusName(destructibleStatus));
            return status;
        }
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        memset(&actionState.destructibleUndo, 0,
               sizeof(actionState.destructibleUndo));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.lineIndex = target.lineIndex;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = (uint8_t)route;
        actionState.pending.feedback = ACTION_FEEDBACK_DOOR_CLEARED;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        actionState.pending.worldChanged = 1U;
        actionState.pending.active = 1U;
        printf("[DESTRUCTIBLE] ARM seq=%u tile=%u line=%u event=%u global=%u subtype=%u weapon=%u distance=%u runFlags=%08x hitCalc=%u rng=pending mutation=no rollback=armed\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.tileIndex,
               (unsigned int)target.lineIndex,
               (unsigned int)preflight.eventIndex,
               (unsigned int)preflight.globalCommandIndex,
               (unsigned int)target.subtype,
               (unsigned int)weapon,
               (unsigned int)target.distance,
               (unsigned int)ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_DEATH_RUN_FLAGS,
               (unsigned int)ACTION_JAMMED_DOOR_CALC_HIT);
    }
    else if (route == ACTION_ROUTE_NOTHING || route == ACTION_ROUTE_HUMAN) {
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = target.spriteIndex;
        actionState.pending.lineIndex = target.lineIndex;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = (uint8_t)route;
        actionState.pending.feedback = ACTION_FEEDBACK_NOTHING;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        actionState.pending.active = 1U;
        ++actionState.noUses;
    }
    else if (route == ACTION_ROUTE_ENEMY_DEFERRED) {
        ++actionState.combatDeferred;
        printf("[ACTIONENGINE] BACKEND-DEFER seq=%u sprite=%u family=monster-combat reason=native-monster-hp+attack-state-not-owned mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex);
    }
    else if (route == ACTION_ROUTE_DESTRUCTIBLE_DEFERRED) {
        ++actionState.destructibleDeferred;
        printf("[ACTIONENGINE] BACKEND-DEFER seq=%u sprite=%u line=%u family=destructible-combat reason=generic-hit+hp/subtype-consequence-not-owned mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.lineIndex);
    }
    return status;
}

void EspNativeGameplayActionEngine_reset(void) {
    EspNativeGameplayWeapon_cancelAttack();
    memset(&actionState, 0, sizeof(actionState));
}

static void logActionFrame(const ActionPending* pending,
                           const char* phase,
                           const EspNativeGameplayFrameStats* frame) {
    if (pending == NULL || phase == NULL || frame == NULL) return;
    printf("[ACTIONENGINE] FRAME seq=%u route=%s phase=%s frame=%08x worldUs=%u spriteUs=%u hudUs=%u presentUs=%u totalUs=%u sprites=%u pixels=%u spriteReads=%u hudReads=%u presented=%u\n",
           (unsigned int)pending->sequence,
           routeName((ActionRoute)pending->route),
           phase,
           (unsigned int)frame->frameAfterFNV,
           (unsigned int)frame->worldMicros,
           (unsigned int)frame->spriteMicros,
           (unsigned int)frame->hudMicros,
           (unsigned int)frame->presentMicros,
           (unsigned int)frame->totalMicros,
           (unsigned int)frame->spriteDraws,
           (unsigned int)frame->spritePixels,
           (unsigned int)frame->spritePackReads,
           (unsigned int)frame->hudPackReads,
           (unsigned int)frame->finalPresented);
}

int EspNativeGameplayActionEngine_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view = EspPlayerView_view();
    ActionPending pending;

    if (!serviceFeedbackExpiry()) return 0;
    /* PASS TURN and future non-SELECT actions may queue transient feedback
     * without owning an ActionPending transaction. If no gameplay redraw has
     * consumed it yet, present the existing framebuffer with the queued topbar. */
    if (actionState.pending.active == 0U && actionState.feedbackPending != 0U) {
        if (!__wrap_Esp32PlatformVideo_present()) return 0;
    }
    if (actionState.pending.active == 0U) return 1;
    if (doomRpg == NULL || doomRpg->render == NULL || view == NULL ||
        view->active != 1U || !ensureOwner()) {
        return 0;
    }
    pending = actionState.pending;

    if (pending.worldChanged == 0U) {
        actionState.feedbackPending = 1U;
        actionState.feedbackKind = pending.feedback;
        if (!__wrap_Esp32PlatformVideo_present()) return 0;
        printf("[ACTIONENGINE] PRESENT seq=%u route=%s worldMutation=no fullRedraw=no feedback=yes\n",
               (unsigned int)pending.sequence,
               routeName((ActionRoute)pending.route));
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        return 1;
    }

    {
        EspNativeGameplayFrameStats frame;
        EspNativeGameplayPlayerState playerBefore;
        int isFire = pending.route == ACTION_ROUTE_FIRE_CLEARED;
        int isJammedDoor = pending.route == ACTION_ROUTE_JAMMED_DOOR_CLEARED;
        int animateWeapon = isFire || isJammedDoor;
        Random_t randomBefore;
        uint32_t playerFNVBefore = 0U;
        uint32_t playerFNVAfter = 0U;
        uint8_t ammoBefore = 0U;
        uint8_t ammoAfter = 0U;
        uint8_t playerCaptured = 0U;
        uint8_t randomCaptured = 0U;
        uint8_t randHit = 0U;
        uint8_t destructibleMutated = 0U;

        memset(&frame, 0, sizeof(frame));
        memset(&playerBefore, 0, sizeof(playerBefore));
        memset(&randomBefore, 0, sizeof(randomBefore));
        if (animateWeapon &&
            !EspNativeGameplayWeapon_armAttack(pending.weapon)) {
            if (isFire) setRemoved(pending.spriteIndex, 0);
            memset(&actionState.pending, 0, sizeof(actionState.pending));
            printf("[ACTIONENGINE] FAILED seq=%u reason=weapon-attack-arm weapon=%u rollback=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.weapon);
            return 0;
        }

        if (isFire) {
            if (!EspNativeGameplayPlayerState_ensure() ||
                !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
                setRemoved(pending.spriteIndex, 0);
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[ACTIONENGINE] FAILED seq=%u sprite=%u reason=fire-player-snapshot rollback=world+weapon mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }
            playerCaptured = 1U;
            playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
            if (!EspNativeGameplayPlayerState_consumeAmmo(
                    ACTION_EXTINGUISHER_AMMO_TYPE,
                    ACTION_EXTINGUISHER_AMMO_USAGE,
                    &ammoBefore,
                    &ammoAfter)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                setRemoved(pending.spriteIndex, 0);
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[ACTIONENGINE] NOAMMO seq=%u sprite=%u weapon=%u ammoType=%u need=%u have=%u playerRollback=yes worldRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)pending.weapon,
                       (unsigned int)ACTION_EXTINGUISHER_AMMO_TYPE,
                       (unsigned int)ACTION_EXTINGUISHER_AMMO_USAGE,
                       (unsigned int)ammoBefore);
                return 1;
            }
            playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
        }

        if (isJammedDoor) {
            EspNativeGameplayDestructibleStatus destructibleStatus;
            randomBefore = doomRpg->random;
            randomCaptured = 1U;
            randHit = DoomRPG_randNextByte(&doomRpg->random);
            if ((uint32_t)randHit >= ACTION_JAMMED_DOOR_CALC_HIT) {
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[DESTRUCTIBLE] FAILED seq=%u reason=impossible-base-hit rand=%u calc=%u mutation=no rngRollback=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)randHit,
                       (unsigned int)ACTION_JAMMED_DOOR_CALC_HIT);
                return 0;
            }
            memset(&actionState.destructibleUndo, 0,
                   sizeof(actionState.destructibleUndo));
            destructibleStatus = EspNativeGameplayDestructible_executeLineDeath(
                pending.tileIndex, pending.lineIndex,
                &actionState.destructibleUndo);
            if (destructibleStatus != ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK) {
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[DESTRUCTIBLE] FAILED seq=%u line=%u reason=death-event-%s rand=%u rngRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.lineIndex,
                       EspNativeGameplayDestructible_statusName(destructibleStatus),
                       (unsigned int)randHit);
                return 0;
            }
            destructibleMutated = 1U;
            printf("[DESTRUCTIBLE] HIT seq=%u line=%u event=%u global=%u subtype=%u weapon=%u distance=%u rand=%u calc=%u guaranteed=yes open=%u->%u rngConsumed=1 xp=1-pending\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)actionState.destructibleUndo.lineIndex,
                   (unsigned int)actionState.destructibleUndo.eventIndex,
                   (unsigned int)actionState.destructibleUndo.globalCommandIndex,
                   (unsigned int)pending.subtype,
                   (unsigned int)pending.weapon,
                   (unsigned int)pending.distance,
                   (unsigned int)randHit,
                   (unsigned int)ACTION_JAMMED_DOOR_CALC_HIT,
                   (unsigned int)actionState.destructibleUndo.openBefore,
                   (unsigned int)actionState.destructibleUndo.openAfter);
        }

        actionState.feedbackPending = 1U;
        actionState.feedbackKind = pending.feedback;
        if (!EspNativeGameplayFrame_renderTurn(
                doomRpg->render, (uint8_t)view->viewAngle, &frame)) {
            int rollbackOk = 1;
            EspNativeGameplayWeapon_cancelAttack();
            actionState.feedbackPending = 0U;
            actionState.feedbackKind = ACTION_FEEDBACK_NONE;
            if (isFire) {
                setRemoved(pending.spriteIndex, 0);
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
            }
            else if (isJammedDoor && destructibleMutated != 0U) {
                rollbackOk = EspNativeGameplayDestructible_rollbackLineDeath(
                    &actionState.destructibleUndo);
                if (randomCaptured != 0U) doomRpg->random = randomBefore;
                memset(&actionState.destructibleUndo, 0,
                       sizeof(actionState.destructibleUndo));
            }
            memset(&frame, 0, sizeof(frame));
            if (!rollbackOk || !EspNativeGameplayFrame_renderTurn(
                    doomRpg->render, (uint8_t)view->viewAngle, &frame)) {
                printf("[ACTIONENGINE] FAILED seq=%u reason=render+rollback-render sprite=%u line=%u rollback=%s\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)pending.lineIndex,
                       rollbackOk ? "yes" : "NO");
                return 0;
            }
            printf("[ACTIONENGINE] ROLLBACK seq=%u sprite=%u line=%u route=%s restored=yes rng=%s player=%s frame=%08x\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)pending.lineIndex,
                   routeName((ActionRoute)pending.route),
                   isJammedDoor ? "restored" : "unchanged",
                   isFire ? "restored" : "unchanged",
                   (unsigned int)frame.frameAfterFNV);
            memset(&actionState.pending, 0, sizeof(actionState.pending));
            return 1;
        }

        logActionFrame(&pending, animateWeapon ? "attack" : "commit", &frame);

        if (isFire) {
            printf("[ACTIONENGINE] FIRE-COMMIT seq=%u sprite=%u ammoType=%u ammo=%u->%u playerFNV=%08x->%08x xp=2-deferred sound=5045-deferred turnAdvance=deferred rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)ACTION_EXTINGUISHER_AMMO_TYPE,
                   (unsigned int)ammoBefore,
                   (unsigned int)ammoAfter,
                   (unsigned int)playerFNVBefore,
                   (unsigned int)playerFNVAfter);
        }
        if (isJammedDoor) {
            ++actionState.jammedDoorClears;
            ++actionState.deferredXp;
            printf("[DESTRUCTIBLE] COMMIT seq=%u line=%u event=%u open=0->1 message=\"Door cleared!\" xp=1-deferred xpDeferredTotal=%u sound=5044-deferred turnAdvance=deferred rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.lineIndex,
                   (unsigned int)actionState.destructibleUndo.eventIndex,
                   (unsigned int)actionState.deferredXp);
            memset(&actionState.destructibleUndo, 0,
                   sizeof(actionState.destructibleUndo));
        }

        if (animateWeapon) {
            EspNativeGameplayFrameStats settle;
            memset(&settle, 0, sizeof(settle));
            actionState.feedbackPending = 1U;
            actionState.feedbackKind = pending.feedback;
            if (EspNativeGameplayFrame_renderTurn(
                    doomRpg->render, (uint8_t)view->viewAngle, &settle)) {
                logActionFrame(&pending, "settle-idle", &settle);
                printf("[ACTIONENGINE] ATTACK seq=%u weapon=%u frame=1->0 generic=yes worldCommitted=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.weapon);
            }
            else {
                actionState.feedbackPending = 0U;
                actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                EspNativeGameplayWeapon_cancelAttack();
                printf("[ACTIONENGINE] SETTLE-FAILED seq=%u weapon=%u worldCommitted=yes recovery=next-full-redraw\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.weapon);
            }
        }
    }

    memset(&actionState.pending, 0, sizeof(actionState.pending));
    return 1;
}
