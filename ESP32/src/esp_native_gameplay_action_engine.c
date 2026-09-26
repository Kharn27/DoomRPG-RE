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
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_native_gameplay_destructible.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_facing_label.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_status_message.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_transition_presentation.h"
#include "esp_native_sprite_renderer.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define ACTION_TRACE_MASK 0x5687U
#define ACTION_TRACE_TILES 8U
#define ACTION_REMOVED_BYTES ESP_NATIVE_GAMEPLAY_ACTION_REMOVED_SNAPSHOT_MAX_BYTES
#define ACTION_MAX_SPRITES (ACTION_REMOVED_BYTES * 8U)
#define ACTION_VISUAL_HIDDEN 0x80U

#define ACTION_ENTITY_ENEMY 1U
#define ACTION_ENTITY_HUMAN 2U
#define ACTION_ENTITY_FIRE 10U
#define ACTION_ENTITY_DESTRUCTIBLE 12U
#define ACTION_DESTRUCTIBLE_BARREL_SUBTYPE 1U
#define ACTION_DESTRUCTIBLE_CRATE_SUBTYPE 2U
#define ACTION_DESTRUCTIBLE_JAMMED_SUBTYPE 3U
#define ACTION_WEAPON_AXE 0U
#define ACTION_WEAPON_EXTINGUISHER 1U
#define ACTION_EXTINGUISHER_AMMO_TYPE 0U
#define ACTION_EXTINGUISHER_AMMO_USAGE 1U
#define ACTION_SPRITE_DEF_MASK 511U
#define ACTION_SPRITE_DEF_TILE_FLAG 0x00040000UL
#define ACTION_SPRITE_DEF_TILE_BASE 305U
#define ACTION_TRAP_EXPLOSION_LOGICAL 180U
#define ACTION_TRAP_EXPLOSION_FRAMES 3U
#define ACTION_TRAP_DAMAGE_FLASH_MS 500U
#define ACTION_BARREL_CHAIN_MAX 16U
#define ACTION_BARREL_RADIUS_NEIGHBORS 8U

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
#define FEEDBACK_DYNAMIC_TEXT_BYTES 24U
#define FEEDBACK_DAMAGE_RED565 0xb800U
#define FEEDBACK_VIEW_Y FEEDBACK_TOP_HEIGHT
#define FEEDBACK_VIEW_HEIGHT (DOOMRPG_LOGICAL_HEIGHT - (FEEDBACK_TOP_HEIGHT * 2U))
#define FEEDBACK_BORDER_THICKNESS 2U
#define FEEDBACK_BORDER_PIXELS \
    ((DOOMRPG_LOGICAL_WIDTH * FEEDBACK_BORDER_THICKNESS * 2U) + \
     ((FEEDBACK_VIEW_HEIGHT - (FEEDBACK_BORDER_THICKNESS * 2U)) * \
      FEEDBACK_BORDER_THICKNESS * 2U))

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native action feedback requires the 160x120 logical framebuffer"
#endif

typedef EspNativeGameplayActionFeedback ActionFeedback;
#define ACTION_FEEDBACK_NONE ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE
#define ACTION_FEEDBACK_NOTHING ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NOTHING
#define ACTION_FEEDBACK_FIRE_CLEARED ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_FIRE_CLEARED
#define ACTION_FEEDBACK_DOOR_CLEARED ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DOOR_CLEARED
#define ACTION_FEEDBACK_PASS_TURN ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN
#define ACTION_FEEDBACK_PICKUP ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP
#define ACTION_FEEDBACK_DAMAGE ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE
#define ACTION_FEEDBACK_PLAYER_HIT ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT
#define ACTION_FEEDBACK_NO_EFFECT ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NO_EFFECT
#define ACTION_FEEDBACK_TRAPPED ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_TRAPPED
#define ACTION_FEEDBACK_NO_AMMO ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NO_AMMO
#define ACTION_FEEDBACK_COMBAT_TEXT ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_COMBAT_TEXT

typedef enum ActionRoute_e {
    ACTION_ROUTE_INVALID = 0,
    ACTION_ROUTE_NOTHING = 1,
    ACTION_ROUTE_FIRE_CLEARED = 2,
    ACTION_ROUTE_HUMAN = 3,
    ACTION_ROUTE_ENEMY_DEFERRED = 4,
    ACTION_ROUTE_JAMMED_DOOR_CLEARED = 5,
    ACTION_ROUTE_DESTRUCTIBLE_DEFERRED = 6,
    ACTION_ROUTE_CRATE_SUBTYPE2 = 7,
    ACTION_ROUTE_BARREL_SUBTYPE1 = 8
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
    uint32_t viewportFlashShownAtMs;
    uint16_t viewportFlashDurationMs;
    uint16_t viewportFlashColor565;
    uint8_t viewportFlashPending;
    uint8_t viewportFlashVisible;
    uint8_t viewportFlashSnapshotValid;
    uint8_t framebufferFresh;
    char feedbackText[FEEDBACK_DYNAMIC_TEXT_BYTES];
} ActionEngineState;

typedef struct FeedbackScratch_s {
    EspNativeIndexedBmp bar;
    EspNativeIndexedBmp font;
} FeedbackScratch;

static ActionEngineState actionState;
static uint16_t viewportBorderSnapshot[FEEDBACK_BORDER_PIXELS];

EspNativeGameplayActionStatus __real_EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);
int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);
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

/*
 * DoomRPG-RE's inherited DoomRPG_randNextInt() advances nextRand as a byte
 * offset, then incorrectly applies that byte offset as an int* array index.
 * Once nextRand reaches 32 this can read beyond the 128-byte random table and
 * into unrelated DoomRPG_t fields, making crate blast damage architecture-
 * dependent.  The original BREW expression only needs the low byte of the
 * next 32-bit RNG word.  On the little-endian target that byte is exactly
 * randTable[nextRand], while the word still consumes four table bytes.
 *
 * Keep this correction local to the already-audited explosion families
 * (crate trap and barrel anim #1) for now: other inherited randNextInt() call
 * sites need their own bounded audit before changing global gameplay RNG
 * sequencing. If the legacy word would refill the table (nextRand + 4 >=
 * RANDTABLESIZE), fail closed rather than calling setRand() during a preview
 * transaction whose hidden refill state cannot be rolled back.
 */
static int explosionPeekWordLowByte(const Random_t* random,
                                    uint8_t* outByte) {
    if (random == NULL || outByte == NULL || random->nextRand < 0 ||
        random->nextRand + (int)sizeof(int) >= RANDTABLESIZE) {
        return 0;
    }
    *outByte = random->randTable[random->nextRand];
    return 1;
}

static int explosionConsumeWordLowByte(Random_t* random,
                                       uint8_t* outByte) {
    if (!explosionPeekWordLowByte(random, outByte)) return 0;
    random->nextRand += (int)sizeof(int);
    return 1;
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

static uint32_t removedCountBits(const uint8_t* bits, uint16_t bytes) {
    uint32_t count = 0U;
    uint16_t i;
    if (bits == NULL) return 0U;
    for (i = 0U; i < bytes; ++i) {
        uint8_t value = bits[i];
        while (value != 0U) {
            count += (uint32_t)(value & 1U);
            value >>= 1U;
        }
    }
    return count;
}

static uint32_t removedFNV(const uint8_t* bits, uint16_t bytes) {
    uint32_t hash = 2166136261U;
    uint16_t i;
    if (bits == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= bits[i];
        hash *= 16777619U;
    }
    return hash;
}

static int removedSnapshotShapeValid(
    const EspNativeGameplayActionRemovedSnapshot* snapshot,
    uint32_t expectedArenaFNV,
    uint8_t expectedMapId,
    uint16_t expectedSpriteCount) {
    uint16_t expectedBytes;
    uint16_t i;
    uint32_t validTailBits;
    uint8_t validTailMask;

    if (snapshot == NULL || snapshot->reserved0 != 0U ||
        snapshot->sourceArenaFNV1a != expectedArenaFNV ||
        snapshot->targetMapId != expectedMapId ||
        snapshot->spriteCount != expectedSpriteCount ||
        snapshot->spriteCount == 0U ||
        snapshot->spriteCount > ACTION_MAX_SPRITES ||
        snapshot->removedCount > snapshot->spriteCount) {
        return 0;
    }

    expectedBytes = (uint16_t)((snapshot->spriteCount + 7U) >> 3U);
    if (expectedBytes == 0U || expectedBytes > ACTION_REMOVED_BYTES ||
        snapshot->removedBytes != expectedBytes ||
        removedCountBits(snapshot->removedBits, expectedBytes) !=
            snapshot->removedCount ||
        snapshot->stateFNV1a != removedFNV(snapshot->removedBits,
                                           expectedBytes)) {
        return 0;
    }

    validTailBits = snapshot->spriteCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((snapshot->removedBits[expectedBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return 0;
        }
    }
    for (i = expectedBytes; i < ACTION_REMOVED_BYTES; ++i) {
        if (snapshot->removedBits[i] != 0U) return 0;
    }
    return 1;
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
    if (!EspNativeGameplayCrateState_applyEntity(
            spriteIndex, outType, outSubType, outLinkState)) {
        return 0;
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

    printf("[ACTIONENGINE] READY map=%u arena=%08x sprites=%u ownerBytes=%u traceMask=%04x traceTiles=%u fires=%u humans=%u enemies=%u destructibles=%u eventFirst=yes feedbackMs=%u ammo=playerState monsterCombat=deferred crate2=parm+generic-combat+loot-owned/radial-deferred jammedDoor3=axe-adjacent-owned otherDestructibles=deferred\n",
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

int EspNativeGameplayActionEngine_snapshotRemoved(
    EspNativeGameplayActionRemovedSnapshot* outSnapshot) {
    uint16_t usedBytes;
    if (outSnapshot == NULL || !ensureOwner()) return 0;
    usedBytes = (uint16_t)((actionState.spriteCount + 7U) >> 3U);
    if (usedBytes == 0U || usedBytes > ACTION_REMOVED_BYTES) return 0;

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->sourceArenaFNV1a = actionState.arenaFNV;
    outSnapshot->spriteCount = actionState.spriteCount;
    outSnapshot->removedBytes = usedBytes;
    outSnapshot->targetMapId = actionState.targetMapId;
    memcpy(outSnapshot->removedBits, actionState.removedBits, usedBytes);
    outSnapshot->removedCount =
        (uint16_t)removedCountBits(outSnapshot->removedBits, usedBytes);
    outSnapshot->stateFNV1a =
        removedFNV(outSnapshot->removedBits, usedBytes);
    return removedSnapshotShapeValid(outSnapshot,
                                     actionState.arenaFNV,
                                     actionState.targetMapId,
                                     actionState.spriteCount);
}

int EspNativeGameplayActionEngine_restoreRemoved(
    const EspNativeGameplayActionRemovedSnapshot* snapshot) {
    if (!ensureOwner() ||
        !removedSnapshotShapeValid(snapshot,
                                   actionState.arenaFNV,
                                   actionState.targetMapId,
                                   actionState.spriteCount)) {
        return 0;
    }
    memset(actionState.removedBits, 0, sizeof(actionState.removedBits));
    memcpy(actionState.removedBits, snapshot->removedBits,
           snapshot->removedBytes);
    return removedFNV(actionState.removedBits, snapshot->removedBytes) ==
           snapshot->stateFNV1a;
}

uint32_t EspNativeGameplayActionEngine_removedFingerprint(void) {
    uint16_t usedBytes;
    if (!ensureOwner()) return 0U;
    usedBytes = (uint16_t)((actionState.spriteCount + 7U) >> 3U);
    if (usedBytes == 0U || usedBytes > ACTION_REMOVED_BYTES) return 0U;
    return removedFNV(actionState.removedBits, usedBytes);
}

int EspNativeGameplayActionEngine_queueFeedback(
    EspNativeGameplayActionFeedback feedback) {
    if (feedback <= ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE ||
        feedback > ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT ||
        !ensureOwner() || actionState.pending.active != 0U ||
        actionState.feedbackPending != 0U) {
        return 0;
    }
    actionState.feedbackText[0] = '\0';
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = (uint8_t)feedback;
    return 1;
}

int EspNativeGameplayActionEngine_queueTextFeedback(
    EspNativeGameplayActionFeedback feedback,
    const char* text,
    uint16_t viewportFlashMs) {
    size_t len;
    if ((feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP &&
         feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE &&
         feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT &&
         feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_COMBAT_TEXT &&
         feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT) ||
        text == NULL || !ensureOwner() || actionState.pending.active != 0U ||
        actionState.feedbackPending != 0U) {
        return 0;
    }
    len = strnlen(text, FEEDBACK_DYNAMIC_TEXT_BYTES);
    if (len == 0U || len >= FEEDBACK_DYNAMIC_TEXT_BYTES) return 0;
    memcpy(actionState.feedbackText, text, len + 1U);
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = (uint8_t)feedback;
    if (viewportFlashMs != 0U) {
        actionState.viewportFlashPending = 1U;
        actionState.viewportFlashDurationMs = viewportFlashMs;
        actionState.viewportFlashColor565 =
            feedback == ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE
                ? FEEDBACK_DAMAGE_RED565 : 0xffffU;
    }
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
    actionState.feedbackText[0] = '\0';
    actionState.viewportFlashPending = 0U;
    if (actionState.viewportFlashVisible == 0U) {
        actionState.viewportFlashDurationMs = 0U;
        actionState.viewportFlashColor565 = 0U;
    }
    return 1;
}

void EspNativeGameplayActionEngine_markFreshFrame(void) {
    actionState.framebufferFresh = 1U;
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

static const int8_t barrelRadiusDx[ACTION_BARREL_RADIUS_NEIGHBORS] = {
    -1, 1, 0, 0, -1, 1, 1, -1
};
static const int8_t barrelRadiusDy[ACTION_BARREL_RADIUS_NEIGHBORS] = {
     0, 0,-1, 1,  1, 1,-1, -1
};

static int barrelChainContains(const uint16_t* chain,
                               uint8_t count,
                               uint16_t spriteIndex) {
    uint8_t i;
    if (chain == NULL) return 0;
    for (i = 0U; i < count; ++i) {
        if (chain[i] == spriteIndex) return 1;
    }
    return 0;
}

/* Game_findMapEntityXYFlag(..., 0x5687) resolves the top linked entity whose
 * type participates in the trace mask. Radius damage does not perform the
 * line-of-sight special-entity test used by SELECT tracing, so keep this lookup
 * independent from findSpriteOnTile(). */
static int findRadiusSpriteOnTile(uint16_t tile, ActionTarget* outTarget) {
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
        if (!actionGetEntity(i, &type, &subtype, &linkState, &linkOrder)) {
            return -1;
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !entityTypeInTraceMask(type)) {
            continue;
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

static int barrelRadiusCellSupported(uint16_t tile,
                                     const EspPlayerViewState* view,
                                     ActionTarget* outTarget,
                                     int* outHasBarrel,
                                     int* outIsPlayer) {
    uint16_t lineIndex;
    uint8_t lineType;
    uint8_t lineSubtype;
    int lineResult;
    int spriteResult;

    if (outTarget == NULL || outHasBarrel == NULL || outIsPlayer == NULL ||
        view == NULL) return 0;
    *outHasBarrel = 0;
    *outIsPlayer = 0;
    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->spriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outTarget->lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outTarget->tileIndex = tile;

    if (((uint16_t)(view->destY >> 6) * MAP_WIDTH +
         (uint16_t)(view->destX >> 6)) == tile) {
        /* Game_findMapEntityXYFlag() tests flags&0x100 and returns the player
         * before consulting entityDb. Radius z=0 therefore always hurts the
         * player on this tile, regardless of any line/sprite underneath. */
        outTarget->type = 0xfeU; /* player sentinel for diagnostics */
        *outIsPlayer = 1;
        return 1;
    }

    lineResult = findLinkedLineBlocker(tile, &lineIndex, &lineType,
                                       &lineSubtype);
    if (lineResult < 0) return 0;
    if (lineResult > 0) {
        outTarget->lineIndex = lineIndex;
        outTarget->isLine = 1U;
        outTarget->type = lineType;
        outTarget->subtype = lineSubtype;
        /* A line without an EntityDef but with flags&24 is linked as the
         * generic type-9 fallback. Entity_initspawn() leaves it non-hurtable,
         * so Game_hurtEntityAt() finds it first and no-ops. Native
         * findLinkedLineBlocker() represents that fallback as type=0/sub=255.
         * Jammed doors and power couplings are also explicit radius no-ops. */
        if (lineType == 0U && lineSubtype == 0xffU) return 1;
        return lineType == ACTION_ENTITY_DESTRUCTIBLE &&
               (lineSubtype == ACTION_DESTRUCTIBLE_JAMMED_SUBTYPE ||
                lineSubtype == 4U);
    }

    spriteResult = findRadiusSpriteOnTile(tile, outTarget);
    if (spriteResult < 0) return 0;
    if (spriteResult == 0) return 1;

    if (outTarget->type == ACTION_ENTITY_FIRE) return 1;
    if (outTarget->type == ACTION_ENTITY_DESTRUCTIBLE &&
        (outTarget->subtype == ACTION_DESTRUCTIBLE_JAMMED_SUBTYPE ||
         outTarget->subtype == 4U)) {
        return 1;
    }
    if (outTarget->type == ACTION_ENTITY_DESTRUCTIBLE &&
        outTarget->subtype == ACTION_DESTRUCTIBLE_BARREL_SUBTYPE) {
        *outHasBarrel = 1;
        return 1;
    }
    return 0;
}

/* Dry-run the whole barrel-connected component before consuming explosion RNG
 * or mutating world state. This milestone owns barrel->barrel propagation only.
 * Any player/monster/human/crate/other radius consequence stays fail-closed. */
static int barrelChainPreflight(uint16_t primarySprite,
                                const EspPlayerViewState* view,
                                uint16_t* chain,
                                uint8_t* outCount,
                                ActionTarget* outUnsupported) {
    uint8_t count = 1U;
    uint8_t cursor;

    if (view == NULL || chain == NULL || outCount == NULL ||
        outUnsupported == NULL ||
        primarySprite == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) {
        return 0;
    }
    chain[0] = primarySprite;
    memset(outUnsupported, 0, sizeof(*outUnsupported));

    for (cursor = 0U; cursor < count; ++cursor) {
        EspMapSprite source;
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint8_t n;

        if (!__real_EspMapRuntime_getMapSprite(chain[cursor], &source) ||
            !actionGetEntity(chain[cursor], &type, &subtype,
                             &linkState, &linkOrder) ||
            type != ACTION_ENTITY_DESTRUCTIBLE ||
            subtype != ACTION_DESTRUCTIBLE_BARREL_SUBTYPE ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U) {
            return 0;
        }
        (void)linkOrder;

        for (n = 0U; n < ACTION_BARREL_RADIUS_NEIGHBORS; ++n) {
            int32_t tileX = ((int32_t)source.x >> 6) + barrelRadiusDx[n];
            int32_t tileY = ((int32_t)source.y >> 6) + barrelRadiusDy[n];
            uint16_t tile;
            ActionTarget target;
            int hasBarrel;
            int isPlayer;

            if (tileX < 0 || tileX >= MAP_WIDTH ||
                tileY < 0 || tileY >= MAP_WIDTH) {
                continue;
            }
            tile = (uint16_t)(tileY * MAP_WIDTH + tileX);
            if (!barrelRadiusCellSupported(tile, view, &target,
                                           &hasBarrel, &isPlayer)) {
                *outUnsupported = target;
                return 0;
            }
            (void)isPlayer;
            if (!hasBarrel || removed(target.spriteIndex) ||
                barrelChainContains(chain, count, target.spriteIndex)) {
                continue;
            }
            if (count >= ACTION_BARREL_CHAIN_MAX) {
                outUnsupported->spriteIndex = target.spriteIndex;
                outUnsupported->tileIndex = tile;
                outUnsupported->type = ACTION_ENTITY_DESTRUCTIBLE;
                outUnsupported->subtype = ACTION_DESTRUCTIBLE_BARREL_SUBTYPE;
                return 0;
            }
            chain[count++] = target.spriteIndex;
        }
    }
    *outCount = count;
    return 1;
}

static void barrelRollbackRemoved(const uint16_t* chain, uint8_t count) {
    uint8_t i;
    if (chain == NULL) return;
    for (i = 0U; i < count; ++i) setRemoved(chain[i], 0);
}

static int barrelTriggerNeighbors(uint16_t sourceSprite,
                                  const EspPlayerViewState* view,
                                  uint8_t blastDamage,
                                  uint16_t* chain,
                                  uint8_t* ioCount,
                                  uint8_t* outTriggered,
                                  uint8_t* outPlayerHits,
                                  uint16_t* outPlayerMessageDamage) {
    EspMapSprite source;
    uint8_t n;
    uint8_t triggered = 0U;
    uint8_t playerHits = 0U;
    uint16_t playerMessageDamage = 0U;

    if (view == NULL || chain == NULL || ioCount == NULL ||
        outTriggered == NULL || outPlayerHits == NULL ||
        outPlayerMessageDamage == NULL || blastDamage == 0U ||
        !__real_EspMapRuntime_getMapSprite(sourceSprite, &source)) {
        return 0;
    }

    for (n = 0U; n < ACTION_BARREL_RADIUS_NEIGHBORS; ++n) {
        int32_t tileX = ((int32_t)source.x >> 6) + barrelRadiusDx[n];
        int32_t tileY = ((int32_t)source.y >> 6) + barrelRadiusDy[n];
        uint16_t tile;
        ActionTarget target;
        int hasBarrel;
        int isPlayer;

        if (tileX < 0 || tileX >= MAP_WIDTH ||
            tileY < 0 || tileY >= MAP_WIDTH) {
            continue;
        }
        tile = (uint16_t)(tileY * MAP_WIDTH + tileX);
        if (!barrelRadiusCellSupported(tile, view, &target,
                                       &hasBarrel, &isPlayer)) {
            return 0;
        }
        if (isPlayer) {
            EspNativeGameplayPlayerDamageResult damageResult;
            EspNativeGameplayPlayerDamageStatus damageStatus;
            uint8_t component = n < 4U
                                    ? blastDamage
                                    : (uint8_t)(blastDamage >> 1U);
            memset(&damageResult, 0, sizeof(damageResult));
            if (component == 0U) continue;
            damageStatus = EspNativeGameplayPlayerState_applyDamageNonlethal(
                component, component, &damageResult);
            if (damageStatus != ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_OK) {
                printf("[BARRELRADIUS] PLAYER-DEFER source=%u tile=%u relation=%s component=%u status=%u hp=%u armor=%u mutation=%s\n",
                       (unsigned int)sourceSprite,
                       (unsigned int)tile,
                       n < 4U ? "cardinal" : "diagonal",
                       (unsigned int)component,
                       (unsigned int)damageStatus,
                       (unsigned int)damageResult.healthBefore,
                       (unsigned int)damageResult.armorBefore,
                       damageStatus ==
                               ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_DEFERRED
                           ? "no/lethal-deferred" : "no/invalid");
                return 0;
            }
            ++playerHits;
            playerMessageDamage =
                (uint16_t)((uint16_t)component * 2U);
            printf("[BARRELRADIUS] PLAYER-HIT source=%u tile=%u relation=%s component=%u messageDamage=%u hp=%u->%u armor=%u->%u playerFNV=%08x->%08x mutation=yes\n",
                   (unsigned int)sourceSprite,
                   (unsigned int)tile,
                   n < 4U ? "cardinal" : "diagonal",
                   (unsigned int)component,
                   (unsigned int)playerMessageDamage,
                   (unsigned int)damageResult.healthBefore,
                   (unsigned int)damageResult.healthAfter,
                   (unsigned int)damageResult.armorBefore,
                   (unsigned int)damageResult.armorAfter,
                   (unsigned int)damageResult.stateFNVBefore,
                   (unsigned int)damageResult.stateFNVAfter);
            continue;
        }
        if (!hasBarrel || removed(target.spriteIndex)) continue;
        if (barrelChainContains(chain, *ioCount, target.spriteIndex)) {
            continue;
        }
        if (*ioCount >= ACTION_BARREL_CHAIN_MAX) return 0;
        setRemoved(target.spriteIndex, 1);
        chain[(*ioCount)++] = target.spriteIndex;
        ++triggered;
        printf("[BARRELRADIUS] CHAIN source=%u target=%u tile=%u ordinal=%u relation=%s mutation=removed\n",
               (unsigned int)sourceSprite,
               (unsigned int)target.spriteIndex,
               (unsigned int)tile,
               (unsigned int)(*ioCount),
               n < 4U ? "cardinal" : "diagonal");
    }
    *outTriggered = triggered;
    *outPlayerHits = playerHits;
    *outPlayerMessageDamage = playerMessageDamage;
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

static int spriteDefinition(uint16_t spriteIndex,
                            uint16_t* outDefTile,
                            uint8_t* outType,
                            uint8_t* outSubtype,
                            int32_t* outParm) {
    EspMapSprite sprite;
    uint32_t lookup;
    if (!__real_EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;
    lookup = sprite.info & ACTION_SPRITE_DEF_MASK;
    if ((sprite.info & ACTION_SPRITE_DEF_TILE_FLAG) != 0U) {
        lookup += ACTION_SPRITE_DEF_TILE_BASE;
    }
    if (lookup >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT ||
        !EspEntityDefTypeCatalog_getMetadata((uint16_t)lookup,
                                             outType, outSubtype, outParm)) {
        return 0;
    }
    if (outDefTile != NULL) *outDefTile = (uint16_t)lookup;
    return 1;
}

static int crateWeaponEligible(const ActionTarget* target,
                               uint8_t weapon,
                               uint16_t* outDefTile,
                               int32_t* outParm) {
    uint8_t type;
    uint8_t subtype;
    int32_t parm;
    uint16_t defTile;
    if (target == NULL || target->isLine != 0U ||
        target->spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
        weapon >= 32U ||
        !spriteDefinition(target->spriteIndex, &defTile,
                          &type, &subtype, &parm) ||
        type != ACTION_ENTITY_DESTRUCTIBLE ||
        subtype != ACTION_DESTRUCTIBLE_CRATE_SUBTYPE) {
        return -1;
    }
    if (outDefTile != NULL) *outDefTile = defTile;
    if (outParm != NULL) *outParm = parm;
    return (((uint32_t)parm & (1UL << weapon)) != 0U) ? 1 : 0;
}

static int targetWorldDistance(const ActionTarget* target,
                               const EspPlayerViewState* view,
                               uint32_t* outWorldDistance) {
    EspMapSprite sprite;
    int64_t dx;
    int64_t dy;
    int64_t distance;
    if (target == NULL || view == NULL || outWorldDistance == NULL ||
        target->isLine != 0U ||
        target->spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
        !__real_EspMapRuntime_getMapSprite(target->spriteIndex, &sprite)) {
        return 0;
    }
    dx = (int64_t)(int32_t)sprite.x - (int64_t)view->viewX;
    dy = (int64_t)(int32_t)sprite.y - (int64_t)view->viewY;
    distance = dx * dx + dy * dy;
    if (distance < 0 || distance > 0xffffffffLL) return 0;
    *outWorldDistance = (uint32_t)distance;
    return 1;
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
        if (target->isLine == 0U &&
            target->subtype == ACTION_DESTRUCTIBLE_BARREL_SUBTYPE) {
            return ACTION_ROUTE_BARREL_SUBTYPE1;
        }
        if (target->isLine == 0U &&
            target->subtype == ACTION_DESTRUCTIBLE_CRATE_SUBTYPE) {
            return ACTION_ROUTE_CRATE_SUBTYPE2;
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
    case ACTION_ROUTE_CRATE_SUBTYPE2:
        return "CRATE_SUBTYPE2";
    case ACTION_ROUTE_BARREL_SUBTYPE1:
        return "BARREL_SUBTYPE1";
    default: return "INVALID";
    }
}

static const char* feedbackText(uint8_t feedback) {
    if (feedback == ACTION_FEEDBACK_NOTHING) return "Nothing to use";
    if (feedback == ACTION_FEEDBACK_FIRE_CLEARED) return "Fire cleared!";
    if (feedback == ACTION_FEEDBACK_DOOR_CLEARED) return "Door cleared!";
    if (feedback == ACTION_FEEDBACK_PASS_TURN) return "Turn passed.";
    if (feedback == ACTION_FEEDBACK_NO_EFFECT) return "No effect!";
    if (feedback == ACTION_FEEDBACK_TRAPPED) return "Trapped!";
    if (feedback == ACTION_FEEDBACK_NO_AMMO) return "Not enough ammo!";
    if ((feedback == ACTION_FEEDBACK_PICKUP ||
         feedback == ACTION_FEEDBACK_DAMAGE ||
         feedback == ACTION_FEEDBACK_PLAYER_HIT ||
         feedback == ACTION_FEEDBACK_COMBAT_TEXT ||
         feedback == ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT) &&
        actionState.feedbackText[0] != '\0') {
        return actionState.feedbackText;
    }
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

static int visitViewportBorder(uint16_t* framebuffer,
                               int mode,
                               uint16_t flashColor) {
    uint32_t pos = 0U;
    uint32_t x;
    uint32_t y;
    uint32_t top = FEEDBACK_VIEW_Y;
    uint32_t bottom = FEEDBACK_VIEW_Y + FEEDBACK_VIEW_HEIGHT;
    uint32_t leftEnd = FEEDBACK_BORDER_THICKNESS;
    uint32_t rightBegin = DOOMRPG_LOGICAL_WIDTH - FEEDBACK_BORDER_THICKNESS;

    if (framebuffer == NULL || mode < 0 || mode > 2) return 0;
#define VISIT_BORDER_PIXEL(index_) do { \
        if (mode == 1) framebuffer[(index_)] = viewportBorderSnapshot[pos]; \
        else { \
            if (mode == 0) viewportBorderSnapshot[pos] = framebuffer[(index_)]; \
            framebuffer[(index_)] = flashColor; \
        } \
        ++pos; \
    } while (0)
    for (y = top; y < top + FEEDBACK_BORDER_THICKNESS; ++y) {
        for (x = 0U; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
            uint32_t index = y * DOOMRPG_LOGICAL_WIDTH + x;
            VISIT_BORDER_PIXEL(index);
        }
    }
    for (y = bottom - FEEDBACK_BORDER_THICKNESS; y < bottom; ++y) {
        for (x = 0U; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
            uint32_t index = y * DOOMRPG_LOGICAL_WIDTH + x;
            VISIT_BORDER_PIXEL(index);
        }
    }
    for (y = top + FEEDBACK_BORDER_THICKNESS;
         y < bottom - FEEDBACK_BORDER_THICKNESS; ++y) {
        for (x = 0U; x < leftEnd; ++x) {
            uint32_t index = y * DOOMRPG_LOGICAL_WIDTH + x;
            VISIT_BORDER_PIXEL(index);
        }
        for (x = rightBegin; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
            uint32_t index = y * DOOMRPG_LOGICAL_WIDTH + x;
            VISIT_BORDER_PIXEL(index);
        }
    }
#undef VISIT_BORDER_PIXEL
    return pos == FEEDBACK_BORDER_PIXELS;
}

static int paintViewportFlash(void) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
            DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    uint16_t color = actionState.viewportFlashColor565 != 0U
                         ? actionState.viewportFlashColor565 : 0xffffU;
    int preserveSnapshot;
    if (framebuffer == NULL || Esp32PlatformVideo_framebufferSizeBytes() != expected) {
        return 0;
    }
    /* A second damage/pickup flash may arrive before the first 500 ms lease
     * expires. In that case the framebuffer border is already painted. Never
     * snapshot those painted pixels as the restore target: keep the original
     * pre-flash snapshot and only repaint/reset the lease. A real full-frame
     * redraw marks framebufferFresh, so the snapshot must then be refreshed
     * from the newly rendered clean border instead. */
    preserveSnapshot = actionState.viewportFlashVisible != 0U &&
                       actionState.viewportFlashSnapshotValid != 0U &&
                       actionState.framebufferFresh == 0U;
    if (!visitViewportBorder(framebuffer, preserveSnapshot ? 2 : 0, color)) {
        return 0;
    }
    if (!preserveSnapshot) actionState.viewportFlashSnapshotValid = 1U;
    else {
        printf("[VIEWFLASH] REFRESH color565=%04x snapshot=preserved framebufferFresh=0\n",
               (unsigned int)color);
    }
    return 1;
}

static int restoreViewportFlash(void) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
            DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (actionState.viewportFlashSnapshotValid == 0U) return 1;
    if (framebuffer == NULL || Esp32PlatformVideo_framebufferSizeBytes() != expected ||
        !visitViewportBorder(framebuffer, 1, 0U)) {
        return 0;
    }
    actionState.viewportFlashSnapshotValid = 0U;
    return 1;
}

static int paintFeedback(uint8_t feedback) {
    FeedbackScratch scratch;
    EspNativeIndexedBmpStats stats;
    const EspNativeGameplayFacingLabelView* facingLabel = NULL;
    const EspMapStatusMessageState* statusMessage = NULL;
    const char* text = feedbackText(feedback);
    uint16_t* framebuffer;
    size_t framebufferBytes;
    size_t visible = 0U;
    size_t i;
    int x = FEEDBACK_TEXT_X;
    int ok = 0;

    /*
     * Legacy Hud_drawTopBar priority after timed action feedback is:
     * statBarMessage -> logMessage -> facingEntity name -> empty.
     * A native logMessage owner does not exist yet, but FORCE_MESSAGE is
     * already permanent. Never let the new facing fallback overwrite it.
     */
    if (feedback == ACTION_FEEDBACK_NONE) {
        statusMessage = EspNativeGameplayStatusMessage_view();
        if (statusMessage != NULL &&
            EspMapStatusMessage_isActive(statusMessage)) {
            if (!EspNativeGameplayStatusMessage_repaintCurrent()) return 0;
            printf("[TOPBARFALLBACK] PAINT source=status priority=statBarMessage>facing present=caller\n");
            return 1;
        }

        facingLabel = EspNativeGameplayFacingLabel_view();
        if (facingLabel != NULL && facingLabel->displayable != 0U) {
            text = facingLabel->name;
        }
    }

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

        if (feedback != ACTION_FEEDBACK_NONE) {
            printf("[ACTIONFEEDBACK] PAINT kind=%u text=\"%s\" chars=%u reads=%u bytes=%u present=caller durationMs=%u\n",
                   (unsigned int)feedback,
                   text,
                   (unsigned int)visible,
                   (unsigned int)stats.packReads,
                   (unsigned int)stats.bytesRead,
                   (unsigned int)FEEDBACK_DISPLAY_MS);
        }
        else {
            printf("[FACINGLABEL] PAINT name=\"%s\" chars=%u source=%s index=%u priority=fallback reads=%u bytes=%u present=caller\n",
                   text,
                   (unsigned int)visible,
                   facingLabel != NULL && facingLabel->isLine != 0U
                       ? "line" : "sprite",
                   (unsigned int)(facingLabel != NULL &&
                                  facingLabel->isLine != 0U
                                      ? facingLabel->lineIndex
                                      : (facingLabel != NULL
                                             ? facingLabel->spriteIndex
                                             : ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX)),
                   (unsigned int)stats.packReads,
                   (unsigned int)stats.bytesRead);
        }
    }
    else if (feedback != ACTION_FEEDBACK_NONE) {
        printf("[ACTIONFEEDBACK] CLEAR mode=topbar-only reads=%u bytes=%u present=caller\n",
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead);
    }
    else {
        printf("[FACINGLABEL] CLEAR source=none priority=fallback reads=%u bytes=%u present=caller\n",
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead);
    }
    if (!EspNativeGameplayHud_paintTopTouchNotches()) goto done;
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (ok && feedback == ACTION_FEEDBACK_NONE) {
        EspNativeGameplayFacingLabel_markPainted();
    }
    return ok;
}

int __wrap_Esp32PlatformVideo_present(void) {
    uint8_t feedback = ACTION_FEEDBACK_NONE;
    int hadFeedback = 0;
    int flashPainted = 0;
    int ok;

    /* A full-screen loading owner is physically visible while checkpoint
     * resume/cache priming is allowed to mutate the logical framebuffer behind
     * it. Treat every gameplay present as acknowledged-but-suppressed until the
     * session explicitly releases that owner. This also prevents stale
     * feedback/viewport-flash compositors from painting onto the loading card. */
    if (EspNativeTransitionPresentation_isLoadingActive()) {
        return 1;
    }

    /*
     * Touch feedback owns an exact whole-frame baseline from begin() until
     * restore(). Its saved edit list contains only the pixels modified by the
     * touch overlay, so any unrelated compositor mutation during this lease
     * makes the final full-frame FNV intentionally fail closed.
     *
     * Therefore an active touch lease is a strict presentation barrier:
     * physically present the framebuffer exactly as it stands, but defer all
     * action-feedback, facing-label and viewport-flash painting/state changes
     * until the resident owner restores and releases the touch snapshot.
     *
     * Do not consume framebufferFresh here. If a fresh world frame ever reaches
     * this barrier, the deferred compositor still needs to observe it after the
     * lease is released.
     */
    if (EspNativeGameplayControls_isActive()) {
        return __real_Esp32PlatformVideo_present();
    }

    if (actionState.feedbackPending != 0U) {
        feedback = actionState.feedbackKind;
        if (!paintFeedback(feedback)) {
            printf("[ACTIONFEEDBACK] FAILED kind=%u\n", (unsigned int)feedback);
            return 0;
        }
        hadFeedback = 1;
    }
    else if (actionState.feedbackVisible != 0U &&
             actionState.framebufferFresh != 0U &&
             !EspAssetPack_isOpen()) {
        /*
         * A full gameplay redraw can occur while a 1200 ms top-bar lease is
         * still active (for example when a short impact overlay expires).
         * Repaint the already-visible message onto that fresh frame without
         * restarting its timer. Dialog-owned PAK leases remain authoritative:
         * in that case defer the repaint instead of escalating a temporary
         * ownership conflict.
         */
        feedback = actionState.feedbackVisibleKind;
        if (!paintFeedback(feedback)) {
            printf("[ACTIONFEEDBACK] FAILED kind=%u phase=refresh\n",
                   (unsigned int)feedback);
            return 0;
        }
        printf("[ACTIONFEEDBACK] REFRESH kind=%u lease=preserved freshFrame=yes\n",
               (unsigned int)feedback);
    }
    else if ((actionState.framebufferFresh != 0U ||
              EspNativeGameplayFacingLabel_isDirty()) &&
             !EspAssetPack_isOpen()) {
        /* No timed message owns the top bar. Recompose the permanent fallback
         * on every fresh world frame, and also when a pose-only refresh changed
         * the derived facing label before an otherwise plain present. */
        if (!paintFeedback(ACTION_FEEDBACK_NONE)) {
            printf("[TOPBARFALLBACK] FAILED phase=refresh\n");
            return 0;
        }
    }

    if ((actionState.viewportFlashPending != 0U ||
         actionState.viewportFlashVisible != 0U) &&
        (actionState.viewportFlashPending != 0U ||
         actionState.framebufferFresh != 0U)) {
        if (!paintViewportFlash()) {
            printf("[VIEWFLASH] FAILED phase=paint\n");
            return 0;
        }
        flashPainted = 1;
    }

    ok = __real_Esp32PlatformVideo_present();
    if (!ok) return 0;

    if (flashPainted && actionState.viewportFlashPending != 0U) {
        actionState.viewportFlashPending = 0U;
        actionState.viewportFlashVisible = 1U;
        actionState.viewportFlashShownAtMs = actionNowMs();
        printf("[VIEWFLASH] PAINT color565=%04x viewport=0,%u,%u,%u thickness=%u pixels=%u durationMs=%u snapshot=bounded present=caller feedback=%u\n",
               (unsigned int)actionState.viewportFlashColor565,
               (unsigned int)FEEDBACK_VIEW_Y,
               (unsigned int)DOOMRPG_LOGICAL_WIDTH,
               (unsigned int)FEEDBACK_VIEW_HEIGHT,
               (unsigned int)FEEDBACK_BORDER_THICKNESS,
               (unsigned int)FEEDBACK_BORDER_PIXELS,
               (unsigned int)actionState.viewportFlashDurationMs,
               (unsigned int)feedback);
    }
    actionState.framebufferFresh = 0U;

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

static int serviceViewportFlashExpiry(void) {
    uint32_t now;
    uint32_t elapsed;
    uint16_t duration;

    if (actionState.viewportFlashVisible == 0U) return 1;
    now = actionNowMs();
    elapsed = now - actionState.viewportFlashShownAtMs;
    duration = actionState.viewportFlashDurationMs;
    if (duration == 0U || elapsed < duration) return 1;
    if (EspNativeGameplayControls_isActive()) return 1;
    if (!restoreViewportFlash()) {
        printf("[VIEWFLASH] FAILED phase=restore\n");
        return 0;
    }
    if (!__real_Esp32PlatformVideo_present()) return 0;
    printf("[VIEWFLASH] EXPIRE elapsedMs=%u targetMs=%u color565=%04x restored=viewport-border-only\n",
           (unsigned int)elapsed, (unsigned int)duration,
           (unsigned int)actionState.viewportFlashColor565);
    actionState.viewportFlashVisible = 0U;
    actionState.viewportFlashShownAtMs = 0U;
    actionState.viewportFlashDurationMs = 0U;
    actionState.viewportFlashColor565 = 0U;
    return 1;
}

static int serviceFeedbackExpiry(void) {
    uint32_t now;
    uint32_t elapsed;
    uint8_t kind;

    if (!serviceViewportFlashExpiry()) return 0;
    if (actionState.feedbackVisible == 0U) return 1;
    now = actionNowMs();
    elapsed = now - actionState.feedbackShownAtMs;
    if (elapsed < FEEDBACK_DISPLAY_MS) return 1;

    /* Touch feedback owns a strict framebuffer snapshot until its 120 ms lease
     * is restored. Do not mutate the top bar underneath that lease: the next
     * resident service restores the touch overlay first, then this expiry may
     * safely repaint/present the message bar. */
    if (EspNativeGameplayControls_isActive()) return 1;

    /* Native dialogs intentionally keep the PAK open for their typewriter
     * lifetime. The top-bar painter requires an exclusive short PAK lease, so
     * an expired feedback must wait for the dialog to close instead of turning
     * a temporary storage-owner conflict into a fatal gameplay failure. The
     * viewport flash is independent and has already expired above on schedule. */
    if (EspAssetPack_isOpen()) return 1;

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
    int crateEligibility = -1;
    int32_t crateParm = 0;
    uint16_t crateDefTile = 0U;
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
    if (route == ACTION_ROUTE_CRATE_SUBTYPE2) {
        crateEligibility = crateWeaponEligible(
            &target, weapon, &crateDefTile, &crateParm);
        if (crateEligibility < 0) {
            ++actionState.destructibleDeferred;
            printf("[CRATE] DEFER seq=%u sprite=%u reason=source-definition-not-ready mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex);
            return status;
        }
        if (crateEligibility == 0) route = ACTION_ROUTE_NOTHING;
    }
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
    else if (route == ACTION_ROUTE_BARREL_SUBTYPE1) {
        const EspNativeGameplayPlayerState* player;
        const EspNativeGameplayWeaponSpec* weaponSpec;
        const EspPlayerViewState* playerView;
        uint32_t worldDistance = 0U;
        uint8_t haveAmmo;

        player = EspNativeGameplayPlayerState_view();
        weaponSpec = EspNativeGameplayCombatMath_weapon(weapon);
        playerView = EspPlayerView_view();
        if (player == NULL || player->active != 1U ||
            playerView == NULL || playerView->active != 1U ||
            weaponSpec == NULL || weapon >= ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS ||
            removed(target.spriteIndex) ||
            !targetWorldDistance(&target, playerView, &worldDistance)) {
            ++actionState.destructibleDeferred;
            printf("[BARREL] DEFER seq=%u sprite=%u reason=owner-or-standard-weapon-preflight weapon=%u mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon);
            return status;
        }
        /* Legacy rocket/BFG hits also call Game_radiusHurtEntities() after the
         * barrel dies. Keep those two weapons fail-closed until the complete
         * radius family is owned; do not silently destroy only the barrel. */
        if (weaponSpec->radialDamage != 0U) {
            ++actionState.destructibleDeferred;
            printf("[BARREL] DEFER seq=%u sprite=%u weapon=%u reason=radius-damage-family-not-owned mutation=no rngConsumed=0 ammoConsumed=0\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon);
            return status;
        }

        haveAmmo = EspNativeGameplayPlayerState_ammo(weaponSpec->ammoType);
        if (weaponSpec->ammoUsage != 0U && haveAmmo < weaponSpec->ammoUsage) {
            if (!EspNativeGameplayActionEngine_queueFeedback(
                    ACTION_FEEDBACK_NO_AMMO)) {
                ++actionState.destructibleDeferred;
                printf("[BARREL] DEFER seq=%u sprite=%u reason=no-ammo-feedback-busy mutation=no\n",
                       (unsigned int)intent->sequence,
                       (unsigned int)target.spriteIndex);
                return status;
            }
            printf("[BARREL] NOAMMO seq=%u sprite=%u weapon=%u ammoType=%u need=%u have=%u message=\"Not enough ammo!\" mutation=no rngConsumed=0 turnAdvance=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon,
                   (unsigned int)weaponSpec->ammoType,
                   (unsigned int)weaponSpec->ammoUsage,
                   (unsigned int)haveAmmo);
            return status;
        }

        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = target.spriteIndex;
        actionState.pending.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = (uint8_t)route;
        actionState.pending.feedback = ACTION_FEEDBACK_NONE;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        actionState.pending.worldChanged = 1U;
        actionState.pending.active = 1U;
        printf("[BARREL] ARM seq=%u sprite=%u tile=%u weapon=%u distanceTiles=%u worldDist=%u ammoType=%u ammoUsage=%u loops=%u radial=0 consequence=one-damaging-hit-removes persistence=v5-removal-owner rollback=player+rng+world\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.tileIndex,
               (unsigned int)weapon,
               (unsigned int)target.distance,
               (unsigned int)worldDistance,
               (unsigned int)weaponSpec->ammoType,
               (unsigned int)weaponSpec->ammoUsage,
               (unsigned int)weaponSpec->attackLoops);
    }
    else if (route == ACTION_ROUTE_CRATE_SUBTYPE2) {
        const EspNativeGameplayPlayerState* player;
        const EspNativeGameplayWeaponSpec* weaponSpec;
        const EspPlayerViewState* playerView;
        uint32_t worldDistance = 0U;
        uint8_t haveAmmo;

        player = EspNativeGameplayPlayerState_view();
        weaponSpec = EspNativeGameplayCombatMath_weapon(weapon);
        playerView = EspPlayerView_view();
        if (!EspNativeGameplayCrateState_ensure() ||
            player == NULL || player->active != 1U ||
            playerView == NULL || playerView->active != 1U ||
            weaponSpec == NULL || weapon >= ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS ||
            EspNativeGameplayCrateState_isTransformed(target.spriteIndex) ||
            removed(target.spriteIndex) ||
            !targetWorldDistance(&target, playerView, &worldDistance)) {
            ++actionState.destructibleDeferred;
            printf("[CRATE] DEFER seq=%u sprite=%u reason=owner-or-standard-weapon-preflight weapon=%u mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon);
            return status;
        }
        if (weaponSpec->radialDamage != 0U) {
            ++actionState.destructibleDeferred;
            printf("[CRATE] DEFER seq=%u sprite=%u weapon=%u reason=radius-damage-family-not-owned mutation=no rngConsumed=0 ammoConsumed=0\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon);
            return status;
        }

        haveAmmo = EspNativeGameplayPlayerState_ammo(weaponSpec->ammoType);
        if (weaponSpec->ammoUsage != 0U && haveAmmo < weaponSpec->ammoUsage) {
            if (!EspNativeGameplayActionEngine_queueFeedback(
                    ACTION_FEEDBACK_NO_AMMO)) {
                ++actionState.destructibleDeferred;
                printf("[CRATE] DEFER seq=%u sprite=%u reason=no-ammo-feedback-busy mutation=no\n",
                       (unsigned int)intent->sequence,
                       (unsigned int)target.spriteIndex);
                return status;
            }
            printf("[CRATE] NOAMMO seq=%u sprite=%u weapon=%u ammoType=%u need=%u have=%u message=\"Not enough ammo!\" mutation=no rngConsumed=0 turnAdvance=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)weapon,
                   (unsigned int)weaponSpec->ammoType,
                   (unsigned int)weaponSpec->ammoUsage,
                   (unsigned int)haveAmmo);
            return status;
        }

        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = target.spriteIndex;
        actionState.pending.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = (uint8_t)route;
        actionState.pending.feedback = ACTION_FEEDBACK_NONE;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        /* Service owns the actual world mutation after RNG/combat preflight.
         * This flag means full transactional action service is required. */
        actionState.pending.worldChanged = 1U;
        actionState.pending.active = 1U;
        printf("[CRATE] ARM seq=%u sprite=%u tile=%u defTile=%u parm=%08x weapon=%u distanceTiles=%u worldDist=%u ammoType=%u ammoUsage=%u loops=%u radial=0 transformPersistence=deferred rollback=player+rng+world\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.tileIndex,
               (unsigned int)crateDefTile,
               (unsigned int)crateParm,
               (unsigned int)weapon,
               (unsigned int)target.distance,
               (unsigned int)worldDistance,
               (unsigned int)weaponSpec->ammoType,
               (unsigned int)weaponSpec->ammoUsage,
               (unsigned int)weaponSpec->attackLoops);
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
    EspNativeSpriteRenderer_clearTransient();
    EspNativeGameplayCrateState_reset();
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

static const char* crateOutcomeName(
    EspNativeGameplayCrateOutcome outcome) {
    switch (outcome) {
    case ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE:
        return "TRAPPED_REMOVE";
    case ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM:
        return "TRANSFORM";
    case ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE:
        return "BREAK_REMOVE";
    default:
        return "NO_EFFECT";
    }
}

int EspNativeGameplayActionEngine_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view = EspPlayerView_view();
    ActionPending pending;

    if (!serviceFeedbackExpiry()) return 0;
    /* PASS TURN and future non-SELECT actions may queue transient feedback
     * without owning an ActionPending transaction. If no gameplay redraw has
     * consumed it yet, present the existing framebuffer with the queued topbar.
     * A native dialog may currently own the PAK, in which case the feedback
     * painter must simply wait for that bounded owner to release it. */
    if (actionState.pending.active == 0U && actionState.feedbackPending != 0U) {
        if (EspAssetPack_isOpen()) return 1;
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
        EspNativeGameplayAttackRoll crateRoll;
        EspNativeGameplayCrateOutcome crateOutcome =
            ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID;
        const EspNativeGameplayWeaponSpec* crateWeapon = NULL;
        int isFire = pending.route == ACTION_ROUTE_FIRE_CLEARED;
        int isJammedDoor = pending.route == ACTION_ROUTE_JAMMED_DOOR_CLEARED;
        int isBarrel = pending.route == ACTION_ROUTE_BARREL_SUBTYPE1;
        int isCrate = pending.route == ACTION_ROUTE_CRATE_SUBTYPE2;
        int animateWeapon = isFire || isJammedDoor || isBarrel || isCrate;
        Random_t randomBefore;
        uint32_t playerFNVBefore = 0U;
        uint32_t playerFNVAfter = 0U;
        uint32_t crateWorldDistance = 0U;
        uint32_t crateCombatRng = 0U;
        uint16_t crateEffectiveDefTile = 0U;
        uint8_t ammoBefore = 0U;
        uint8_t ammoAfter = 0U;
        uint8_t playerCaptured = 0U;
        uint8_t randomCaptured = 0U;
        uint8_t randHit = 0U;
        uint8_t destructibleMutated = 0U;
        uint8_t barrelRemoved = 0U;
        uint8_t barrelTurnRequested = 0U;
        uint8_t barrelExplosionArmed = 0U;
        uint8_t barrelExplosionFrame = 0U;
        uint8_t barrelChainCount = 0U;
        uint8_t barrelExpectedChainCount = 0U;
        uint8_t barrelBlastRngCalls = 0U;
        uint8_t barrelPlayerHits = 0U;
        uint16_t barrelTotalPlayerMessageDamage = 0U;
        uint16_t barrelChain[ACTION_BARREL_CHAIN_MAX];
        EspNativeSpriteTransient
            barrelWaveItems[ACTION_BARREL_CHAIN_MAX];
        int16_t barrelWorldX = 0;
        int16_t barrelWorldY = 0;
        uint8_t crateRandFirst = 0U;
        uint8_t crateRandSecond = 0U;
        uint8_t crateRandSecondValid = 0U;
        uint8_t crateTransformed = 0U;
        uint8_t crateRemoved = 0U;
        uint8_t crateTurnRequested = 0U;
        uint8_t crateTrapArmed = 0U;
        uint8_t crateTrapPreviewByte = 0U;
        uint8_t crateTrapDamage = 0U;
        uint8_t crateTrapFrame = 0U;
        int16_t crateTrapWorldX = 0;
        int16_t crateTrapWorldY = 0;
        EspNativeGameplayPlayerDamageResult crateTrapPlayerDamage;

        memset(&frame, 0, sizeof(frame));
        memset(&crateTrapPlayerDamage, 0, sizeof(crateTrapPlayerDamage));
        memset(&playerBefore, 0, sizeof(playerBefore));
        memset(&crateRoll, 0, sizeof(crateRoll));
        memset(barrelChain, 0xff, sizeof(barrelChain));
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

        if (isBarrel) {
            ActionTarget barrelTarget;
            EspMapSprite barrelSprite;
            const EspNativeGameplayPlayerState* player;
            EspNativeGraphicsCatalogStatus graphicsStatus;

            memset(&barrelTarget, 0, sizeof(barrelTarget));
            memset(&barrelSprite, 0, sizeof(barrelSprite));
            barrelTarget.spriteIndex = pending.spriteIndex;
            barrelTarget.tileIndex = pending.tileIndex;
            barrelTarget.lineIndex = pending.lineIndex;
            barrelTarget.type = pending.type;
            barrelTarget.subtype = pending.subtype;
            barrelTarget.distance = pending.distance;
            barrelTarget.isLine = 0U;

            crateWeapon = EspNativeGameplayCombatMath_weapon(pending.weapon);
            player = EspNativeGameplayPlayerState_view();
            if (crateWeapon == NULL || crateWeapon->radialDamage != 0U ||
                player == NULL || player->active != 1U ||
                removed(pending.spriteIndex) ||
                !targetWorldDistance(&barrelTarget, view, &crateWorldDistance) ||
                !__real_EspMapRuntime_getMapSprite(
                    pending.spriteIndex, &barrelSprite) ||
                !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARREL] FAILED seq=%u sprite=%u reason=transaction-preflight mutation=no rngConsumed=0\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }

            playerCaptured = 1U;
            randomCaptured = 1U;
            randomBefore = doomRpg->random;
            playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
            ammoBefore = EspNativeGameplayPlayerState_ammo(crateWeapon->ammoType);
            ammoAfter = ammoBefore;
            if (crateWeapon->ammoUsage != 0U &&
                !EspNativeGameplayPlayerState_consumeAmmo(
                    crateWeapon->ammoType, crateWeapon->ammoUsage,
                    &ammoBefore, &ammoAfter)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARREL] FAILED seq=%u sprite=%u reason=ammo-preflight-race playerRollback=yes rngRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }

            player = EspNativeGameplayPlayerState_view();
            memset(&crateRoll, 0, sizeof(crateRoll));
            if (player == NULL ||
                !EspNativeGameplayCombatMath_rollDestructibleAttack(
                    doomRpg, pending.weapon, player,
                    crateWorldDistance, &crateRoll)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARREL] FAILED seq=%u sprite=%u reason=combat-roll playerRollback=yes rngRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }
            playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

            if (crateRoll.hitLoops == 0U ||
                crateRoll.totalDamage + crateRoll.totalArmorDamage == 0) {
                /* Legacy stage 2 reports No effect! for non-enemy misses,
                 * including barrels. Ammo/RNG and the player turn still commit. */
                pending.feedback = ACTION_FEEDBACK_NO_EFFECT;
                printf("[BARREL] NO-EFFECT seq=%u sprite=%u weapon=%u worldDist=%u loops=%u hits=%u damage=%ld armorDamage=%ld rng=%u firstHitRand=%u firstCalc=%ld ammo=%u->%u mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)pending.weapon,
                       (unsigned int)crateWorldDistance,
                       (unsigned int)crateRoll.loops,
                       (unsigned int)crateRoll.hitLoops,
                       (long)crateRoll.totalDamage,
                       (long)crateRoll.totalArmorDamage,
                       (unsigned int)crateRoll.rngCalls,
                       (unsigned int)crateRoll.randHit[0],
                       (long)crateRoll.calcHit[0],
                       (unsigned int)ammoBefore,
                       (unsigned int)ammoAfter);
            }
            else {
                ActionTarget unsupported;
                uint8_t preflightCount = 0U;
                int32_t rngEnd;

                /* Entity_died(type=12/subtype=1) allocates gsprite animation #1
                 * at the exact sprite coordinate, then Game_remove()s the barrel.
                 * Native logical sprite 180 / frames 0..2 is the already proven
                 * equivalent used by the crate-trap path. */
                memset(&unsupported, 0, sizeof(unsupported));
                if (!barrelChainPreflight(pending.spriteIndex, view,
                                          barrelChain, &preflightCount,
                                          &unsupported)) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    doomRpg->random = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=unsupported-radius-family targetSprite=%u line=%u tile=%u type=%u subtype=%u chainCap=%u playerSentinel=%u playerRollback=yes rngRollback=yes mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)unsupported.spriteIndex,
                           (unsigned int)unsupported.lineIndex,
                           (unsigned int)unsupported.tileIndex,
                           (unsigned int)unsupported.type,
                           (unsigned int)unsupported.subtype,
                           (unsigned int)ACTION_BARREL_CHAIN_MAX,
                           0xfeU);
                    return 1;
                }
                barrelExpectedChainCount = preflightCount;
                rngEnd = doomRpg->random.nextRand +
                         ((int32_t)sizeof(int) *
                          (int32_t)barrelExpectedChainCount);
                if (doomRpg->random.nextRand < 0 ||
                    rngEnd >= RANDTABLESIZE) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    doomRpg->random = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=rng-word-refill-boundary nextRand=%d explosions=%u bytesPerWord=%u playerRollback=yes rngRollback=yes mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           doomRpg->random.nextRand,
                           (unsigned int)barrelExpectedChainCount,
                           (unsigned int)sizeof(int));
                    return 1;
                }

                graphicsStatus = EspNativeGraphicsCatalog_ensureSprite(
                    ACTION_TRAP_EXPLOSION_LOGICAL);
                if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                    graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    doomRpg->random = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARREL] DEFER seq=%u sprite=%u reason=explosion-resource logical=%u catalogStatus=%u playerRollback=yes rngRollback=yes mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                           (unsigned int)graphicsStatus);
                    return 1;
                }

                /* The dry-run array proved capacity/support only. Runtime must
                 * rediscover neighbors at each 450 ms expiry so removals happen
                 * in the same causal order as Game_gsprite_update(). */
                memset(barrelChain, 0xff, sizeof(barrelChain));
                barrelChain[0] = pending.spriteIndex;
                barrelChainCount = 1U;
                setRemoved(pending.spriteIndex, 1);
                barrelRemoved = 1U;
                barrelWorldX = barrelSprite.x;
                barrelWorldY = barrelSprite.y;
                barrelExplosionArmed = 1U;
                printf("[BARRELRADIUS] PREFLIGHT seq=%u root=%u chain=%u radius=4-cardinal-full+4-diagonal-half targetFamily=barrel+player unsupported=fail-closed rngWords=%u visual=wave-batch-concurrent gameplayOrder=legacy-causal\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)barrelExpectedChainCount,
                       (unsigned int)barrelExpectedChainCount);
                printf("[BARREL] HIT seq=%u sprite=%u weapon=%u worldDist=%u loops=%u hits=%u damage=%ld armorDamage=%ld rng=%u ammo=%u->%u removed=0->1 explosion=logical%u/x%u pos=%d,%d persistence=v5-removal-owner\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)pending.weapon,
                       (unsigned int)crateWorldDistance,
                       (unsigned int)crateRoll.loops,
                       (unsigned int)crateRoll.hitLoops,
                       (long)crateRoll.totalDamage,
                       (long)crateRoll.totalArmorDamage,
                       (unsigned int)crateRoll.rngCalls,
                       (unsigned int)ammoBefore,
                       (unsigned int)ammoAfter,
                       (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                       (unsigned int)ACTION_TRAP_EXPLOSION_FRAMES,
                       (int)barrelWorldX,
                       (int)barrelWorldY);
            }

            if (!EspNativeGameplayMonsterTurn_requestPlayerAttack(
                    pending.sequence)) {
                if (barrelRemoved != 0U) {
                    barrelRollbackRemoved(barrelChain, barrelChainCount);
                }
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARREL] FAILED seq=%u sprite=%u reason=monster-turn-request-busy rollback=yes player=yes rng=yes world=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 1;
            }
            barrelTurnRequested = 1U;
        }

        if (isCrate) {
            ActionTarget crateTarget;
            const EspNativeGameplayPlayerState* player;

            memset(&crateTarget, 0, sizeof(crateTarget));
            crateTarget.spriteIndex = pending.spriteIndex;
            crateTarget.tileIndex = pending.tileIndex;
            crateTarget.lineIndex = pending.lineIndex;
            crateTarget.type = pending.type;
            crateTarget.subtype = pending.subtype;
            crateTarget.distance = pending.distance;
            crateTarget.isLine = 0U;

            crateWeapon = EspNativeGameplayCombatMath_weapon(pending.weapon);
            player = EspNativeGameplayPlayerState_view();
            if (crateWeapon == NULL || crateWeapon->radialDamage != 0U ||
                player == NULL || player->active != 1U ||
                !EspNativeGameplayCrateState_ensure() ||
                EspNativeGameplayCrateState_isTransformed(pending.spriteIndex) ||
                removed(pending.spriteIndex) ||
                !targetWorldDistance(&crateTarget, view, &crateWorldDistance) ||
                !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[CRATE] FAILED seq=%u sprite=%u reason=transaction-preflight mutation=no rngConsumed=0\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }

            playerCaptured = 1U;
            randomCaptured = 1U;
            randomBefore = doomRpg->random;
            playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
            ammoBefore = EspNativeGameplayPlayerState_ammo(crateWeapon->ammoType);
            ammoAfter = ammoBefore;
            if (crateWeapon->ammoUsage != 0U &&
                !EspNativeGameplayPlayerState_consumeAmmo(
                    crateWeapon->ammoType, crateWeapon->ammoUsage,
                    &ammoBefore, &ammoAfter)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[CRATE] FAILED seq=%u sprite=%u reason=ammo-preflight-race playerRollback=yes rngRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }
            player = EspNativeGameplayPlayerState_view();
            if (player == NULL ||
                !EspNativeGameplayCombatMath_rollDestructibleAttack(
                    doomRpg, pending.weapon, player,
                    crateWorldDistance, &crateRoll)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[CRATE] FAILED seq=%u sprite=%u reason=combat-roll playerRollback=yes rngRollback=yes mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 0;
            }
            crateCombatRng = crateRoll.rngCalls;
            playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

            if (crateRoll.hitLoops == 0U ||
                crateRoll.totalDamage + crateRoll.totalArmorDamage == 0) {
                pending.feedback = ACTION_FEEDBACK_NO_EFFECT;
                printf("[CRATE] NO-EFFECT seq=%u sprite=%u weapon=%u worldDist=%u loops=%u hits=%u damage=%ld armorDamage=%ld rngCombat=%u firstHitRand=%u firstCalc=%ld ammo=%u->%u consequenceRng=0 mutation=no\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)pending.weapon,
                       (unsigned int)crateWorldDistance,
                       (unsigned int)crateRoll.loops,
                       (unsigned int)crateRoll.hitLoops,
                       (long)crateRoll.totalDamage,
                       (long)crateRoll.totalArmorDamage,
                       (unsigned int)crateRoll.rngCalls,
                       (unsigned int)crateRoll.randHit[0],
                       (long)crateRoll.calcHit[0],
                       (unsigned int)ammoBefore,
                       (unsigned int)ammoAfter);
            }
            else {
                crateRandFirst = DoomRPG_randNextByte(&doomRpg->random);
                if (EspNativeGameplayCrateState_requiresSecondRng(
                        crateRandFirst)) {
                    crateRandSecond = DoomRPG_randNextByte(&doomRpg->random);
                    crateRandSecondValid = 1U;
                }
                if (!EspNativeGameplayCrateState_resolveOutcome(
                        crateRandFirst, crateRandSecond,
                        crateRandSecondValid, &crateOutcome,
                        &crateEffectiveDefTile)) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    doomRpg->random = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[CRATE] FAILED seq=%u sprite=%u reason=outcome-resolve first=%u second=%u secondValid=%u playerRollback=yes rngRollback=yes mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)crateRandFirst,
                           (unsigned int)crateRandSecond,
                           (unsigned int)crateRandSecondValid);
                    return 0;
                }

                if (crateOutcome ==
                    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE) {
                    EspNativeGraphicsCatalogStatus graphicsStatus;
                    const EspNativeGameplayPlayerState* trapPlayer;
                    int32_t previewHealthDamage;
                    int32_t previewArmor;

                    graphicsStatus = EspNativeGraphicsCatalog_ensureSprite(
                        ACTION_TRAP_EXPLOSION_LOGICAL);
                    if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                        graphicsStatus !=
                            ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATETRAP] DEFER seq=%u sprite=%u reason=explosion-resource logical=%u catalogStatus=%u playerRollback=yes rngRollback=yes mutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                               (unsigned int)graphicsStatus);
                        return 1;
                    }

                    if (!explosionPeekWordLowByte(
                            &doomRpg->random, &crateTrapPreviewByte)) {
                        int nextRand = doomRpg->random.nextRand;
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATETRAP] DEFER seq=%u sprite=%u reason=rng-word-refill-boundary nextRand=%d wordBytes=%u playerRollback=yes rngRollback=yes mutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               nextRand,
                               (unsigned int)sizeof(int));
                        return 1;
                    }
                    crateTrapDamage =
                        (uint8_t)((crateTrapPreviewByte / 11U) + 5U);
                    trapPlayer = EspNativeGameplayPlayerState_view();
                    if (trapPlayer == NULL) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATETRAP] DEFER seq=%u sprite=%u reason=player-view playerRollback=yes rngRollback=yes mutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex);
                        return 1;
                    }
                    previewArmor = (int32_t)((trapPlayer->param1 >> 16) & 0xffU);
                    previewHealthDamage = (int32_t)crateTrapDamage;
                    if (previewArmor < (int32_t)crateTrapDamage) {
                        previewHealthDamage +=
                            (int32_t)crateTrapDamage - previewArmor;
                    }
                    if ((int32_t)(trapPlayer->param1 & 0xffU) -
                            previewHealthDamage <=
                        0) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATETRAP] LETHAL-DEFER seq=%u sprite=%u previewByte=%u blast=%u hp=%u armor=%u playerDeathState=not-owned playerRollback=yes rngRollback=yes worldMutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)crateTrapPreviewByte,
                               (unsigned int)crateTrapDamage,
                               (unsigned int)(trapPlayer->param1 & 0xffU),
                               (unsigned int)((trapPlayer->param1 >> 16) & 0xffU));
                        return 1;
                    }

                    crateTrapWorldX =
                        (int16_t)(((pending.tileIndex % MAP_WIDTH) * TILE_SIZE) +
                                  TILE_CENTER);
                    crateTrapWorldY =
                        (int16_t)(((pending.tileIndex / MAP_WIDTH) * TILE_SIZE) +
                                  TILE_CENTER);
                    crateTrapArmed = 1U;
                    printf("[CRATETRAP] ARM seq=%u sprite=%u tile=%u logical=%u frames=%u cadence=legacy-150ms/render-bounded pos=%d,%d previewByte=%u blast=%u sound=5061-deferred shake=200ms-deferred otherRadiusEntities=deferred rngWord=byte-offset-low/4B desktopPointerIndexBug=avoided rngCommit=after-animation\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)pending.tileIndex,
                           (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                           (unsigned int)ACTION_TRAP_EXPLOSION_FRAMES,
                           (int)crateTrapWorldX,
                           (int)crateTrapWorldY,
                           (unsigned int)crateTrapPreviewByte,
                           (unsigned int)crateTrapDamage);
                }

                if (crateOutcome == ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM) {
                    EspNativeGraphicsCatalogStatus graphicsStatus =
                        EspNativeGraphicsCatalog_ensureSprite(
                            crateEffectiveDefTile);
                    if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                        graphicsStatus !=
                            ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATE] FAILED seq=%u sprite=%u reason=graphics-preflight defTile=%u catalogStatus=%u playerRollback=yes rngRollback=yes mutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)crateEffectiveDefTile,
                               (unsigned int)graphicsStatus);
                        return 0;
                    }
                    if (graphicsStatus == ESP_NATIVE_GRAPHICS_CATALOG_OK) {
                        const EspNativeGraphicsCatalogView* graphics =
                            EspNativeGraphicsCatalog_view();
                        printf("[NATIVEGFX] DYNAMIC-SPRITE resource=%u source=crate-transform sprites=%u storageBytes=%u admission=atomic\n",
                               (unsigned int)crateEffectiveDefTile,
                               graphics != NULL
                                   ? (unsigned int)graphics->spriteCount : 0U,
                               graphics != NULL
                                   ? (unsigned int)graphics->storageBytes : 0U);
                    }
                    if (!EspNativeGameplayCrateState_transform(
                            pending.spriteIndex, crateEffectiveDefTile)) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0, sizeof(actionState.pending));
                        printf("[CRATE] FAILED seq=%u sprite=%u reason=transform-commit defTile=%u playerRollback=yes rngRollback=yes mutation=no\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)crateEffectiveDefTile);
                        return 0;
                    }
                    crateTransformed = 1U;
                }
                else {
                    setRemoved(pending.spriteIndex, 1);
                    crateRemoved = 1U;
                    if (crateOutcome ==
                        ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE) {
                        pending.feedback = ACTION_FEEDBACK_TRAPPED;
                    }
                }

                printf("[CRATE] CONSEQUENCE seq=%u sprite=%u first=%u second=%u secondValid=%u outcome=%s effectiveDefTile=%u rngCombat=%u rngConsequence=%u attackDamage=%ld attackArmorDamage=%ld mutation=%s explosion=%s message=%s persistence=%s\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)crateRandFirst,
                       (unsigned int)crateRandSecond,
                       (unsigned int)crateRandSecondValid,
                       crateOutcomeName(crateOutcome),
                       (unsigned int)crateEffectiveDefTile,
                       (unsigned int)crateCombatRng,
                       (unsigned int)(1U + crateRandSecondValid),
                       (long)crateRoll.totalDamage,
                       (long)crateRoll.totalArmorDamage,
                       crateTransformed != 0U ? "transform-overlay" : "removed-overlay",
                       crateOutcome == ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE
                           ? "native-180x3-armed" : "none",
                       pending.feedback == ACTION_FEEDBACK_TRAPPED
                           ? "Trapped!" : "none",
                       crateTransformed != 0U ? "deferred-v6" : "v5-removal-owner");
            }

            if (!EspNativeGameplayMonsterTurn_requestPlayerAttack(
                    pending.sequence)) {
                int rollbackOk = 1;
                if (crateTransformed != 0U) {
                    rollbackOk = EspNativeGameplayCrateState_rollbackTransform(
                        pending.spriteIndex, crateEffectiveDefTile);
                }
                if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[CRATE] FAILED seq=%u sprite=%u reason=monster-turn-request-busy rollback=%s player=yes rng=yes world=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
            crateTurnRequested = 1U;
        }

        if (pending.feedback != ACTION_FEEDBACK_NONE) {
            actionState.feedbackPending = 1U;
            actionState.feedbackKind = pending.feedback;
        }
        if (!EspNativeGameplayFacingLabel_refresh("ACTION-WORLD-COMMIT")) {
            printf("[FACINGLABEL] DEFER reason=ACTION-WORLD-COMMIT worldRender=continue\n");
        }
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
            else if (isBarrel) {
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (barrelRemoved != 0U) {
                    barrelRollbackRemoved(barrelChain, barrelChainCount);
                }
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                if (randomCaptured != 0U) doomRpg->random = randomBefore;
            }
            else if (isCrate) {
                if (crateTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (crateTransformed != 0U &&
                    !EspNativeGameplayCrateState_rollbackTransform(
                        pending.spriteIndex, crateEffectiveDefTile)) {
                    rollbackOk = 0;
                }
                if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                if (randomCaptured != 0U) doomRpg->random = randomBefore;
            }
            memset(&frame, 0, sizeof(frame));
            if (rollbackOk &&
                !EspNativeGameplayFacingLabel_refresh("ACTION-WORLD-ROLLBACK")) {
                printf("[FACINGLABEL] DEFER reason=ACTION-WORLD-ROLLBACK worldRender=continue\n");
            }
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
                   (isJammedDoor || isBarrel || isCrate) ? "restored" : "unchanged",
                   (isFire || isBarrel || isCrate) ? "restored" : "unchanged",
                   (unsigned int)frame.frameAfterFNV);
            memset(&actionState.pending, 0, sizeof(actionState.pending));
            return 1;
        }

        logActionFrame(&pending, animateWeapon ? "attack" : "commit", &frame);

        if (isBarrel && barrelExplosionArmed != 0U) {
            uint8_t waveStart = 0U;
            uint8_t waveOrdinal = 0U;

            /*
             * Legacy Game_gsprite_update() lets every barrel killed by the
             * same radius tick allocate its own gsprite immediately. Those
             * siblings therefore animate concurrently for the next 450 ms.
             * Snapshot chainCount at each wave boundary: radius-triggered
             * barrels are removed immediately, then rendered together as the
             * next wave before any of their own radius effects execute.
             */
            while (waveStart < barrelChainCount) {
                uint8_t waveEnd = barrelChainCount;
                uint8_t waveCount = (uint8_t)(waveEnd - waveStart);
                uint8_t waveItem;
                uint8_t blastCursor;

                ++waveOrdinal;
                for (waveItem = 0U; waveItem < waveCount; ++waveItem) {
                    EspMapSprite explodingSprite;
                    uint16_t spriteIndex =
                        barrelChain[(uint8_t)(waveStart + waveItem)];
                    if (!__real_EspMapRuntime_getMapSprite(
                            spriteIndex, &explodingSprite)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        barrelRollbackRemoved(
                            barrelChain, barrelChainCount);
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0,
                               sizeof(actionState.pending));
                        printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u reason=wave-sprite-read wave=%u item=%u rollback=%s rng=yes player=yes world=yes\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)waveOrdinal,
                               (unsigned int)waveItem,
                               rollbackOk ? "yes" : "NO");
                        return rollbackOk ? 1 : 0;
                    }
                    barrelWaveItems[waveItem].worldX =
                        explodingSprite.x;
                    barrelWaveItems[waveItem].worldY =
                        explodingSprite.y;
                }

                for (barrelExplosionFrame = 0U;
                     barrelExplosionFrame <
                         ACTION_TRAP_EXPLOSION_FRAMES;
                     ++barrelExplosionFrame) {
                    memset(&frame, 0, sizeof(frame));
                    if (!EspNativeSpriteRenderer_armTransientBatch(
                            ACTION_TRAP_EXPLOSION_LOGICAL,
                            barrelExplosionFrame,
                            barrelWaveItems,
                            waveCount) ||
                        !EspNativeGameplayFrame_renderTurn(
                            doomRpg->render,
                            (uint8_t)view->viewAngle,
                            &frame)) {
                        int rollbackOk = 1;
                        EspNativeSpriteRenderer_clearTransient();
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        barrelRollbackRemoved(
                            barrelChain, barrelChainCount);
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        doomRpg->random = randomBefore;
                        actionState.feedbackPending = 0U;
                        actionState.feedbackKind =
                            ACTION_FEEDBACK_NONE;
                        actionState.feedbackText[0] = '\0';
                        actionState.viewportFlashPending = 0U;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&frame, 0, sizeof(frame));
                        if (rollbackOk &&
                            EspNativeGameplayFacingLabel_refresh(
                                "BARREL-ROLLBACK")) {
                            rollbackOk =
                                EspNativeGameplayFrame_renderTurn(
                                    doomRpg->render,
                                    (uint8_t)view->viewAngle,
                                    &frame);
                        }
                        printf("[BARREL] ROLLBACK seq=%u root=%u reason=wave-frame wave=%u active=%u frame=%u rollback=%s rng=yes player=yes world=yes stableFrame=%08x\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)waveOrdinal,
                               (unsigned int)waveCount,
                               (unsigned int)barrelExplosionFrame,
                               rollbackOk ? "yes" : "NO",
                               rollbackOk
                                   ? (unsigned int)frame.frameAfterFNV
                                   : 0U);
                        memset(&actionState.pending, 0,
                               sizeof(actionState.pending));
                        return rollbackOk ? 1 : 0;
                    }
                    printf("[BARREL] WAVE-FRAME seq=%u root=%u wave=%u active=%u chainRange=%u..%u ordinal=%u/%u anim=%u logical=%u frame=%08x presented=%u\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)waveOrdinal,
                           (unsigned int)waveCount,
                           (unsigned int)(waveStart + 1U),
                           (unsigned int)waveEnd,
                           (unsigned int)(barrelExplosionFrame + 1U),
                           (unsigned int)ACTION_TRAP_EXPLOSION_FRAMES,
                           (unsigned int)barrelExplosionFrame,
                           (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                           (unsigned int)frame.frameAfterFNV,
                           (unsigned int)frame.finalPresented);
                }
                EspNativeSpriteRenderer_clearTransient();

                /*
                 * All explosions in this wave reach 450 ms together. Their
                 * radius calls still execute deterministically in allocation
                 * order, preserving legacy RNG and duplicate-barrel suppression.
                 * Any newly killed barrel is appended to the next wave.
                 */
                for (blastCursor = waveStart;
                     blastCursor < waveEnd;
                     ++blastCursor) {
                    uint8_t blastByte = 0U;
                    uint8_t blastDamage;
                    uint8_t triggered = 0U;
                    uint8_t playerHitsThisBlast = 0U;
                    uint16_t playerMessageDamageThisBlast = 0U;

                    if (!explosionConsumeWordLowByte(
                            &doomRpg->random, &blastByte)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        barrelRollbackRemoved(
                            barrelChain, barrelChainCount);
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0,
                               sizeof(actionState.pending));
                        printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u exploding=%u wave=%u reason=rng-word-refill-boundary rollback=%s rng=yes player=yes world=yes\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)barrelChain[blastCursor],
                               (unsigned int)waveOrdinal,
                               rollbackOk ? "yes" : "NO");
                        return rollbackOk ? 1 : 0;
                    }
                    ++barrelBlastRngCalls;
                    blastDamage =
                        (uint8_t)((blastByte / 11U) + 5U);
                    if (!barrelTriggerNeighbors(
                            barrelChain[blastCursor],
                            view,
                            blastDamage,
                            barrelChain,
                            &barrelChainCount,
                            &triggered,
                            &playerHitsThisBlast,
                            &playerMessageDamageThisBlast)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        barrelRollbackRemoved(
                            barrelChain, barrelChainCount);
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        doomRpg->random = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0,
                               sizeof(actionState.pending));
                        printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u exploding=%u wave=%u reason=runtime-radius-contract-or-player-death rollback=%s rng=yes player=yes world=yes\n",
                               (unsigned int)pending.sequence,
                               (unsigned int)pending.spriteIndex,
                               (unsigned int)barrelChain[blastCursor],
                               (unsigned int)waveOrdinal,
                               rollbackOk ? "yes" : "NO");
                        return rollbackOk ? 1 : 0;
                    }
                    barrelPlayerHits =
                        (uint8_t)(barrelPlayerHits +
                                  playerHitsThisBlast);
                    barrelTotalPlayerMessageDamage =
                        (uint16_t)(
                            barrelTotalPlayerMessageDamage +
                            playerMessageDamageThisBlast);
                    printf("[BARRELRADIUS] BLAST seq=%u root=%u source=%u wave=%u rngByte=%u damage=%u diagonal=%u triggered=%u playerHits=%u playerMessageDamage=%u totalPlayerMessageDamage=%u chainNow=%u/%u sound=5061-deferred shake=200ms-deferred\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)barrelChain[blastCursor],
                           (unsigned int)waveOrdinal,
                           (unsigned int)blastByte,
                           (unsigned int)blastDamage,
                           (unsigned int)(blastDamage >> 1),
                           (unsigned int)triggered,
                           (unsigned int)playerHitsThisBlast,
                           (unsigned int)playerMessageDamageThisBlast,
                           (unsigned int)barrelTotalPlayerMessageDamage,
                           (unsigned int)barrelChainCount,
                           (unsigned int)barrelExpectedChainCount);
                }

                waveStart = waveEnd;
            }

            if (barrelPlayerHits != 0U) {
                int written = snprintf(
                    actionState.feedbackText,
                    sizeof(actionState.feedbackText),
                    "%u damage!",
                    (unsigned int)barrelTotalPlayerMessageDamage);
                if (written <= 0 ||
                    (size_t)written >=
                        sizeof(actionState.feedbackText)) {
                    int rollbackOk = 1;
                    if (barrelTurnRequested != 0U &&
                        !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                            pending.sequence)) {
                        rollbackOk = 0;
                    }
                    barrelRollbackRemoved(
                        barrelChain, barrelChainCount);
                    if (!EspNativeGameplayPlayerState_restore(
                            &playerBefore)) {
                        rollbackOk = 0;
                    }
                    doomRpg->random = randomBefore;
                    actionState.feedbackPending = 0U;
                    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                    actionState.feedbackText[0] = '\0';
                    actionState.viewportFlashPending = 0U;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0,
                           sizeof(actionState.pending));
                    printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u reason=player-feedback-format rollback=%s rng=yes player=yes world=yes\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           rollbackOk ? "yes" : "NO");
                    return rollbackOk ? 1 : 0;
                }
                actionState.feedbackPending = 1U;
                actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
                actionState.viewportFlashPending = 1U;
                actionState.viewportFlashDurationMs =
                    ACTION_TRAP_DAMAGE_FLASH_MS;
                actionState.viewportFlashColor565 =
                    FEEDBACK_DAMAGE_RED565;
                playerFNVAfter =
                    EspNativeGameplayPlayerState_fingerprint();
                printf("[BARRELRADIUS] PLAYER-COMMIT seq=%u hits=%u totalMessageDamage=%u playerFNV=%08x->%08x feedback=DAMAGE-AGGREGATED flash=%ums legacyHud=queued-messages/nativeBounded=one-summary\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)barrelPlayerHits,
                       (unsigned int)barrelTotalPlayerMessageDamage,
                       (unsigned int)playerFNVBefore,
                       (unsigned int)playerFNVAfter,
                       (unsigned int)ACTION_TRAP_DAMAGE_FLASH_MS);
            }

            if (barrelChainCount != barrelExpectedChainCount) {
                int rollbackOk = 1;
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                barrelRollbackRemoved(
                    barrelChain, barrelChainCount);
                if (!EspNativeGameplayPlayerState_restore(
                        &playerBefore)) {
                    rollbackOk = 0;
                }
                doomRpg->random = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0,
                       sizeof(actionState.pending));
                printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u reason=preflight-runtime-chain-mismatch expected=%u actual=%u rollback=%s rng=yes player=yes world=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)barrelExpectedChainCount,
                       (unsigned int)barrelChainCount,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
        }

        if (isCrate && crateTrapArmed != 0U) {
            uint8_t blastByte;
            EspNativeGameplayPlayerDamageStatus damageStatus;

            for (crateTrapFrame = 0U;
                 crateTrapFrame < ACTION_TRAP_EXPLOSION_FRAMES;
                 ++crateTrapFrame) {
                memset(&frame, 0, sizeof(frame));
                if (!EspNativeSpriteRenderer_armTransient(
                        ACTION_TRAP_EXPLOSION_LOGICAL,
                        crateTrapFrame,
                        crateTrapWorldX,
                        crateTrapWorldY) ||
                    !EspNativeGameplayFrame_renderTurn(
                        doomRpg->render, (uint8_t)view->viewAngle, &frame)) {
                    int rollbackOk = 1;
                    EspNativeSpriteRenderer_clearTransient();
                    if (crateTurnRequested != 0U &&
                        !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                            pending.sequence)) {
                        rollbackOk = 0;
                    }
                    if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                    if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                        rollbackOk = 0;
                    }
                    doomRpg->random = randomBefore;
                    actionState.feedbackPending = 0U;
                    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                    actionState.viewportFlashPending = 0U;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&frame, 0, sizeof(frame));
                    if (rollbackOk &&
                        EspNativeGameplayFacingLabel_refresh(
                            "CRATETRAP-ROLLBACK")) {
                        rollbackOk = EspNativeGameplayFrame_renderTurn(
                            doomRpg->render,
                            (uint8_t)view->viewAngle,
                            &frame);
                    }
                    printf("[CRATETRAP] ROLLBACK seq=%u sprite=%u reason=animation-frame-%u rollback=%s rng=yes player=yes world=yes stableFrame=%08x\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)crateTrapFrame,
                           rollbackOk ? "yes" : "NO",
                           rollbackOk
                               ? (unsigned int)frame.frameAfterFNV : 0U);
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    return rollbackOk ? 1 : 0;
                }
                printf("[CRATETRAP] FRAME seq=%u sprite=%u ordinal=%u/%u anim=%u logical=%u pos=%d,%d frame=%08x presented=%u\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)(crateTrapFrame + 1U),
                       (unsigned int)ACTION_TRAP_EXPLOSION_FRAMES,
                       (unsigned int)crateTrapFrame,
                       (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                       (int)crateTrapWorldX,
                       (int)crateTrapWorldY,
                       (unsigned int)frame.frameAfterFNV,
                       (unsigned int)frame.finalPresented);
            }
            EspNativeSpriteRenderer_clearTransient();

            if (!explosionConsumeWordLowByte(&doomRpg->random, &blastByte)) {
                int rollbackOk = 1;
                if (crateTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                doomRpg->random = randomBefore;
                actionState.feedbackPending = 0U;
                actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                actionState.viewportFlashPending = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&frame, 0, sizeof(frame));
                if (rollbackOk &&
                    EspNativeGameplayFacingLabel_refresh(
                        "CRATETRAP-RNG-ROLLBACK")) {
                    rollbackOk = EspNativeGameplayFrame_renderTurn(
                        doomRpg->render,
                        (uint8_t)view->viewAngle,
                        &frame);
                }
                printf("[CRATETRAP] ROLLBACK seq=%u sprite=%u reason=rng-word-refill-boundary rollback=%s rng=yes player=yes world=yes stableFrame=%08x\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       rollbackOk ? "yes" : "NO",
                       rollbackOk ? (unsigned int)frame.frameAfterFNV : 0U);
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                return rollbackOk ? 1 : 0;
            }
            damageStatus = EspNativeGameplayPlayerState_applyDamageNonlethal(
                crateTrapDamage,
                crateTrapDamage,
                &crateTrapPlayerDamage);
            if (blastByte != crateTrapPreviewByte ||
                damageStatus != ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_OK) {
                int rollbackOk = 1;
                if (crateTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                doomRpg->random = randomBefore;
                actionState.feedbackPending = 0U;
                actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                actionState.viewportFlashPending = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&frame, 0, sizeof(frame));
                if (rollbackOk &&
                    EspNativeGameplayFacingLabel_refresh(
                        "CRATETRAP-BLAST-ROLLBACK")) {
                    rollbackOk = EspNativeGameplayFrame_renderTurn(
                        doomRpg->render,
                        (uint8_t)view->viewAngle,
                        &frame);
                }
                printf("[CRATETRAP] ROLLBACK seq=%u sprite=%u reason=%s previewByte=%u liveByte=%u damageStatus=%u rollback=%s rng=yes player=yes world=yes stableFrame=%08x\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       blastByte != crateTrapPreviewByte
                           ? "rng-preview-mismatch" : "player-damage",
                       (unsigned int)crateTrapPreviewByte,
                       (unsigned int)blastByte,
                       (unsigned int)damageStatus,
                       rollbackOk ? "yes" : "NO",
                       rollbackOk
                           ? (unsigned int)frame.frameAfterFNV : 0U);
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                return rollbackOk ? 1 : 0;
            }

            playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

            /*
             * Legacy timing is two distinct HUD messages: Entity_died() posts
             * "Trapped!" when the crate arms gsprite explosion #1, then the
             * 450 ms gsprite expiry calls Game_radiusHurtEntities() ->
             * Player_pain(), which posts "<damage+armorDamage> damage!".
             *
             * Keep the already-visible TRAPPED lease throughout the three
             * explosion frames, then stage the shared dynamic DAMAGE feedback
             * for the settle frame after the player mutation commits. For this
             * trap both legacy damage components equal crateTrapDamage, so the
             * displayed amount is their sum even when armor absorbs one half.
             */
            if (snprintf(actionState.feedbackText,
                         sizeof(actionState.feedbackText),
                         "%u damage!",
                         (unsigned int)crateTrapDamage * 2U) <= 0) {
                int rollbackOk = 1;
                if (crateTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (crateRemoved != 0U) setRemoved(pending.spriteIndex, 0);
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                doomRpg->random = randomBefore;
                actionState.feedbackPending = 0U;
                actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                actionState.feedbackText[0] = '\0';
                actionState.viewportFlashPending = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&frame, 0, sizeof(frame));
                if (rollbackOk &&
                    EspNativeGameplayFacingLabel_refresh(
                        "CRATETRAP-FEEDBACK-ROLLBACK")) {
                    rollbackOk = EspNativeGameplayFrame_renderTurn(
                        doomRpg->render,
                        (uint8_t)view->viewAngle,
                        &frame);
                }
                printf("[CRATETRAP] ROLLBACK seq=%u sprite=%u reason=damage-feedback-format rollback=%s rng=yes player=yes world=yes stableFrame=%08x\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       rollbackOk ? "yes" : "NO",
                       rollbackOk ? (unsigned int)frame.frameAfterFNV : 0U);
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                return rollbackOk ? 1 : 0;
            }
            pending.feedback = ACTION_FEEDBACK_DAMAGE;
            actionState.viewportFlashPending = 1U;
            actionState.viewportFlashDurationMs = ACTION_TRAP_DAMAGE_FLASH_MS;
            actionState.viewportFlashColor565 = FEEDBACK_DAMAGE_RED565;
            printf("[CRATETRAP] BLAST seq=%u sprite=%u rngByte=%u blast=%u rngCalls=1 hp=%u->%u armor=%u->%u playerFNV=%08x->%08x radius=adjacent-cardinal-player otherRadiusEntities=deferred message=\"%u damage!\" redFlash=%ums sound=5061-deferred shake=200ms-deferred\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)blastByte,
                   (unsigned int)crateTrapDamage,
                   (unsigned int)crateTrapPlayerDamage.healthBefore,
                   (unsigned int)crateTrapPlayerDamage.healthAfter,
                   (unsigned int)crateTrapPlayerDamage.armorBefore,
                   (unsigned int)crateTrapPlayerDamage.armorAfter,
                   (unsigned int)crateTrapPlayerDamage.stateFNVBefore,
                   (unsigned int)crateTrapPlayerDamage.stateFNVAfter,
                   (unsigned int)crateTrapDamage * 2U,
                   (unsigned int)ACTION_TRAP_DAMAGE_FLASH_MS);
        }

        if (isBarrel) {
            printf("[BARREL] COMMIT seq=%u sprite=%u weapon=%u ammoType=%u ammo=%u->%u playerFNV=%08x->%08x loops=%u hits=%u rngCombat=%u rngBlast=%u damage=%ld armorDamage=%ld removed=%u chainRemoved=%u playerRadiusHits=%u explosion=%s sound=%u-deferred barrelExplosionSound=5061-deferred radius=barrel-chain-wave+player-owned/other-hurtable-families-fail-closed turnAdvance=PLAYER_ATTACK-requested rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)pending.weapon,
                   crateWeapon != NULL ? (unsigned int)crateWeapon->ammoType : 0U,
                   (unsigned int)ammoBefore,
                   (unsigned int)ammoAfter,
                   (unsigned int)playerFNVBefore,
                   (unsigned int)playerFNVAfter,
                   (unsigned int)crateRoll.loops,
                   (unsigned int)crateRoll.hitLoops,
                   (unsigned int)crateRoll.rngCalls,
                   (unsigned int)barrelBlastRngCalls,
                   (long)crateRoll.totalDamage,
                   (long)crateRoll.totalArmorDamage,
                   (unsigned int)barrelRemoved,
                   (unsigned int)(barrelExplosionArmed != 0U
                                      ? barrelChainCount : 0U),
                   (unsigned int)barrelPlayerHits,
                   barrelExplosionArmed != 0U ? "native-180x3-each" : "none",
                   crateWeapon != NULL ? (unsigned int)crateWeapon->resourceId : 0U);
        }

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
        if (isCrate) {
            uint8_t effectiveType = 0U;
            uint8_t effectiveSubtype = 0U;
            int32_t effectiveParm = 0;
            if (crateTransformed != 0U) {
                (void)EspEntityDefTypeCatalog_getMetadata(
                    crateEffectiveDefTile, &effectiveType,
                    &effectiveSubtype, &effectiveParm);
            }
            printf("[CRATE] COMMIT seq=%u sprite=%u weapon=%u ammoType=%u ammo=%u->%u playerFNV=%08x->%08x loops=%u hits=%u rngCombat=%u rngConsequence=%u outcome=%s effective=%u/%u/def%u removed=%u transformed=%u sound=%u-deferred particles=impact-debris-deferred trapExplosion=%s trapBlast=%u otherRadiusEntities=%s turnAdvance=PLAYER_ATTACK-requested rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)pending.weapon,
                   crateWeapon != NULL ? (unsigned int)crateWeapon->ammoType : 0U,
                   (unsigned int)ammoBefore,
                   (unsigned int)ammoAfter,
                   (unsigned int)playerFNVBefore,
                   (unsigned int)playerFNVAfter,
                   (unsigned int)crateRoll.loops,
                   (unsigned int)crateRoll.hitLoops,
                   (unsigned int)crateRoll.rngCalls,
                   crateOutcome != ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID
                       ? (unsigned int)(1U + crateRandSecondValid) : 0U,
                   crateOutcomeName(crateOutcome),
                   (unsigned int)effectiveType,
                   (unsigned int)effectiveSubtype,
                   (unsigned int)crateEffectiveDefTile,
                   (unsigned int)crateRemoved,
                   (unsigned int)crateTransformed,
                   crateWeapon != NULL ? (unsigned int)crateWeapon->resourceId : 0U,
                   crateTrapArmed != 0U ? "native-180x3" : "none",
                   (unsigned int)(crateTrapArmed != 0U ? crateTrapDamage : 0U),
                   crateTrapArmed != 0U ? "deferred" : "n/a");
            (void)effectiveParm;
        }

        if (animateWeapon) {
            EspNativeGameplayFrameStats settle;
            memset(&settle, 0, sizeof(settle));
            if (pending.feedback != ACTION_FEEDBACK_NONE) {
                actionState.feedbackPending = 1U;
                actionState.feedbackKind = pending.feedback;
            }
            if (EspNativeGameplayFrame_renderTurn(
                    doomRpg->render, (uint8_t)view->viewAngle, &settle)) {
                logActionFrame(&pending, "settle-idle", &settle);
                printf("[ACTIONENGINE] ATTACK seq=%u weapon=%u frame=1->0 generic=yes worldCommitted=%s\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.weapon,
                       (isCrate && crateOutcome ==
                                      ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID) ||
                               (isBarrel && barrelRemoved == 0U)
                           ? "no-effect" : "yes");
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
