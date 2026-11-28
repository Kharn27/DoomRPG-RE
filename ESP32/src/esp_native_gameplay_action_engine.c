#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "esp_native_root_state.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "esp_native_process_rng_owner.h"
#include "Render.h"

#include "doomrpg_log.h"
#include "esp_asset_pack.h"
#include "esp_native_audio_intent.h"
#include "esp_audio_entrance_music.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_barrel_radius.h"
#include "esp_native_gameplay_explosion.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_native_gameplay_destructible.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_facing_label.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_level_up.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_dynamic_line_render.h"
#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include "esp_native_graphics_catalog.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_pickup_miniature.h"
#include "esp_native_gameplay_player_pain_visual.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_resident_gameplay.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_native_gameplay_status_message.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_native_indexed_bmp.h"
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
#define ACTION_BARREL_RADIUS_NEIGHBORS ESP_NATIVE_EXPLOSION_NEIGHBORS

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
#define FEEDBACK_DYNAMIC_TEXT_BYTES ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_TEXT_BYTES
#define FEEDBACK_LEGACY_QUEUE_MAX ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_QUEUE_MAX
#define FEEDBACK_FOLLOWUP_MAX (FEEDBACK_LEGACY_QUEUE_MAX - 1U)
#define FEEDBACK_DAMAGE_RED565 0xb800U
#define FEEDBACK_VIEW_Y FEEDBACK_TOP_HEIGHT
#define FEEDBACK_VIEW_HEIGHT (DOOMRPG_LOGICAL_HEIGHT - (FEEDBACK_TOP_HEIGHT * 2U))
#define FEEDBACK_BORDER_THICKNESS 2U
/* One shared 500ms flash clock: the frame and direction are ALWAYS in
 * the same ON/OFF state. Three brisk pulses: 105ms ON / 65ms OFF.
 * Only a change of the combined border state requests physical present. */
#define FEEDBACK_DAMAGE_BLINK_ON_MS 105U
#define FEEDBACK_DAMAGE_BLINK_GAP_MS 65U
#define FEEDBACK_DAMAGE_BLINK_PULSES 3U
/* The original border remains 2px opaque. The direction adds 10px of
 * 50%-alpha RGB565 red-orange inside it: 12px total, 24px physical. */
#define DAMAGE_BORDER_DIRECTION_THICKNESS 12U
#define DAMAGE_BORDER_INNER_THICKNESS \
    (DAMAGE_BORDER_DIRECTION_THICKNESS - FEEDBACK_BORDER_THICKNESS)
#define DAMAGE_BORDER_INNER_MAX_PIXELS \
    ((DOOMRPG_LOGICAL_WIDTH - 2U * FEEDBACK_BORDER_THICKNESS) * \
     DAMAGE_BORDER_INNER_THICKNESS)
#define DAMAGE_BORDER_TINT565 0xfa20U
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
    ACTION_ROUTE_BARREL_SUBTYPE1 = 8,
    ACTION_ROUTE_FIRE_RANGE_MISS = 9
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
    uint32_t deferredMonsterSequence;
    uint16_t deferredMonsterSprite;
    uint16_t deferredMonsterTile;
    uint8_t deferredMonsterSubtype;
    uint8_t deferredMonsterReserved;
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
    uint8_t viewportFlashLit; /* shared border+side ON bit, NO second clock */
    uint8_t framebufferFresh;
    char feedbackText[FEEDBACK_DYNAMIC_TEXT_BYTES];
    char feedbackFollowupText[FEEDBACK_FOLLOWUP_MAX]
                             [FEEDBACK_DYNAMIC_TEXT_BYTES];
    uint8_t feedbackFollowupCount;
    uint8_t feedbackFollowupHead;
    uint8_t feedbackFollowupKind;
    uint8_t feedbackFollowupReserved;
} ActionEngineState;

typedef struct FeedbackScratch_s {
    /*
     * Bar and font metadata are never live at the same time: the bar is tiled
     * before the font is opened. Reuse one bounded descriptor to cut the
     * SELECT -> frame -> top-bar stack path without changing presentation.
     */
    EspNativeIndexedBmp bmp;
} FeedbackScratch;

static ActionEngineState actionState;
static uint16_t viewportBorderSnapshot[FEEDBACK_BORDER_PIXELS];

/* One 500ms flash owner, one shared ON/OFF bit. The inner 10px of the
 * chosen side is tinted translucently, never a separate pulse lease. */
typedef struct DirectionalCueOwner_s {
    EspNativeGameplayDamageCue pending;
    uint16_t snapshotPixels;
    uint8_t pendingValid;
    uint8_t activeSide;
    uint8_t snapshotValid;
    uint8_t workActive;
    uint8_t stageNonlethalBorder;
} DirectionalCueOwner;
static DirectionalCueOwner directionCue;
static uint16_t directionCueSnapshot[DAMAGE_BORDER_INNER_MAX_PIXELS];
_Static_assert(DAMAGE_BORDER_INNER_MAX_PIXELS == 1560U,
               "directional tint snapshot must stay within 3.2 KB BSS");

int EspMapRuntime_getMapSpriteBase(uint32_t index, EspMapSprite* outSprite);
int EspMapSpriteTopology_getVisualStateBase(uint32_t spriteIndex,
                                               uint8_t* outVisualState);
int EspMapSpriteTopology_getEntityBase(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder);

static uint32_t actionNowMs(void) {
    /* Keep legacy/ESP32 header boundaries clean: this recovered runtime clock
     * is already the canonical millisecond source for the native gameplay
     * service, so do not pull ESP-IDF timer headers into legacy C units. */
    return DoomRPG_GetUpTimeMS();
}

/* Type-1 explosions use the shared guarded word RNG at wave expiry. */

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
    if (!EspMapSpriteTopology_getEntityBase(spriteIndex, outType, outSubType,
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

static void clearFeedbackFollowups(void) {
    memset(actionState.feedbackFollowupText, 0,
           sizeof(actionState.feedbackFollowupText));
    actionState.feedbackFollowupCount = 0U;
    actionState.feedbackFollowupHead = 0U;
    actionState.feedbackFollowupKind = ACTION_FEEDBACK_NONE;
    actionState.feedbackFollowupReserved = 0U;
}

int EspNativeGameplayActionEngine_queueFeedback(
    EspNativeGameplayActionFeedback feedback) {
    if (feedback <= ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE ||
        feedback > ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT ||
        !ensureOwner() || actionState.pending.active != 0U ||
        actionState.feedbackPending != 0U) {
        return 0;
    }
    clearFeedbackFollowups();
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
    clearFeedbackFollowups();
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

int EspNativeGameplayActionEngine_queuePickupFeedbackBatch(
    const char* const* texts,
    uint8_t count,
    uint16_t viewportFlashMs) {
    uint8_t i;
    size_t lengths[FEEDBACK_LEGACY_QUEUE_MAX];

    if (texts == NULL || count == 0U ||
        count > FEEDBACK_LEGACY_QUEUE_MAX ||
        !ensureOwner() || actionState.pending.active != 0U ||
        actionState.feedbackPending != 0U) {
        return 0;
    }
    memset(lengths, 0, sizeof(lengths));
    for (i = 0U; i < count; ++i) {
        if (texts[i] == NULL) return 0;
        lengths[i] = strnlen(texts[i], FEEDBACK_DYNAMIC_TEXT_BYTES);
        if (lengths[i] == 0U ||
            lengths[i] >= FEEDBACK_DYNAMIC_TEXT_BYTES) {
            return 0;
        }
    }

    clearFeedbackFollowups();
    memset(actionState.feedbackText, 0, sizeof(actionState.feedbackText));
    memcpy(actionState.feedbackText, texts[0], lengths[0] + 1U);
    for (i = 1U; i < count; ++i) {
        memcpy(actionState.feedbackFollowupText[i - 1U],
               texts[i], lengths[i] + 1U);
    }
    actionState.feedbackFollowupCount = (uint8_t)(count - 1U);
    actionState.feedbackFollowupHead = 0U;
    actionState.feedbackFollowupKind = ACTION_FEEDBACK_PICKUP;
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_PICKUP;
    if (viewportFlashMs != 0U) {
        actionState.viewportFlashPending = 1U;
        actionState.viewportFlashDurationMs = viewportFlashMs;
        actionState.viewportFlashColor565 = 0xffffU;
    }
    printf("[ACTIONFEEDBACK] QUEUE-BATCH kind=%u count=%u followups=%u flashMs=%u legacyCap=%u owner=bounded-static\n",
           (unsigned int)ACTION_FEEDBACK_PICKUP,
           (unsigned int)count,
           (unsigned int)actionState.feedbackFollowupCount,
           (unsigned int)viewportFlashMs,
           (unsigned int)FEEDBACK_LEGACY_QUEUE_MAX);
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
    clearFeedbackFollowups();
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

int EspNativeGameplayActionEngine_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState) {
    if (!EspMapSpriteTopology_getVisualStateBase(spriteIndex,
                                                    outVisualState)) {
        return 0;
    }
    if (outVisualState != NULL && removed(spriteIndex)) {
        *outVisualState |= ACTION_VISUAL_HIDDEN;
    }
    return 1;
}

int EspNativeGameplayActionEngine_getEntity(uint32_t spriteIndex,
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
        if (type == ACTION_ENTITY_ENEMY) {
            /*
             * actionGetEntity() deliberately reads the private action-engine
             * leaf (to avoid recursing through MonsterCombat's public topology
             * wrapper). That leaf still reports immutable BSP linkage for a
             * dead enemy. Entity_died() unlinks it in the original game;
             * the native MonsterState.alive bit is authoritative instead.
             * Match findRadiusSpriteOnTile() and MonsterTrace_forward() so a
             * corpse can neither shadow a fire/other blocker farther along
             * this ray nor produce a spurious TRACE-MISMATCH. Living monsters
             * continue to block and no gameplay/RNG owner is mutated.
             */
            const EspNativeGameplayMonsterRecord* monster =
                EspNativeGameplayMonsterState_find((uint16_t)i);
            if (monster == NULL || monster->alive == 0U) continue;
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
static int findRadiusSpriteOnTile(uint16_t tile, uint16_t ignoredSprite,
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
        if (i == ignoredSprite) continue;
        if (!actionGetEntity(i, &type, &subtype, &linkState, &linkOrder)) {
            return -1;
        }
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !entityTypeInTraceMask(type)) {
            continue;
        }
        if (type == ACTION_ENTITY_ENEMY) {
            const EspNativeGameplayMonsterRecord* living =
                EspNativeGameplayMonsterState_find((uint16_t)i);
            if (living == NULL || living->alive == 0U) continue;
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

static int barrelRadiusCellSupportedExcept(uint16_t tile,
                                     const EspPlayerViewState* view,
                                     uint16_t ignoredSprite,
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
        /*
         * The linked type=0 line is never hurtable in the legacy, whatever
         * its subtype: Entity_initspawn() does not set info&0x40000 for
         * eType=0. This includes real EntityDef entries (e.g. type=0/sub=1)
         * as well as the synthetic type=0/sub=255 fallback. The original
         * Game_hurtEntityAt() sees the first linked entity and no-ops; it
         * does not propagate the blast to another entity on the same tile.
         * Jammed doors / power couplings are also no-ops. Keep every other
         * line family fail-closed rather than destroying it implicitly.
         */
        return EspNativeGameplayBarrelRadius_lineNoop(lineType, lineSubtype);
    }

    spriteResult = findRadiusSpriteOnTile(tile, ignoredSprite, outTarget);
    if (spriteResult < 0) return 0;
    if (spriteResult == 0) return 1;

    if (EspNativeGameplayBarrelRadius_entityNoop(outTarget->type,
                                                outTarget->subtype)) return 1;
    if (outTarget->type == ACTION_ENTITY_ENEMY) {
        const EspNativeGameplayMonsterRecord* victim =
            EspNativeGameplayMonsterState_find(outTarget->spriteIndex);
        /* Boss/spawn death families remain explicitly fail-closed.
         * The ordinary 0..6,9..11 families share the native death owner. */
        return victim != NULL && victim->alive == 1U &&
               victim->subtype == outTarget->subtype &&
               victim->subtype != 7U && victim->subtype != 8U &&
               victim->subtype != 12U && victim->subtype != 13U;
    }
    if (outTarget->type == ACTION_ENTITY_DESTRUCTIBLE &&
        outTarget->subtype == ACTION_DESTRUCTIBLE_BARREL_SUBTYPE) {
        *outHasBarrel = 1;
        return 1;
    }
    return 0;
}

static int barrelRadiusCellSupported(uint16_t tile,
                                     const EspPlayerViewState* view,
                                     ActionTarget* outTarget,
                                     int* outHasBarrel,
                                     int* outIsPlayer) {
    return barrelRadiusCellSupportedExcept(tile, view,
        ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE, outTarget, outHasBarrel, outIsPlayer);
}

/* Dry-run the whole barrel-connected component before consuming explosion RNG
 * or mutating world state. Barrel chains, player damage and bounded ordinary
 * monster damage are owned; other radius consequences remain fail-closed. */
static int barrelChainPreflight(uint16_t primarySprite,
                                uint8_t rootSubtype,
                                const EspPlayerViewState* view,
                                uint16_t* chain,
                                uint8_t* outCount,
                                uint8_t* outMonsterCount,
                                uint8_t* outPlayerCount,
                                ActionTarget* outUnsupported) {
    uint8_t count = 1U;
    uint8_t cursor;
    uint8_t monsterCount = 0U;
    uint8_t playerCount = 0U;
 
    if (view == NULL || chain == NULL || outCount == NULL ||
        outMonsterCount == NULL || outPlayerCount == NULL ||
        outUnsupported == NULL ||
        primarySprite == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
        (rootSubtype != ACTION_DESTRUCTIBLE_BARREL_SUBTYPE &&
         rootSubtype != ACTION_DESTRUCTIBLE_CRATE_SUBTYPE)) {
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

        if (!EspMapRuntime_getMapSpriteBase(chain[cursor], &source) ||
            !actionGetEntity(chain[cursor], &type, &subtype,
                             &linkState, &linkOrder) ||
            type != ACTION_ENTITY_DESTRUCTIBLE ||
            subtype != (cursor == 0U ? rootSubtype : ACTION_DESTRUCTIBLE_BARREL_SUBTYPE) ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U) {
            return 0;
        }
        (void)linkOrder;

        for (n = 0U; n < ACTION_BARREL_RADIUS_NEIGHBORS; ++n) {
            uint16_t tile;
            ActionTarget target;
            int hasBarrel;
            int isPlayer;

            if (!EspNativeExplosion_neighborTile(source.x, source.y, n,
                                                 &tile)) continue;
            if (!barrelRadiusCellSupportedExcept(tile, view, primarySprite,
                                                 &target, &hasBarrel, &isPlayer)) {
                *outUnsupported = target;
                return 0;
            }
            if (isPlayer) ++playerCount;
            if (target.type == ACTION_ENTITY_ENEMY) {
                 if (monsterCount == UINT8_MAX) return 0;
                ++monsterCount;
                continue;
            }
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
    /* A player cell can reserve a death byte before a later blast, just
     * like monster consequences can interleave RNG. */
     *outCount = count;
    *outMonsterCount = monsterCount;
    *outPlayerCount = playerCount;
    return 1;
}

static int barrelRollbackRemoved(DoomRPG_t* runtime,
                                 const uint16_t* chain, uint8_t count) {
    uint8_t i;
    const int exact =
        EspNativeGameplayMonsterCombat_cancelSplashSequence(runtime);
    if (!exact) {
        DRPG_LOGE("[BARRELRADIUS] ROLLBACK-FAILED owner=monster-splash stateExact=NO\n");
    }
    if (chain != NULL) {
        for (i = 0U; i < count; ++i) setRemoved(chain[i], 0);
    }
    return exact;
}

/* A type-1 blast is located at its current world coordinates, not its
 * immutable spawn tile. The existing barrel wrapper still reads its original
 * immutable center. A moving monster origin will use this same leaf only
 * after its transaction/preflight owner is implemented. */
typedef struct ExplosionImpactJournal_s {
    DoomRPG_t* runtime;
    uint32_t sequence;
    Random_t randomBeforeDeathByte;
    uint16_t monsterHits;
    uint8_t monsterSequenceArmed;
    uint8_t deathRand;
    uint8_t deathRandValid;
} ExplosionImpactJournal;

static int explosionTriggerNeighborsAt(uint16_t sourceSprite,
                                  int32_t sourceX, int32_t sourceY,
                                  const EspPlayerViewState* view,
                                  uint8_t blastDamage,
                                  uint16_t* chain,
                                  uint8_t* ioCount,
                                  ExplosionImpactJournal* journal,
                                  uint8_t* outTriggered,
                                  uint8_t* outPlayerHits,
                                  uint16_t* outPlayerMessageDamage,
                                  int allowTerminalDeath,
                                  uint8_t* outLethal) {
    uint8_t n;
    uint8_t triggered = 0U;
    uint8_t playerHits = 0U;
    uint16_t playerMessageDamage = 0U;

    if (view == NULL || chain == NULL || ioCount == NULL ||
        journal == NULL || journal->runtime == NULL || journal->sequence == 0U ||
        outTriggered == NULL || outPlayerHits == NULL ||
        outPlayerMessageDamage == NULL || outLethal == NULL ||
        blastDamage == 0U) {
        return 0;
    }

    for (n = 0U; n < ACTION_BARREL_RADIUS_NEIGHBORS; ++n) {
        uint16_t tile;
        ActionTarget target;
        int hasBarrel;
        int isPlayer;

        if (!EspNativeExplosion_neighborTile(sourceX, sourceY, n,
                                             &tile)) continue;
        if (!barrelRadiusCellSupported(tile, view, &target,
                                       &hasBarrel, &isPlayer)) {
            return 0;
        }
        if (isPlayer) {
            EspNativeGameplayPlayerDamageResult damageResult;
            EspNativeGameplayPlayerDamageStatus damageStatus;
            uint8_t component = EspNativeExplosion_component(blastDamage, n);
            memset(&damageResult, 0, sizeof(damageResult));
            if (component == 0U) continue;
            damageStatus = EspNativeGameplayPlayerState_applyDamageNonlethal(
                component, component, &damageResult);
            if (damageStatus ==
                    ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_DEFERRED &&
                allowTerminalDeath) {
                /* Player_died in the original is idempotent while DYING.
                 * Later blasts still apply armor/HP damage, but reserve the
                 * sound byte only on the first lethal cell. */
                damageStatus =
                    EspNativeGameplayPlayerState_applyDamageForDeath(
                        component, component, &damageResult);
                if (damageStatus ==
                    ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_COMMITTED) {
                    if (*outLethal != 0U) {
                        if (journal->deathRandValid == 0U ||
                            damageResult.healthBefore != 0U) return 0;
                        printf("[BARRELRADIUS] POST-DEATH-HIT seq=%u source=%u armor=%u->%u deathRng=unchanged\n",
                               (unsigned int)journal->sequence, (unsigned int)sourceSprite,
                               (unsigned int)damageResult.armorBefore,
                               (unsigned int)damageResult.armorAfter);
                    } else {
                    if (journal->deathRandValid != 0U) return 0;
                    journal->randomBeforeDeathByte = (*EspNativeProcessRng_live());
                    journal->deathRand = DoomRPG_randNextByte(EspNativeProcessRng_live());
                    journal->deathRandValid = 1U;
                    *outLethal = 1U;
                    printf("[BARRELRADIUS] DEATH-RNG-RESERVE seq=%u source=%u nextBefore=%d nextAfter=%d value=%u ordering=radius-cell transaction=pending\n",
                           (unsigned int)journal->sequence, (unsigned int)sourceSprite,
                           journal->randomBeforeDeathByte.nextRand,
                           EspNativeProcessRng_live()->nextRand,
                           (unsigned int)journal->deathRand);
                    }
                }
            }
            if (damageStatus != ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_OK &&
                damageStatus !=
                    ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_COMMITTED) {
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
                           ? "no/lethal-deferred-chain-complex"
                           : "no/invalid");
                return 0;
            }
            ++playerHits;
            playerMessageDamage =
                (uint16_t)((uint16_t)component * 2U);
            printf("[BARRELRADIUS] PLAYER-HIT source=%u tile=%u relation=%s component=%u messageDamage=%u hp=%u->%u armor=%u->%u playerFNV=%08x->%08x mutation=yes terminal=%s\n",
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
                   (unsigned int)damageResult.stateFNVAfter,
                   *outLethal ? "pending-death-arm" : "no");
            continue;
        }
        if (target.type == ACTION_ENTITY_ENEMY) {
            const uint8_t component = EspNativeExplosion_component(blastDamage, n);
            if (component == 0U) continue;
            EspNativeGameplaySplashHit hit;
            if (journal->monsterSequenceArmed == 0U ||
                journal->monsterHits >=
                    ACTION_BARREL_CHAIN_MAX * ESP_NATIVE_GAMEPLAY_SPLASH_MAX_HITS) return 0;
            hit.spriteIndex = target.spriteIndex;
            hit.healthComponent = component;
            hit.armorComponent = component;
            if (!EspNativeGameplayMonsterCombat_applySplash(
                    journal->runtime, &hit, 1U, journal->sequence)) return 0;
            ++journal->monsterHits;
            /* Legacy level-up restores HP. A level-up after terminal damage
             * requires a separate death/dialog contract; roll back for now. */
            if (*outLethal != 0U &&
                EspNativeGameplayPlayerState_health() != 0U) return 0;
            printf("[BARRELRADIUS] MONSTER-CELL source=%u sprite=%u tile=%u component=%u relation=%s mutation=provisional\n",
                   (unsigned int)sourceSprite, (unsigned int)target.spriteIndex,
                   (unsigned int)tile, (unsigned int)component,
                   n < 4U ? "cardinal" : "diagonal");
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

static int barrelTriggerNeighbors(uint16_t sourceSprite,
                                  const EspPlayerViewState* view,
                                  uint8_t blastDamage,
                                  uint16_t* chain,
                                  uint8_t* ioCount,
                                  ExplosionImpactJournal* journal,
                                  uint8_t* outTriggered,
                                  uint8_t* outPlayerHits,
                                  uint16_t* outPlayerMessageDamage,
                                  int allowTerminalDeath,
                                  uint8_t* outLethal) {
    EspMapSprite source;
    if (!EspMapRuntime_getMapSpriteBase(sourceSprite, &source)) return 0;
    return explosionTriggerNeighborsAt(sourceSprite, source.x, source.y,
        view, blastDamage, chain, ioCount, journal,
        outTriggered, outPlayerHits, outPlayerMessageDamage,
        allowTerminalDeath, outLethal);
}

/* Direct subtype8 origin. All damage goes through the shared radius leaf.
 * Mixed barrel/special-death origins are deliberately still deferred. */
int EspNativeGameplayActionEngine_prepareDeathBlast(
    uint16_t sourceSprite, EspNativeGameplayDeathBlastPlan* outPlan) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayMonsterPositionRecord* position;
    EspNativeGraphicsCatalogStatus graphicsStatus;
    const EspNativeGameplayMonsterRecord* source;
    uint8_t n, monsters = 0U, players = 0U;
    if (outPlan == NULL) return 0;
    memset(outPlan, 0, sizeof(*outPlan));
    if (view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle || !ensureOwner() ||
        !EspNativeGameplayMonsterPosition_ensure()) return 0;
    source = EspNativeGameplayMonsterState_find(sourceSprite);
    if (source == NULL || source->subtype != 8U || source->alive != 1U ||
        (source->param1 & 0xffU) != 0U) return 0;
    position = EspNativeGameplayMonsterPosition_find(sourceSprite);
    if (position == NULL || position->worldX >= 2048U ||
        position->worldY >= 2048U ||
        position->tileIndex != (uint16_t)((position->worldY >> 6U) * 32U +
                                         (position->worldX >> 6U))) return 0;
    for (n = 0U; n < ESP_NATIVE_EXPLOSION_NEIGHBORS; ++n) {
        uint16_t tile;
        ActionTarget target;
        int hasBarrel, isPlayer;
        if (!EspNativeExplosion_neighborTile(position->worldX,
                                             position->worldY, n, &tile)) continue;
        if (!barrelRadiusCellSupported(tile, view, &target, &hasBarrel,
                                       &isPlayer) || hasBarrel) {
            printf("[MONSTERBLAST] DEFER source=%u tile=%u reason=unsupported-mixed-radius mutation=no\n",
                   (unsigned int)sourceSprite, (unsigned int)tile);
            return 0;
        }
        if (isPlayer) ++players;
        else if (target.type == ACTION_ENTITY_ENEMY) ++monsters;
    }
    graphicsStatus = EspNativeGraphicsCatalog_ensureSprite(ACTION_TRAP_EXPLOSION_LOGICAL);
    if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
        graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) return 0;
    outPlan->sourceSprite = sourceSprite;
    outPlan->worldX = position->worldX;
    outPlan->worldY = position->worldY;
    outPlan->monsterCount = monsters;
    outPlan->playerCount = players;
    printf("[MONSTERBLAST] PREFLIGHT source=%u pos=%u,%u monsters=%u player=%u mixed=deferred\n",
           (unsigned int)sourceSprite, (unsigned int)outPlan->worldX,
           (unsigned int)outPlan->worldY, (unsigned int)monsters,
           (unsigned int)players);
    return 1;
}

int EspNativeGameplayActionEngine_runDeathBlast(
    DoomRPG_t* runtime, uint32_t sequence,
    const EspNativeGameplayDeathBlastPlan* plan,
    EspNativeGameplayDeathBlastResult* outResult) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayMonsterPositionRecord* position;
    const EspNativeGameplayMonsterRecord* source;
    ExplosionImpactJournal journal = {0};
    EspNativeGameplayFrameStats frame;
    uint16_t chain[ACTION_BARREL_CHAIN_MAX];
    uint16_t playerMessageDamage = 0U;
    uint8_t count = 0U, triggered = 0U, playerHits = 0U, lethal = 0U;
    uint8_t phase, i, damage;
    uint32_t word;
    int ok;
    if (outResult == NULL) return 0;
    memset(outResult, 0, sizeof(*outResult));
    if (runtime == NULL || sequence == 0U || plan == NULL || view == NULL ||
        view->active != 1U || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        EspNativeRootOwners_live()->render == NULL ||
        !EspNativeGameplayMonsterCombat_splashSequenceArmed(sequence)) return 0;
    source = EspNativeGameplayMonsterState_find(plan->sourceSprite);
    if (source == NULL || source->subtype != 8U || source->alive != 0U ||
        (source->param1 & 0xffU) != 0U) return 0;
    position = EspNativeGameplayMonsterPosition_find(plan->sourceSprite);
    if (position == NULL || position->worldX != plan->worldX ||
        position->worldY != plan->worldY) return 0;
    journal.runtime = runtime;
    journal.sequence = sequence;
    journal.monsterSequenceArmed = 1U;
    EspEntranceMusic_pumpGameplay();
    for (phase = 0U; phase < ACTION_TRAP_EXPLOSION_FRAMES; ++phase) {
#if defined(ESP_PLATFORM)
        const uint32_t started = DoomRPG_GetUpTimeMS();
#endif
        memset(&frame, 0, sizeof(frame));
        if (!EspNativeSpriteRenderer_armTransient(
                ACTION_TRAP_EXPLOSION_LOGICAL, phase,
                (int16_t)plan->worldX, (int16_t)plan->worldY) ||
            !EspNativeDynamicLineRender_renderAmbient(
                EspNativeRootOwners_live()->render, &frame) ||
            frame.finalPresented == 0U) {
            EspNativeSpriteRenderer_clearTransient();
            printf("[MONSTERBLAST] ABORT seq=%u source=%u phase=%u reason=frame-not-presented rollback=required\n",
                   (unsigned int)sequence, (unsigned int)plan->sourceSprite,
                   (unsigned int)phase);
            return 0;
        }
        printf("[MONSTERBLAST] FRAME seq=%u source=%u phase=%u/3 presented=1 frame=%08x\n",
               (unsigned int)sequence, (unsigned int)plan->sourceSprite,
               (unsigned int)(phase + 1U), (unsigned int)frame.frameAfterFNV);
        if (phase == 0U && frame.finalPresented != 0U)
            (void)EspNativeAudioIntent_publish(5061U, 0U, 4U);
        EspEntranceMusic_pumpGameplay();
#if defined(ESP_PLATFORM)
        /* Three original 150ms phases. Yield and feed the existing audio
         * owner between completed PAK/frame transactions. */
        while ((uint32_t)(DoomRPG_GetUpTimeMS() - started) < 150U) {
            vTaskDelay(1);
            EspEntranceMusic_pumpGameplay();
        }
#endif
    }
    EspNativeSpriteRenderer_clearTransient();
    word = (uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());
    damage = EspNativeExplosion_damageFromWord(word);
    ok = explosionTriggerNeighborsAt(plan->sourceSprite, plan->worldX,
        plan->worldY, view, damage, chain, &count, &journal, &triggered,
        &playerHits, &playerMessageDamage, 1, &lethal);
    /* Unexpected barrel discovery: undo that removal. All other changes
     * remain provisional in the enclosing combat snapshot. */
    for (i = 0U; i < count; ++i) setRemoved(chain[i], 0);
    if (!ok || count != 0U || triggered != 0U ||
        journal.monsterHits != plan->monsterCount ||
        playerHits != plan->playerCount ||
        (lethal != 0U) != (journal.deathRandValid != 0U)) return 0;
    outResult->monsterHits = journal.monsterHits;
    outResult->playerMessageDamage = playerMessageDamage;
    outResult->lethal = lethal;
    outResult->deathRand = journal.deathRand;
    printf("[MONSTERBLAST] RADIUS seq=%u source=%u pos=%u,%u word=%08x damage=%u monsters=%u player=%u lethal=%u transaction=pending\n",
           (unsigned int)sequence, (unsigned int)plan->sourceSprite,
           (unsigned int)plan->worldX, (unsigned int)plan->worldY,
           (unsigned int)word, (unsigned int)damage,
           (unsigned int)journal.monsterHits, (unsigned int)playerHits,
           (unsigned int)lethal);
    return 1;
}

/* Enemy-owned barrel attack. The shot is already animated; Combat_monsterSeq
 * forces a hit on non-player targets without hit/damage RNG. Use the same
 * removal, ordered radius, splash journal and terminal player death owners
 * as a player-origin barrel, but never charge ammo or request another turn. */
int EspNativeGameplayActionEngine_enemyBarrelReady(uint16_t sprite) {
    const EspPlayerViewState* view = EspPlayerView_view();
    uint16_t chain[ACTION_BARREL_CHAIN_MAX];
    uint8_t count, monsters, players;
    ActionTarget unsupported;
    EspNativeGraphicsCatalogStatus status;
    if (view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle || !ensureOwner() ||
        !barrelChainPreflight(sprite, ACTION_DESTRUCTIBLE_BARREL_SUBTYPE,
                              view, chain, &count, &monsters, &players,
                              &unsupported)) return 0;
    status = EspNativeGraphicsCatalog_ensureSprite(ACTION_TRAP_EXPLOSION_LOGICAL);
    return status == ESP_NATIVE_GRAPHICS_CATALOG_OK ||
           status == ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE;
}

int EspNativeGameplayActionEngine_enemyBarrelImpact(
    DoomRPG_t* runtime, uint32_t sequence, uint16_t rootSprite) {
    const EspPlayerViewState* view = EspPlayerView_view();
    uint16_t chain[ACTION_BARREL_CHAIN_MAX];
    uint8_t expected, monsters, players, count=0U, start=0U, lethal=0U;
    uint16_t messageDamage=0U;
    ActionTarget unsupported;
    ExplosionImpactJournal journal = {0};
    EspNativeGameplayPlayerState playerBefore;
    Random_t rngBefore;
    EspNativeGameplayFrameStats frame;
    /* Match the player barrel's bounded simultaneous wave storage. */
    EspNativeSpriteTransient items[ACTION_BARREL_CHAIN_MAX];
    if (runtime == NULL || sequence == 0U ||
        EspNativeRootOwners_live()->render == NULL ||
        !EspNativeGameplayActionEngine_enemyBarrelReady(rootSprite) ||
        !barrelChainPreflight(rootSprite, ACTION_DESTRUCTIBLE_BARREL_SUBTYPE,
                              view, chain, &expected, &monsters, &players,
                              &unsupported) ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) return 0;
    rngBefore = *EspNativeProcessRng_live();
    journal.runtime=runtime;
    journal.sequence=sequence;
    if (monsters != 0U) {
        if (!EspNativeGameplayMonsterCombat_beginSplashSequence(runtime,sequence))
            return 0;
        journal.monsterSequenceArmed=1U;
    }
    chain[0]=rootSprite;
    count=1U;
    setRemoved(rootSprite,1);
    while (start<count) {
        uint8_t end=count, n, phase;
        for (n=start;n<end;++n) {
            EspMapSprite spr;
            if (!EspMapRuntime_getMapSpriteBase(chain[n],&spr)) goto rollback;
            items[n-start].worldX=spr.x;
            items[n-start].worldY=spr.y;
        }
        for (phase=0U;phase<ACTION_TRAP_EXPLOSION_FRAMES;++phase) {
#if defined(ESP_PLATFORM)
            uint32_t started=DoomRPG_GetUpTimeMS();
#endif
            EspEntranceMusic_pumpGameplay();
            memset(&frame,0,sizeof(frame));
            if (!EspNativeSpriteRenderer_armTransientBatch(
                    ACTION_TRAP_EXPLOSION_LOGICAL,phase,items,(uint8_t)(end-start)) ||
                !EspNativeDynamicLineRender_renderAmbient(
                    EspNativeRootOwners_live()->render,&frame) ||
                frame.finalPresented==0U) goto rollback;
            if (start==0U && phase==0U)
                (void)EspNativeAudioIntent_publish(5061U,0U,4U);
            EspEntranceMusic_pumpGameplay();
#if defined(ESP_PLATFORM)
            while ((uint32_t)(DoomRPG_GetUpTimeMS()-started)<150U) {
                vTaskDelay(1);
                EspEntranceMusic_pumpGameplay();
            }
#endif
        }
        EspNativeSpriteRenderer_clearTransient();
        for (n=start;n<end;++n) {
            uint8_t triggered=0U, playerHits=0U;
            uint16_t damageText=0U;
            uint32_t word=(uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());
            if (!barrelTriggerNeighbors(chain[n],view,
                    EspNativeExplosion_damageFromWord(word),chain,&count,&journal,
                    &triggered,&playerHits,&damageText,
                    EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(expected,n),
                    &lethal)) goto rollback;
            messageDamage=(uint16_t)(messageDamage+damageText);
        }
        start=end;
    }
    if (count!=expected || (expected==1U && journal.monsterHits!=monsters) ||
        (lethal!=0U)!=(journal.deathRandValid!=0U)) goto rollback;
    if (lethal!=0U) {
        uint16_t tile=(uint16_t)(((uint32_t)view->destY>>6U)*MAP_WIDTH+
                                  ((uint32_t)view->destX>>6U));
        if (!EspNativeGameplayPlayerDeath_armWithReservedRand(
                runtime,sequence,tile,journal.deathRand)) goto rollback;
        if (journal.monsterSequenceArmed &&
            !EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(sequence)) {
            DRPG_LOGE("[AI14BARREL] FAILED seq=%u reason=terminal-splash-commit death=armed\n",
                      (unsigned)sequence);
            return 0;
        }
    } else if (journal.monsterSequenceArmed &&
               !EspNativeGameplayMonsterCombat_commitSplashSequence(sequence)) {
        goto rollback;
    }
    if (journal.monsterSequenceArmed && journal.monsterHits!=0U)
        (void)EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(sequence);
    if (lethal!=0U) {
        (void)Esp32PlatformVideo_present();
    } else {
        if (messageDamage!=0U) {
            (void)snprintf(actionState.feedbackText,sizeof(actionState.feedbackText),
                           "%u damage!",(unsigned)messageDamage);
            actionState.feedbackPending=1U;
            actionState.feedbackKind=ACTION_FEEDBACK_DAMAGE;
            actionState.viewportFlashPending=1U;
            actionState.viewportFlashDurationMs=ACTION_TRAP_DAMAGE_FLASH_MS;
            actionState.viewportFlashColor565=FEEDBACK_DAMAGE_RED565;
        }
        (void)EspNativeGameplayFacingLabel_refresh("ENEMY-BARREL");
        memset(&frame,0,sizeof(frame));
        (void)EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                               (uint8_t)view->viewAngle,&frame);
        if (journal.monsterHits!=0U)
            (void)EspNativeGameplayMonsterCombat_finishSplashPresentation(sequence);
    }
    printf("[AI14BARREL] COMMIT seq=%u root=%u chain=%u monsterHits=%u playerDamage=%u lethal=%u ammo=unchanged extraTurn=no rollback=closed\n",
           (unsigned)sequence,(unsigned)rootSprite,(unsigned)count,
           (unsigned)journal.monsterHits,(unsigned)messageDamage,(unsigned)lethal);
    return 1;
rollback:
    EspNativeSpriteRenderer_clearTransient();
    if (!barrelRollbackRemoved(runtime,chain,count) ||
        !EspNativeGameplayPlayerState_restore(&playerBefore)) {
        DRPG_LOGE("[AI14BARREL] FAILED seq=%u rollback=NO\n",(unsigned)sequence);
        return 0;
    }
    *EspNativeProcessRng_live()=rngBefore;
    (void)EspNativeGameplayFacingLabel_refresh("ENEMY-BARREL-ROLLBACK");
    memset(&frame,0,sizeof(frame));
    (void)EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                           (uint8_t)view->viewAngle,&frame);
    printf("[AI14BARREL] ROLLBACK seq=%u root=%u world=yes player=yes rng=yes\n",
           (unsigned)sequence,(unsigned)rootSprite);
    return 0;
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
    if (!EspMapRuntime_getMapSpriteBase(spriteIndex, &sprite)) return 0;
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
        !EspMapRuntime_getMapSpriteBase(target->spriteIndex, &sprite)) {
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
    case ACTION_ROUTE_FIRE_RANGE_MISS: return "FIRE_RANGE_MISS";
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

static const char* directionCueName(uint8_t side) {
    switch ((EspNativeGameplayDamageSide)side) {
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT: return "FRONT";
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT: return "RIGHT";
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK: return "BACK";
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT: return "LEFT";
    default: return "NONE";
    }
}

/* One optional synchronous paint hook into the CURRENT red-flash
 * presentation; never schedules a new frame or changes feedback timing.
 * The impact layer registers at native gameplay-session service entry.
 * Ordinary host harnesses / early boot leave this NULL. */
static void (*impactPainter)(void);
void EspNativeGameplayActionEngine_setImpactPainter(void (*paint)(void)) {
    impactPainter = paint;
}

/* Keep the existing nonlethal text and red flash grouped AFTER the
 * synchronous monster FX tail. No direction-specific presentation occurs. */
void EspNativeGameplayActionEngine_beginMonsterVisualWork(void) {
    directionCue.workActive = 1U;
}

void EspNativeGameplayActionEngine_stageNonlethalMonsterFlash(void) {
    if (directionCue.workActive != 0U &&
        actionState.viewportFlashPending != 0U &&
        actionState.viewportFlashColor565 == FEEDBACK_DAMAGE_RED565)
        directionCue.stageNonlethalBorder = 1U;
}

void EspNativeGameplayActionEngine_endMonsterVisualWork(void) {
    const uint8_t staged = directionCue.stageNonlethalBorder;
    directionCue.workActive = 0U;
    directionCue.stageNonlethalBorder = 0U;
    if (staged != 0U && actionState.viewportFlashPending != 0U) {
        /* The original center impact belongs to THIS first red flash.
         * Intermediate monster-frame presents must not expose it early.
         * Paint onto the existing fresh framebuffer, then let the proven
         * flash compositor paint text/2px red/direction and publish once. */
        if (impactPainter != NULL) impactPainter();
        (void)EspNativeGameplayActionEngine_presentBase();
    }
}

int EspNativeGameplayActionEngine_queueDirectionalDamage(
    uint32_t attackProbe, uint16_t attackerSprite,
    EspNativeGameplayDamageSide side) {
    if (attackProbe == 0U || attackerSprite == 0xffffU ||
        side < ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT ||
        side > ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT ||
        actionState.viewportFlashColor565 != FEEDBACK_DAMAGE_RED565 ||
        (actionState.viewportFlashPending == 0U &&
         actionState.viewportFlashVisible == 0U)) {
        printf("[DAMAGEBORDER] SKIP probe=%u sprite=%u side=%s redLease=absent-or-invalid gameplay=unchanged\n",
               (unsigned int)attackProbe, (unsigned int)attackerSprite,
               directionCueName((uint8_t)side));
        return 0;
    }
    /* Several hits in one turn share ONE border lease. The most recently
     * committed attacker determines the one highlighted side. */
    directionCue.pending.probe = attackProbe;
    directionCue.pending.attackerSprite = attackerSprite;
    directionCue.pending.side = (uint8_t)side;
    directionCue.pendingValid = 1U;
    printf("[DAMAGEBORDER] DIRECTION probe=%u sprite=%u side=%s owner=existing-red-flash timer=shared draw=next-flash-present\n",
           (unsigned int)attackProbe, (unsigned int)attackerSprite,
           directionCueName((uint8_t)side));
    return 1;
}

/* The existing outer 2px red border owns its original 944 pixels. The
 * extra INNER 10px uses a 50%-alpha tint over the saved world texels.
 * This function paints/restores ONLY as part of the SAME border pulse.
 * 0=capture, 1=restore, 2=repaint from original snapshot. */
static int visitDamageBorderInner(uint8_t mode, uint8_t side) {
    uint16_t* fb = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const uint32_t inset = FEEDBACK_BORDER_THICKNESS;
    uint32_t x0 = inset, y0 = FEEDBACK_VIEW_Y + inset;
    uint32_t width = DOOMRPG_LOGICAL_WIDTH - 2U * inset;
    uint32_t height = DAMAGE_BORDER_INNER_THICKNESS;
    uint32_t i = 0U, x, y;
    if (fb == NULL || mode > 2U ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) return 0;
    switch ((EspNativeGameplayDamageSide)side) {
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT: break;
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK:
        y0 = FEEDBACK_VIEW_Y + FEEDBACK_VIEW_HEIGHT -
             DAMAGE_BORDER_DIRECTION_THICKNESS;
        break;
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT:
        width = DAMAGE_BORDER_INNER_THICKNESS;
        height = FEEDBACK_VIEW_HEIGHT - 2U * inset;
        break;
    case ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT:
        x0 = DOOMRPG_LOGICAL_WIDTH - DAMAGE_BORDER_DIRECTION_THICKNESS;
        width = DAMAGE_BORDER_INNER_THICKNESS;
        height = FEEDBACK_VIEW_HEIGHT - 2U * inset;
        break;
    default: return 0;
    }
    if (width * height > DAMAGE_BORDER_INNER_MAX_PIXELS ||
        (mode != 0U && (!directionCue.snapshotValid ||
                        directionCue.snapshotPixels != width * height ||
                        directionCue.activeSide != side))) return 0;
    for (y = y0; y < y0 + height; ++y)
        for (x = x0; x < x0 + width; ++x) {
            uint16_t* pixel = &fb[y * DOOMRPG_LOGICAL_WIDTH + x];
            if (mode == 0U) directionCueSnapshot[i] = *pixel;
            *pixel = mode == 1U
                ? directionCueSnapshot[i]
                : (uint16_t)(((directionCueSnapshot[i] & 0xf7deU) >> 1U) +
                             ((DAMAGE_BORDER_TINT565 & 0xf7deU) >> 1U));
            ++i;
        }
    if (mode == 0U) {
        directionCue.snapshotPixels = (uint16_t)i;
        directionCue.snapshotValid = 1U;
        directionCue.activeSide = side;
    }
    return 1;
}

/* One existing 500ms clock, three pulses for white pickup and red damage.
 * Only red damage owns the extra directional translucent contour. */
static int viewportFeedbackBlinkLit(uint32_t elapsedMs) {
    const uint32_t period = FEEDBACK_DAMAGE_BLINK_ON_MS +
                            FEEDBACK_DAMAGE_BLINK_GAP_MS;
    return elapsedMs < period * FEEDBACK_DAMAGE_BLINK_PULSES &&
           elapsedMs % period < FEEDBACK_DAMAGE_BLINK_ON_MS;
}

static int paintViewportFlash(void) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
        DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    const uint16_t color = actionState.viewportFlashColor565 != 0U
        ? actionState.viewportFlashColor565 : 0xffffU;
    const uint8_t newSide =
        color == FEEDBACK_DAMAGE_RED565
            ? (directionCue.pendingValid != 0U
                ? directionCue.pending.side : directionCue.activeSide)
            : 0U;
    const int keepClean = actionState.viewportFlashSnapshotValid != 0U &&
                          actionState.framebufferFresh == 0U;
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) return 0;
    if (directionCue.snapshotValid != 0U) {
        if (!keepClean) {
            /* World redraw invalidates PIXELS, not the committed attack's
             * directional metadata. Next shared ON recaptures this side. */
            directionCue.snapshotValid = 0U;
        }
        else if (newSide != directionCue.activeSide) {
            if (!visitDamageBorderInner(1U, directionCue.activeSide)) return 0;
            directionCue.snapshotValid = 0U;
            directionCue.activeSide = 0U;
        }
    }
    if (!visitViewportBorder(framebuffer, keepClean ? 2 : 0, color))
        return 0;
    actionState.viewportFlashSnapshotValid = 1U;
    if (newSide != 0U) {
        const uint8_t mode = directionCue.snapshotValid != 0U ? 2U : 0U;
        if (!visitDamageBorderInner(mode, newSide)) return 0;
    }
    if (keepClean) {
        DRPG_LOGT("[VIEWFLASH] REFRESH color565=%04x snapshot=preserved framebufferFresh=0\n",
                  (unsigned int)color);
    }
    return 1;
}

static int restoreViewportFlash(int discardSnapshot) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
        DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (actionState.viewportFlashSnapshotValid == 0U) return 1;
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) return 0;
    if (directionCue.snapshotValid != 0U &&
        !visitDamageBorderInner(1U, directionCue.activeSide)) return 0;
    if (!visitViewportBorder(framebuffer, 1, 0U)) return 0;
    if (discardSnapshot != 0) {
        actionState.viewportFlashSnapshotValid = 0U;
        directionCue.snapshotValid = 0U;
        directionCue.activeSide = 0U;
        directionCue.pendingValid = 0U;
    }
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
    if (EspNativeIndexedBmp_open("k.bmp", &scratch.bmp, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        scratch.bmp.width != 20U || scratch.bmp.height != FEEDBACK_TOP_HEIGHT ||
        EspNativeIndexedBmp_tile(
            &scratch.bmp, framebuffer,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            0, 0, DOOMRPG_LOGICAL_WIDTH, FEEDBACK_TOP_HEIGHT,
            FEEDBACK_OPAQUE, &stats) != ESP_NATIVE_INDEXED_BMP_OK) {
        goto done;
    }

    if (text != NULL) {
        size_t length;
        if (EspNativeIndexedBmp_open("a.bmp", &scratch.bmp, &stats) !=
                ESP_NATIVE_INDEXED_BMP_OK ||
            scratch.bmp.width != FEEDBACK_FONT_SOURCE_WIDTH ||
            scratch.bmp.height != FEEDBACK_FONT_SOURCE_HEIGHT) {
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
            if (!drawGlyph(&scratch.bmp, framebuffer, c,
                           x, FEEDBACK_TEXT_Y, &stats)) {
                goto done;
            }
            x += FEEDBACK_FONT_ADVANCE;
        }

        if (feedback != ACTION_FEEDBACK_NONE) {
            DRPG_LOGT("[ACTIONFEEDBACK] PAINT kind=%u text=\"%s\" chars=%u reads=%u bytes=%u present=caller durationMs=%u\n",
                   (unsigned int)feedback,
                   text,
                   (unsigned int)visible,
                   (unsigned int)stats.packReads,
                   (unsigned int)stats.bytesRead,
                   (unsigned int)FEEDBACK_DISPLAY_MS);
        }
        else {
            DRPG_LOGT("[FACINGLABEL] PAINT name=\"%s\" chars=%u source=%s index=%u priority=fallback reads=%u bytes=%u present=caller\n",
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
        DRPG_LOGT("[ACTIONFEEDBACK] CLEAR mode=topbar-only reads=%u bytes=%u present=caller\n",
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead);
    }
    else {
        DRPG_LOGT("[FACINGLABEL] CLEAR source=none priority=fallback reads=%u bytes=%u present=caller\n",
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

/* A full-screen automap frame is opaque and its 3D/HUD visual cues have no
 * owned pixels there. Drop ONLY ephemeral presentation leases, never pending
 * gameplay actions, removed-bits, RNG, damage commits or monster turn tokens.
 * Do not restore their old pixel snapshots: they were captured on a different
 * screen owner and would write HUD or world pixels onto the automap. */
static void muteAutomapVisuals(void) {
    const int hadVisual =
        actionState.feedbackPending != 0U ||
        actionState.feedbackVisible != 0U ||
        actionState.viewportFlashPending != 0U ||
        actionState.viewportFlashVisible != 0U ||
        actionState.viewportFlashSnapshotValid != 0U ||
        actionState.feedbackFollowupCount != 0U ||
        directionCue.pendingValid != 0U ||
        directionCue.snapshotValid != 0U ||
        directionCue.workActive != 0U ||
        directionCue.stageNonlethalBorder != 0U;
    if (!hadVisual) return;
    actionState.feedbackPending = 0U;
    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
    actionState.feedbackVisible = 0U;
    actionState.feedbackVisibleKind = ACTION_FEEDBACK_NONE;
    actionState.feedbackShownAtMs = 0U;
    actionState.feedbackText[0] = '\0';
    clearFeedbackFollowups();
    actionState.viewportFlashPending = 0U;
    actionState.viewportFlashVisible = 0U;
    actionState.viewportFlashSnapshotValid = 0U;
    actionState.viewportFlashLit = 0U;
    actionState.viewportFlashShownAtMs = 0U;
    actionState.viewportFlashDurationMs = 0U;
    actionState.viewportFlashColor565 = 0U;
    actionState.framebufferFresh = 0U;
    memset(&directionCue, 0, sizeof(directionCue));
    printf("[AUTOMAPFX] SUPPRESS owner=automap feedback=hidden border=hidden cue=discarded gameplay=unchanged framebufferMutation=no\n");
}

int EspNativeGameplayActionEngine_presentBase(void) {
    uint8_t feedback = ACTION_FEEDBACK_NONE;
    int hadFeedback = 0;
    int flashPainted = 0;
    int ok;
    uint32_t physicalPresentedAtMs;

    /*
     * The gameplay compositor never owns MENU_MAIN, SAVESLOTS, INTRO or the
     * transition presentation. In particular, resetting the facing-label
     * owner on SYS EXIT leaves facing.dirty=1, and the first SAVESLOTS paint
     * previously re-tiled the gray gameplay top bar over its opaque frame.
     * The resident owner grants a synchronous lease for the initial WORLD
     * present before touch/turn activation: its first facing label must be
     * composed too. Otherwise pass non-world frames straight through without
     * touching HUD feedback or consuming facing.dirty.
     */
    if (!EspNativeResidentGameplay_ownsWorldPresent()) {
        /* A standalone weapon help or another opaque modal may appear
         * immediately after pickup COMMIT. Preserve the visual intent and
         * its FIFO for when WORLD resumes, but invalidate screen pixels. */
        EspPickupMini_markFreshFrame();
        return Esp32PlatformVideo_presentBase();
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
    if (EspNativeResidentGameplay_isAutomapActive()) {
        EspPickupMini_markFreshFrame();
        muteAutomapVisuals();
        return Esp32PlatformVideo_presentBase();
    }
    if (EspNativeGameplayControls_isActive()) {
        return Esp32PlatformVideo_presentBase();
    }

    if (actionState.feedbackPending != 0U) {
        /* Keep the nonlethal DAMAGE text, border and directional tint
         * grouped AFTER synchronous monster visual work. They share one
         * first physical present and then ONE flash clock. */
        if (directionCue.stageNonlethalBorder == 0U ||
            actionState.feedbackKind != ACTION_FEEDBACK_DAMAGE) {
            feedback = actionState.feedbackKind;
            if (!paintFeedback(feedback)) {
                printf("[ACTIONFEEDBACK] FAILED kind=%u\n",
                       (unsigned int)feedback);
                return 0;
            }
            hadFeedback = 1;
        }
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
        DRPG_LOGT("[ACTIONFEEDBACK] REFRESH kind=%u lease=preserved freshFrame=yes\n",
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

    /* ONE flash clock and ON bit. World redraws while the shared flash is
     * OFF must stay clean; never paint a frame that the clock says is OFF.
     * A fresh world render invalidates old snapshots in that case. */
    if ((actionState.viewportFlashPending != 0U ||
         actionState.viewportFlashVisible != 0U) &&
        directionCue.stageNonlethalBorder == 0U) {
        const int shouldPaint =
            actionState.viewportFlashPending != 0U ||
            (actionState.viewportFlashVisible != 0U &&
             viewportFeedbackBlinkLit(
                 actionNowMs() - actionState.viewportFlashShownAtMs));
        if (shouldPaint &&
            (actionState.viewportFlashPending != 0U ||
             actionState.framebufferFresh != 0U ||
             (directionCue.pendingValid != 0U &&
              actionState.viewportFlashLit != 0U))) {
            if (!paintViewportFlash()) {
                printf("[VIEWFLASH] FAILED phase=paint\n");
                return 0;
            }
            flashPainted = 1;
        }
        else if (!shouldPaint && actionState.framebufferFresh != 0U) {
            /* Clean world framebuffer supplied by renderer during a GAP. */
            actionState.viewportFlashSnapshotValid = 0U;
            directionCue.snapshotValid = 0U;
            /* Keep activeSide across this clean-frame GAP; recapture its
             * translucent pixels from the new camera pose on next ON. */
            actionState.viewportFlashLit = 0U;
        }
    }

    /* Icon is painted after the 2px damage border and BEFORE physical
     * present. Only presentation owns its reversible 20x20 underlay. */
    EspPickupMini_beforePresent(actionNowMs());
    ok = Esp32PlatformVideo_presentBase();
    if (!ok) return 0;
    /* One immutable timestamp for the single successful LCD present:
     * border + direction + face + source impact all share this anchor. */
    physicalPresentedAtMs = actionNowMs();
    EspPickupMini_afterPresent(physicalPresentedAtMs);
    /* Start the source 500ms pain face / weapon / shake lease ONLY after
     * a successful PHYSICAL LCD present. Monster-turn visual work defers
     * the already-proven damage flash until endMonsterVisualWork(); its
     * synchronous movement tail can exceed 500ms. The earlier staged frame
     * must not silently expire under that barrier. The same barrier keeps
     * us from counting invisible intermediate world frames. */
    if (directionCue.stageNonlethalBorder == 0U) {
        (void)EspNativePlayerPainVisual_firstPresented(
            physicalPresentedAtMs);
    }
    if (flashPainted) {
        actionState.viewportFlashLit = 1U;
        if (directionCue.pendingValid != 0U &&
            actionState.viewportFlashColor565 == FEEDBACK_DAMAGE_RED565) {
            printf("[DAMAGEBORDER] PAINT probe=%u sprite=%u side=%s thickness=%u contour565=%04x innerTint565=%04x innerAlpha=50pct blink=shared-500ms additionalPresent=no\n",
                   (unsigned int)directionCue.pending.probe,
                   (unsigned int)directionCue.pending.attackerSprite,
                   directionCueName(directionCue.pending.side),
                   (unsigned int)DAMAGE_BORDER_DIRECTION_THICKNESS,
                   (unsigned int)FEEDBACK_DAMAGE_RED565,
                   (unsigned int)DAMAGE_BORDER_TINT565);
        }
        directionCue.pendingValid = 0U;
    }

    if (flashPainted && actionState.viewportFlashPending != 0U) {
        actionState.viewportFlashPending = 0U;
        actionState.viewportFlashVisible = 1U;
        actionState.viewportFlashShownAtMs = physicalPresentedAtMs;
        printf("[VIEWFLASH] PAINT color565=%04x viewport=0,%u,%u,%u thickness=%u pixels=%u durationMs=%u snapshot=bounded present=caller feedback=%u blink=%s\n",
               (unsigned int)actionState.viewportFlashColor565,
               (unsigned int)FEEDBACK_VIEW_Y,
               (unsigned int)DOOMRPG_LOGICAL_WIDTH,
               (unsigned int)FEEDBACK_VIEW_HEIGHT,
               (unsigned int)FEEDBACK_BORDER_THICKNESS,
               (unsigned int)FEEDBACK_BORDER_PIXELS,
               (unsigned int)actionState.viewportFlashDurationMs,
               (unsigned int)feedback,
               "3x105ms-on/65ms-off");
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

/* The existing viewport flash's ONE service drives both the opaque 2px
 * contour and its translucent 10px directional extension. Transition
 * 105ms ON / 65ms OFF / 105ms ON / 65ms OFF / 105ms ON, then clean until
 * original 500ms expiry. NO independent side cadence or physical presents.
 * Missed transitions are skipped if a world render stalls: no catch-up burst. */
static int serviceViewportFlashExpiry(void) {
    uint32_t elapsed, now;
    uint16_t duration;
    int nextLit;
    if (actionState.viewportFlashVisible == 0U ||
        actionState.viewportFlashPending != 0U) return 1;
    if (EspNativeGameplayLevelUp_isActive() ||
        EspNativeGameplayControls_isActive()) return 1;

    now = actionNowMs();
    elapsed = now - actionState.viewportFlashShownAtMs;
    duration = actionState.viewportFlashDurationMs;
    if (duration == 0U || elapsed >= duration) {
        if (actionState.viewportFlashLit != 0U) {
            if (!restoreViewportFlash(0)) {
                printf("[VIEWFLASH] FAILED phase=expiry-restore\n");
                return 0;
            }
            if (!Esp32PlatformVideo_presentBase()) {
                /* Physical screen still shows ON: restore the framebuffer
                 * to that same state before retrying the expiry. */
                if (!paintViewportFlash()) return 0;
                return 0;
            }
            actionState.viewportFlashLit = 0U;
        }
        actionState.viewportFlashSnapshotValid = 0U;
        actionState.viewportFlashVisible = 0U;
        actionState.viewportFlashLit = 0U;
        actionState.viewportFlashShownAtMs = 0U;
        actionState.viewportFlashDurationMs = 0U;
        directionCue.snapshotValid = 0U;
        directionCue.activeSide = 0U;
        directionCue.pendingValid = 0U;
        printf("[VIEWFLASH] EXPIRE elapsedMs=%u targetMs=%u color565=%04x restored=one-border-including-direction blink=%s\n",
               (unsigned int)elapsed, (unsigned int)duration,
               (unsigned int)actionState.viewportFlashColor565,
               "shared");
        actionState.viewportFlashColor565 = 0U;
        return 1;
    }

    /* Purely visual: white and red pulse on the same original clock;
     * no impact on resource commits, world inputs or player turns. */
    nextLit = viewportFeedbackBlinkLit(elapsed);
    if (nextLit == (actionState.viewportFlashLit != 0U)) return 1;

    if (nextLit) {
        if (!paintViewportFlash()) {
            printf("[VIEWFLASH] FAILED phase=blink-paint\n");
            return 0;
        }
        if (!Esp32PlatformVideo_presentBase()) {
            /* Screen was OFF, keep it OFF when present fails. */
            if (!restoreViewportFlash(0)) return 0;
            return 0;
        }
        actionState.viewportFlashLit = 1U;
    }
    else {
        if (!restoreViewportFlash(0)) {
            printf("[VIEWFLASH] FAILED phase=blink-restore\n");
            return 0;
        }
        if (!Esp32PlatformVideo_presentBase()) {
            if (!paintViewportFlash()) return 0;
            return 0;
        }
        actionState.viewportFlashLit = 0U;
    }
    DRPG_LOGT("[VIEWFLASH] BLINK elapsedMs=%u lit=%u color565=%04x shared=border+direction\n",
              (unsigned int)elapsed, (unsigned int)nextLit,
              (unsigned int)actionState.viewportFlashColor565);
    return 1;
}

static int serviceFeedbackExpiry(void) {
    uint32_t now;
    uint32_t elapsed;
    uint8_t kind;

    if (!serviceViewportFlashExpiry()) return 0;
    if (EspNativeGameplayLevelUp_isActive()) return 1;
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
    if (actionState.feedbackFollowupCount != 0U &&
        actionState.feedbackFollowupKind == kind &&
        actionState.feedbackFollowupHead < FEEDBACK_FOLLOWUP_MAX) {
        const uint8_t queueIndex = actionState.feedbackFollowupHead;
        memcpy(actionState.feedbackText,
               actionState.feedbackFollowupText[queueIndex],
               sizeof(actionState.feedbackText));
        memset(actionState.feedbackFollowupText[queueIndex], 0,
               sizeof(actionState.feedbackFollowupText[queueIndex]));
        ++actionState.feedbackFollowupHead;
        --actionState.feedbackFollowupCount;
        actionState.feedbackPending = 1U;
        actionState.feedbackKind = kind;
        if (!EspNativeGameplayActionEngine_presentBase()) return 0;
        printf("[ACTIONFEEDBACK] ADVANCE kind=%u text=\"%s\" remaining=%u elapsedMs=%u targetMs=%u source=legacy-pickup-fifo\n",
               (unsigned int)kind,
               actionState.feedbackText,
               (unsigned int)actionState.feedbackFollowupCount,
               (unsigned int)elapsed,
               (unsigned int)FEEDBACK_DISPLAY_MS);
        if (actionState.feedbackFollowupCount == 0U) {
            actionState.feedbackFollowupHead = 0U;
            actionState.feedbackFollowupKind = ACTION_FEEDBACK_NONE;
        }
        return 1;
    }

    clearFeedbackFollowups();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_NONE;
    if (!EspNativeGameplayActionEngine_presentBase()) return 0;
    printf("[ACTIONFEEDBACK] EXPIRE kind=%u elapsedMs=%u targetMs=%u restored=topbar-only\n",
           (unsigned int)kind,
           (unsigned int)elapsed,
           (unsigned int)FEEDBACK_DISPLAY_MS);
    return 1;
}

EspNativeGameplayActionStatus EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status =
        EspNativeGameplayAction_executeSelect(intent, outResult);
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

    /* CombatEntity_calcHit() returns MISS without a hit RNG draw when
     * weapon 1's rangeMin=0 and worldDist>4096. Combat_playerSeq() still
     * unconditionally calls CombatEntity_calcDamage() for a fire, consuming
     * ONE damage byte even on a miss; Player_fireWeapon() consumes one ammo
     * and DoomCanvas advances the monster turn after the combat animation.
     * Keep the world immutable, but commit those real consequences below. */
    if (target.type == ACTION_ENTITY_FIRE &&
        weapon == ACTION_WEAPON_EXTINGUISHER && target.distance > 1U) {
        ++actionState.selects;
        if (target.spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
            removed(target.spriteIndex) ||
            !EspNativeGameplayPlayerState_ensure()) {
            ++actionState.combatDeferred;
            printf("[FIREMISS] DEFER seq=%u reason=target-or-player-not-ready mutation=no rngConsumed=0\n",
                   (unsigned int)intent->sequence);
            return status;
        }
        if (EspNativeGameplayPlayerState_ammo(
                ACTION_EXTINGUISHER_AMMO_TYPE) <
                ACTION_EXTINGUISHER_AMMO_USAGE) {
            printf("[FIREMISS] NOAMMO seq=%u sprite=%u need=%u have=%u turnAdvance=no rngConsumed=0 mutation=no\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)ACTION_EXTINGUISHER_AMMO_USAGE,
                   (unsigned int)EspNativeGameplayPlayerState_ammo(
                       ACTION_EXTINGUISHER_AMMO_TYPE));
            return status;
        }
        memset(&actionState.pending, 0, sizeof(actionState.pending));
        actionState.pending.sequence = intent->sequence;
        actionState.pending.spriteIndex = target.spriteIndex;
        actionState.pending.lineIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
        actionState.pending.tileIndex = target.tileIndex;
        actionState.pending.route = ACTION_ROUTE_FIRE_RANGE_MISS;
        actionState.pending.feedback = ACTION_FEEDBACK_NO_EFFECT;
        actionState.pending.type = target.type;
        actionState.pending.subtype = target.subtype;
        actionState.pending.weapon = weapon;
        actionState.pending.distance = target.distance;
        /* No world deletion; request the existing attack render transaction. */
        actionState.pending.worldChanged = 1U;
        actionState.pending.active = 1U;
        printf("[FIREMISS] ARM seq=%u sprite=%u distance=%u weapon=1 result=guaranteed-range-miss ammo=1-pending damageRng=1-pending hitRng=0 worldMutation=no turnAdvance=PLAYER_ATTACK-pending\n",
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
        actionState.deferredMonsterSequence = intent->sequence;
        actionState.deferredMonsterSprite = target.spriteIndex;
        actionState.deferredMonsterTile = target.tileIndex;
        actionState.deferredMonsterSubtype = target.subtype;
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

int EspNativeGameplayActionEngine_deferredMonsterTarget(
    uint32_t sequence, uint16_t* outSprite, uint16_t* outTile,
    uint8_t* outSubtype) {
    if (outSprite == NULL || outTile == NULL || outSubtype == NULL ||
        actionState.ready != 1U || sequence == 0U ||
        actionState.deferredMonsterSequence != sequence) return 0;
    *outSprite = actionState.deferredMonsterSprite;
    *outTile = actionState.deferredMonsterTile;
    *outSubtype = actionState.deferredMonsterSubtype;
    return 1;
}

void EspNativeGameplayActionEngine_reset(void) {
    memset(&directionCue, 0, sizeof(directionCue));
    EspNativeGameplayWeapon_cancelAttack();
    EspNativeSpriteRenderer_clearTransient();
    EspNativeGameplayCrateState_reset();
    memset(&actionState, 0, sizeof(actionState));
    EspPickupMini_reset();
}

static void logActionFrame(const ActionPending* pending,
                           const char* phase,
                           const EspNativeGameplayFrameStats* frame) {
    if (pending == NULL || phase == NULL || frame == NULL) return;
    DRPG_LOGT("[ACTIONENGINE] FRAME seq=%u route=%s phase=%s frame=%08x worldUs=%u spriteUs=%u hudUs=%u presentUs=%u totalUs=%u sprites=%u pixels=%u spriteReads=%u hudReads=%u presented=%u\n",
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

    if (EspNativeResidentGameplay_isAutomapActive()) {
        EspPickupMini_markFreshFrame();
        muteAutomapVisuals();
    }
    if (!serviceFeedbackExpiry()) return 0;
    /* The miniature never gates input or the resource transaction. At most
     * one LCD present per elapsed ON/OFF transition, with a cached icon. */
    if (EspPickupMini_due(actionNowMs()) &&
        !EspNativeGameplayControls_isActive() &&
        !EspNativeResidentGameplay_isAutomapActive() &&
        !EspAssetPack_isOpen() &&
        EspNativeResidentGameplay_ownsWorldPresent()) {
        if (!EspNativeGameplayActionEngine_presentBase()) return 0;
    }
    /* PASS TURN and future non-SELECT actions may queue transient feedback
     * without owning an ActionPending transaction. If no gameplay redraw has
     * consumed it yet, present the existing framebuffer with the queued topbar.
     * A native dialog may currently own the PAK, in which case the feedback
     * painter must simply wait for that bounded owner to release it. */
    if (actionState.pending.active == 0U &&
        (actionState.feedbackPending != 0U ||
         (directionCue.pendingValid != 0U &&
          actionState.viewportFlashVisible != 0U &&
          actionState.viewportFlashLit != 0U &&
          directionCue.workActive == 0U))) {
        if (EspAssetPack_isOpen()) return 1;
        if (!EspNativeGameplayActionEngine_presentBase()) return 0;
    }
    if (actionState.pending.active == 0U) return 1;
    if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL || view == NULL ||
        view->active != 1U || !ensureOwner()) {
        return 0;
    }
    pending = actionState.pending;

    if (pending.worldChanged == 0U) {
        actionState.feedbackPending = 1U;
        actionState.feedbackKind = pending.feedback;
        if (!EspNativeGameplayActionEngine_presentBase()) return 0;
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
        int isFireMiss = pending.route == ACTION_ROUTE_FIRE_RANGE_MISS;
        int isFire = pending.route == ACTION_ROUTE_FIRE_CLEARED ||
                     isFireMiss;
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
        uint8_t simpleTurnRequested = 0U;
        uint8_t barrelExplosionArmed = 0U;
        uint8_t barrelExplosionFrame = 0U;
        uint8_t barrelChainCount = 0U;
        uint8_t barrelExpectedChainCount = 0U;
        uint8_t barrelPreflightMonsters = 0U;
        uint8_t barrelPreflightPlayers = 0U;
        uint8_t barrelInterleaved = 0U;
        uint8_t barrelPreviewWords = 0U;
        ExplosionImpactJournal barrelImpact = {0};
        uint8_t barrelBlastRngCalls = 0U;
        uint8_t barrelBlastPreview[ACTION_BARREL_CHAIN_MAX];
        Random_t barrelRandomAfterCombat;
        Random_t barrelRandomAfterPreview;
        uint8_t barrelPlayerHits = 0U;
        uint8_t barrelPlayerLethal = 0U;
        uint16_t barrelPlayerHitSources[ACTION_BARREL_CHAIN_MAX];
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
        uint8_t blastRootPending = 0U;
        uint8_t rootBlastDamage = 0U;

        memset(&frame, 0, sizeof(frame));
        memset(&playerBefore, 0, sizeof(playerBefore));
        memset(&crateRoll, 0, sizeof(crateRoll));
        memset(barrelChain, 0xff, sizeof(barrelChain));
        memset(&randomBefore, 0, sizeof(randomBefore));
        if (animateWeapon &&
            !EspNativeGameplayWeapon_armAttack(pending.weapon)) {
            if (isFire && !isFireMiss) setRemoved(pending.spriteIndex, 0);
            memset(&actionState.pending, 0, sizeof(actionState.pending));
            printf("[ACTIONENGINE] FAILED seq=%u reason=weapon-attack-arm weapon=%u rollback=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.weapon);
            return 0;
        }

        if (isFire) {
            if (!EspNativeGameplayPlayerState_ensure() ||
                !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
                if (!isFireMiss) setRemoved(pending.spriteIndex, 0);
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
                if (!isFireMiss) setRemoved(pending.spriteIndex, 0);
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
            randomBefore = (*EspNativeProcessRng_live());
            randomCaptured = 1U;
            randHit = DoomRPG_randNextByte(EspNativeProcessRng_live());
            if ((uint32_t)randHit >= ACTION_JAMMED_DOOR_CALC_HIT) {
                (*EspNativeProcessRng_live()) = randomBefore;
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
                (*EspNativeProcessRng_live()) = randomBefore;
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
                !EspMapRuntime_getMapSpriteBase(
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
            randomBefore = (*EspNativeProcessRng_live());
            playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
            ammoBefore = EspNativeGameplayPlayerState_ammo(crateWeapon->ammoType);
            ammoAfter = ammoBefore;
            if (crateWeapon->ammoUsage != 0U &&
                !EspNativeGameplayPlayerState_consumeAmmo(
                    crateWeapon->ammoType, crateWeapon->ammoUsage,
                    &ammoBefore, &ammoAfter)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                (*EspNativeProcessRng_live()) = randomBefore;
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
                (*EspNativeProcessRng_live()) = randomBefore;
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
                blastRootPending = 1U;
            }
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
            randomBefore = (*EspNativeProcessRng_live());
            playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
            ammoBefore = EspNativeGameplayPlayerState_ammo(crateWeapon->ammoType);
            ammoAfter = ammoBefore;
            if (crateWeapon->ammoUsage != 0U &&
                !EspNativeGameplayPlayerState_consumeAmmo(
                    crateWeapon->ammoType, crateWeapon->ammoUsage,
                    &ammoBefore, &ammoAfter)) {
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                (*EspNativeProcessRng_live()) = randomBefore;
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
                (*EspNativeProcessRng_live()) = randomBefore;
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
                crateRandFirst = DoomRPG_randNextByte(EspNativeProcessRng_live());
                if (EspNativeGameplayCrateState_requiresSecondRng(
                        crateRandFirst)) {
                    crateRandSecond = DoomRPG_randNextByte(EspNativeProcessRng_live());
                    crateRandSecondValid = 1U;
                }
                if (!EspNativeGameplayCrateState_resolveOutcome(
                        crateRandFirst, crateRandSecond,
                        crateRandSecondValid, &crateOutcome,
                        &crateEffectiveDefTile)) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
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
                    blastRootPending = 1U;
                }

                if (crateOutcome == ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM) {
                    EspNativeGraphicsCatalogStatus graphicsStatus =
                        EspNativeGraphicsCatalog_ensureSprite(
                            crateEffectiveDefTile);
                    if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                        graphicsStatus !=
                            ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                        (*EspNativeProcessRng_live()) = randomBefore;
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
                        (*EspNativeProcessRng_live()) = randomBefore;
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
                else if (blastRootPending != 0U) {
                    pending.feedback = ACTION_FEEDBACK_TRAPPED;
                }
                else {
                    setRemoved(pending.spriteIndex, 1);
                    crateRemoved = 1U;
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

            if (blastRootPending == 0U &&
                !EspNativeGameplayMonsterTurn_requestPlayerAttack(
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
                (*EspNativeProcessRng_live()) = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[CRATE] FAILED seq=%u sprite=%u reason=monster-turn-request-busy rollback=%s player=yes rng=yes world=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
            if (blastRootPending == 0U) crateTurnRequested = 1U;
        }

        if (blastRootPending != 0U) {
                ActionTarget unsupported;
                uint8_t preflightCount = 0U;
                EspNativeGraphicsCatalogStatus graphicsStatus;
                EspMapSprite blastRoot;
                uint8_t previewOrdinal;
                int previewExact;

                /* Entity_died allocates the SAME type-1 gsprite for a barrel
                 * and for a trapped crate. No origin-specific radius rules. */
                memset(&unsupported, 0, sizeof(unsupported));
                if (!EspMapRuntime_getMapSpriteBase(pending.spriteIndex, &blastRoot) ||
                    !barrelChainPreflight(pending.spriteIndex, pending.subtype, view,
                                          barrelChain, &preflightCount,
                                          &barrelPreflightMonsters,
                                          &barrelPreflightPlayers,
                                          &unsupported)) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
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
                barrelInterleaved = (uint8_t)(barrelPreflightMonsters != 0U);
                barrelImpact.runtime = doomRpg;
                barrelImpact.sequence = pending.sequence;
                barrelPreviewWords =
                    EspNativeGameplayBarrelRadius_previewWordsForChain(
                        barrelExpectedChainCount, barrelPreflightMonsters,
                        barrelPreflightPlayers);
                if (barrelExpectedChainCount == 0U ||
                    barrelExpectedChainCount > ACTION_BARREL_CHAIN_MAX ||
                    EspNativeProcessRng_live()->nextRand < 0 ||
                    EspNativeProcessRng_live()->nextRand >= RANDTABLESIZE) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=invalid-rng-or-chain nextRand=%d explosions=%u mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           EspNativeProcessRng_live()->nextRand,
                           (unsigned int)barrelExpectedChainCount);
                    return 1;
                }

                graphicsStatus = EspNativeGraphicsCatalog_ensureSprite(
                    ACTION_TRAP_EXPLOSION_LOGICAL);
                if (graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                    graphicsStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARREL] DEFER seq=%u sprite=%u reason=explosion-resource logical=%u catalogStatus=%u playerRollback=yes rngRollback=yes mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex,
                           (unsigned int)ACTION_TRAP_EXPLOSION_LOGICAL,
                           (unsigned int)graphicsStatus);
                    return 1;
                }

                /*
                 * Preview consecutive words only when neither a monster
                 * consequence nor a player death can interleave draws.
                 * Otherwise preview only the first word; subsequent words
                 * are drawn LIVE after preceding radius cells. Use the
                 * actual RNG pointer for exact hidden-refill reservations,
                 * and restore its Random_t bytes after this probe.
                 */
                barrelRandomAfterCombat = (*EspNativeProcessRng_live());
                if (!EspNativeRngReplayGuard_beginAttackProbe(
                        EspNativeProcessRng_live())) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=rng-probe-owner-busy rollback=player+rng mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex);
                    return 1;
                }
                for (previewOrdinal = 0U;
                     previewOrdinal < barrelPreviewWords;
                     ++previewOrdinal) {
                    barrelBlastPreview[previewOrdinal] = (uint8_t)
                        DoomRPG_randNextInt(EspNativeProcessRng_live());
                }
                barrelRandomAfterPreview = (*EspNativeProcessRng_live());
                (*EspNativeProcessRng_live()) = barrelRandomAfterCombat;
                previewExact = EspNativeRngReplayGuard_endAttackProbe(
                    EspNativeProcessRng_live(), &barrelRandomAfterCombat);
                if (!previewExact) {
                    DRPG_LOGE("[RNGGUARD] BARREL-PREVIEW-FAILED seq=%u sprite=%u chain=%u stateExact=NO action=fail-closed\n",
                              (unsigned int)pending.sequence,
                              (unsigned int)pending.spriteIndex,
                              (unsigned int)barrelExpectedChainCount);
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=rng-preview-not-exact rollback=player+rng mutation=no\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex);
                    return 1;
                }
                printf("[BARRELRADIUS] RNG-PREFLIGHT seq=%u sprite=%u next=%d->%d words=%u plannedBlasts=%u mode=%s refill=%s exact=yes rollback=closed\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       barrelRandomAfterCombat.nextRand,
                       barrelRandomAfterPreview.nextRand,
                       (unsigned int)barrelPreviewWords,
                       (unsigned int)barrelExpectedChainCount,
                       barrelInterleaved != 0U
                           ? "first-word-only/monster-consequence-live"
                           : barrelPreviewWords < barrelExpectedChainCount
                               ? "first-word-only/player-death-live"
                               : "full-consecutive",
                       barrelRandomAfterCombat.nextRand +
                           (int)barrelPreviewWords * 4 >= RANDTABLESIZE
                           ? "reserved" : "none");
                /* Keep the original snapshot even for one blast with monsters:
                 * terminal player admission can still fail after splash. */
                if (barrelPreflightMonsters != 0U &&
                    !EspNativeGameplayMonsterCombat_beginSplashSequence(
                        doomRpg, pending.sequence)) {
                    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                    (*EspNativeProcessRng_live()) = randomBefore;
                    EspNativeGameplayWeapon_cancelAttack();
                    memset(&actionState.pending, 0, sizeof(actionState.pending));
                    printf("[BARRELRADIUS] DEFER seq=%u sprite=%u reason=monster-sequence-snapshot-not-ready mutation=no rollback=player+rng\n",
                           (unsigned int)pending.sequence,
                           (unsigned int)pending.spriteIndex);
                    return 1;
                }

                barrelImpact.monsterSequenceArmed = barrelInterleaved;

                /* The dry-run array proved capacity/support only. Runtime must
                 * rediscover neighbors at each 450 ms expiry so removals happen
                 * in the same causal order as Game_gsprite_update(). */
                memset(barrelChain, 0xff, sizeof(barrelChain));
                barrelChain[0] = pending.spriteIndex;
                barrelChainCount = 1U;
                setRemoved(pending.spriteIndex, 1);
                barrelRemoved = 1U;
                if (isCrate) crateRemoved = 1U;
                barrelWorldX = blastRoot.x;
                barrelWorldY = blastRoot.y;
                barrelExplosionArmed = 1U;
                printf("[BARRELRADIUS] PREFLIGHT seq=%u root=%u chain=%u radius=4-cardinal-full+4-diagonal-half targetFamily=barrel+player+monster unsupported=fail-closed rngWords=%u visual=wave-batch-concurrent gameplayOrder=%s\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       (unsigned int)barrelExpectedChainCount,
                       (unsigned int)barrelPreviewWords,
                       barrelInterleaved != 0U ? "legacy-blast-impact-interleaved"
                                               : "legacy-causal");
            if (isBarrel) {
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
            } else {
                printf("[CRATETRAP] SHARED seq=%u root=%u chain=%u owner=type1-blast-chain\n",
                       (unsigned int)pending.sequence, (unsigned int)pending.spriteIndex,
                       (unsigned int)barrelExpectedChainCount);
            }
        }

        if (isBarrel || barrelExplosionArmed != 0U) {
            if (!EspNativeGameplayMonsterTurn_requestPlayerAttack(
                    pending.sequence)) {
                int rollbackOk = 1;
                if (barrelRemoved != 0U && !barrelRollbackRemoved(
                        doomRpg, barrelChain, barrelChainCount)) rollbackOk = 0;
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) rollbackOk = 0;
                (*EspNativeProcessRng_live()) = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARREL] FAILED seq=%u sprite=%u reason=monster-turn-request-busy rollback=%s player=yes rng=yes world=yes\n",
                       (unsigned int)pending.sequence, (unsigned int)pending.spriteIndex,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
            barrelTurnRequested = 1U;
        }

        /*
         * Legacy Player_fireWeapon() routes every successful player-owned
         * attack through ST_COMBAT. DoomCanvas_combatState() calls
         * Game_advanceTurn() when that combat completes (curAttacker == NULL),
         * including extinguisher fire clears and jammed-door destruction.
         * Barrel/crate routes already publish the same semantic turn above.
         */
        if (isFire) {
            if (!EspNativeGameplayMonsterTurn_requestPlayerAttack(
                    pending.sequence)) {
                int rollbackOk = 1;
                if (!isFireMiss) setRemoved(pending.spriteIndex, 0);
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[ACTIONENGINE] FAILED seq=%u route=%s reason=monster-turn-request-busy rollback=%s player=yes world=%s\n",
                       (unsigned int)pending.sequence,
                       routeName((ActionRoute)pending.route),
                       rollbackOk ? "yes" : "NO",
                       isFireMiss ? "unchanged" : "restored");
                return rollbackOk ? 1 : 0;
            }
            simpleTurnRequested = 1U;
        }

        if (pending.feedback != ACTION_FEEDBACK_NONE) {
            actionState.feedbackPending = 1U;
            actionState.feedbackKind = pending.feedback;
        }
        if (!EspNativeGameplayFacingLabel_refresh("ACTION-WORLD-COMMIT")) {
            printf("[FACINGLABEL] DEFER reason=ACTION-WORLD-COMMIT worldRender=continue\n");
        }
        if (!EspNativeGameplayFrame_renderTurn(
                EspNativeRootOwners_live()->render, (uint8_t)view->viewAngle, &frame)) {
            int rollbackOk = 1;
            EspNativeGameplayWeapon_cancelAttack();
            actionState.feedbackPending = 0U;
            if (simpleTurnRequested != 0U &&
                !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                    pending.sequence)) {
                rollbackOk = 0;
            }
            actionState.feedbackKind = ACTION_FEEDBACK_NONE;
            if (isFire) {
                if (!isFireMiss) setRemoved(pending.spriteIndex, 0);
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
            }
            else if (isJammedDoor && destructibleMutated != 0U) {
                rollbackOk = EspNativeGameplayDestructible_rollbackLineDeath(
                    &actionState.destructibleUndo);
                if (randomCaptured != 0U) (*EspNativeProcessRng_live()) = randomBefore;
                memset(&actionState.destructibleUndo, 0,
                       sizeof(actionState.destructibleUndo));
            }
            else if (isBarrel || barrelExplosionArmed != 0U) {
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (barrelRemoved != 0U &&
                    !barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                if (playerCaptured != 0U &&
                    !EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                if (randomCaptured != 0U) (*EspNativeProcessRng_live()) = randomBefore;
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
                if (randomCaptured != 0U) (*EspNativeProcessRng_live()) = randomBefore;
            }
            memset(&frame, 0, sizeof(frame));
            if (rollbackOk &&
                !EspNativeGameplayFacingLabel_refresh("ACTION-WORLD-ROLLBACK")) {
                printf("[FACINGLABEL] DEFER reason=ACTION-WORLD-ROLLBACK worldRender=continue\n");
            }
            if (!rollbackOk || !EspNativeGameplayFrame_renderTurn(
                    EspNativeRootOwners_live()->render, (uint8_t)view->viewAngle, &frame)) {
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

        /* BARREL-GUN-VISUAL: original Combat_playerSeq sounds the weapon at
         * its firing pose, NOT after all the world/explosion animations.
         * The real attack frame has been presented and PAK is closed. This
         * cue describes that already visible shot even if a later wave
         * needs rollback; a failed/no-present attack never reaches here. */
        if ((isBarrel || barrelExplosionArmed != 0U) && frame.finalPresented != 0U &&
            crateWeapon != NULL && crateWeapon->resourceId != 0U) {
            (void)EspNativeAudioIntent_publish(
                (uint16_t)crateWeapon->resourceId, 0U, 5U);
            EspEntranceMusic_pumpGameplay();
            printf("[BARRELAUDIO] GUN-VISIBLE seq=%u resource=%u priority=5 phase=attack-frame audioPump=immediate\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)crateWeapon->resourceId);
        }

        if (barrelExplosionArmed != 0U) {
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
                    if (!EspMapRuntime_getMapSpriteBase(
                            spriteIndex, &explodingSprite)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        (*EspNativeProcessRng_live()) = randomBefore;
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
                            EspNativeRootOwners_live()->render,
                            (uint8_t)view->viewAngle,
                            &frame)) {
                        int rollbackOk = 1;
                        EspNativeSpriteRenderer_clearTransient();
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        (*EspNativeProcessRng_live()) = randomBefore;
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
                                    EspNativeRootOwners_live()->render,
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
                    /* BARREL-5061-VISUAL: original Game_gsprite_allocAnim
                     * starts the blast sound when its first actual frame
                     * becomes visible, not after 3 frames and radius work.
                     * Exactly one intent per chain, no speculative sound
                     * if the wave frame was not presented successfully. */
                    if (waveOrdinal == 1U &&
                        barrelExplosionFrame == 0U &&
                        barrelRemoved != 0U && barrelExplosionArmed != 0U &&
                        frame.finalPresented != 0U) {
                        (void)EspNativeAudioIntent_publish(5061U, 0U, 4U);
                        EspEntranceMusic_pumpGameplay();
                        printf("[BARRELAUDIO] BLAST-VISIBLE seq=%u resource=5061 priority=4 wave=1 frame=0 audioPump=immediate\n",
                               (unsigned int)pending.sequence);
                    }
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

                    blastByte = (uint8_t)
                        DoomRPG_randNextInt(EspNativeProcessRng_live());
                    if ((blastCursor < barrelPreviewWords &&
                         blastByte != barrelBlastPreview[blastCursor]) ||
                        (blastCursor == 0U && barrelPreviewWords == 1U &&
                         memcmp(EspNativeProcessRng_live(), &barrelRandomAfterPreview,
                                sizeof(barrelRandomAfterPreview)) != 0)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        (*EspNativeProcessRng_live()) = randomBefore;
                        EspNativeGameplayWeapon_cancelAttack();
                        memset(&actionState.pending, 0,
                               sizeof(actionState.pending));
                        DRPG_LOGE("[RNGGUARD] BARREL-PREVIEW-DIVERGED seq=%u root=%u exploding=%u word=%u expected=%u actual=%u rollback=%s sequenceExact=NO\n",
                                  (unsigned int)pending.sequence,
                                  (unsigned int)pending.spriteIndex,
                                  (unsigned int)barrelChain[blastCursor],
                                  (unsigned int)blastCursor,
                                  (unsigned int)barrelBlastPreview[blastCursor],
                                  (unsigned int)blastByte,
                                  rollbackOk ? "yes" : "NO");
                        return rollbackOk ? 1 : 0;
                    }
                    ++barrelBlastRngCalls;
                    blastDamage =
                        EspNativeExplosion_damageFromWord(blastByte);
                    if (blastCursor == 0U) rootBlastDamage = blastDamage;
                    if (!barrelTriggerNeighbors(
                            barrelChain[blastCursor],
                            view,
                            blastDamage,
                            barrelChain,
                            &barrelChainCount,
                            &barrelImpact,
                            &triggered,
                            &playerHitsThisBlast,
                            &playerMessageDamageThisBlast,
                            EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(
                                barrelExpectedChainCount, blastCursor),
                            &barrelPlayerLethal)) {
                        int rollbackOk = 1;
                        if (barrelTurnRequested != 0U &&
                            !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                                pending.sequence)) {
                            rollbackOk = 0;
                        }
                        if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                        if (!EspNativeGameplayPlayerState_restore(
                                &playerBefore)) {
                            rollbackOk = 0;
                        }
                        (*EspNativeProcessRng_live()) = randomBefore;
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
                    if (playerHitsThisBlast != 0U &&
                        barrelPlayerHits < ACTION_BARREL_CHAIN_MAX) {
                        barrelPlayerHitSources[barrelPlayerHits] =
                            barrelChain[blastCursor];
                    }
                    barrelPlayerHits =
                        (uint8_t)(barrelPlayerHits +
                                  playerHitsThisBlast);
                    barrelTotalPlayerMessageDamage =
                        (uint16_t)(
                            barrelTotalPlayerMessageDamage +
                            playerMessageDamageThisBlast);
                    printf("[BARRELRADIUS] BLAST seq=%u root=%u source=%u wave=%u rngByte=%u damage=%u diagonal=%u triggered=%u playerHits=%u playerMessageDamage=%u totalPlayerMessageDamage=%u chainNow=%u/%u sound=5061-deferred shake=200ms-native-transient\n",
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

            /* Trapped! was already presented at the attack frame. */
            if (isCrate) pending.feedback = ACTION_FEEDBACK_NONE;
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
                    if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                    if (!EspNativeGameplayPlayerState_restore(
                            &playerBefore)) {
                        rollbackOk = 0;
                    }
                    (*EspNativeProcessRng_live()) = randomBefore;
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

            /* A full consecutive preview is valid only without interleaved
             * consequences (or one blast). First-word-only paths already
             * checked the complete RNG snapshot before their first impact. */
            if (barrelChainCount != barrelExpectedChainCount ||
                barrelBlastRngCalls != barrelExpectedChainCount ||
                (barrelInterleaved == 0U &&
                 barrelPreviewWords == barrelExpectedChainCount &&
                 memcmp(barrelImpact.deathRandValid != 0U
                            ? &barrelImpact.randomBeforeDeathByte : EspNativeProcessRng_live(),
                        &barrelRandomAfterPreview,
                        sizeof(barrelRandomAfterPreview)) != 0)) {
                int rollbackOk = 1;
                const int rngExact =
                    barrelBlastRngCalls == barrelExpectedChainCount &&
                    (barrelInterleaved != 0U ||
                     barrelPreviewWords < barrelExpectedChainCount ||
                     memcmp(barrelImpact.deathRandValid != 0U
                                ? &barrelImpact.randomBeforeDeathByte
                                : EspNativeProcessRng_live(),
                            &barrelRandomAfterPreview,
                            sizeof(barrelRandomAfterPreview)) == 0);
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) {
                    rollbackOk = 0;
                }
                if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                if (!EspNativeGameplayPlayerState_restore(
                        &playerBefore)) {
                    rollbackOk = 0;
                }
                (*EspNativeProcessRng_live()) = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0,
                       sizeof(actionState.pending));
                if (!rngExact) {
                    DRPG_LOGE("[RNGGUARD] BARREL-PREVIEW-DIVERGED seq=%u root=%u expectedWords=%u actualWords=%u stateExact=NO rollback=%s sequenceExact=NO\n",
                              (unsigned int)pending.sequence,
                              (unsigned int)pending.spriteIndex,
                              (unsigned int)barrelExpectedChainCount,
                              (unsigned int)barrelBlastRngCalls,
                              rollbackOk ? "yes" : "NO");
                }
                printf("[BARRELRADIUS] ROLLBACK seq=%u root=%u reason=%s expected=%u actual=%u rollback=%s rng=yes player=yes world=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       rngExact ? "preflight-runtime-chain-mismatch"
                                : "rng-preview-diverged",
                       (unsigned int)barrelExpectedChainCount,
                       (unsigned int)barrelChainCount,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
            /* Single-blast preflight predicts its exact victim count. */
            if (barrelExpectedChainCount == 1U &&
                barrelImpact.monsterHits != barrelPreflightMonsters) {
                int rollbackOk = 1;
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) rollbackOk = 0;
                if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                if (!EspNativeGameplayPlayerState_restore(&playerBefore)) {
                    rollbackOk = 0;
                }
                (*EspNativeProcessRng_live()) = randomBefore;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                printf("[BARRELRADIUS] ROLLBACK seq=%u reason=monster-splash-count hitCount=%u expected=%u rollback=%s player=yes rng=yes world=yes monster=helper-exact\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)barrelImpact.monsterHits,
                       (unsigned int)barrelPreflightMonsters,
                       rollbackOk ? "yes" : "NO");
                return rollbackOk ? 1 : 0;
            }
            /* Nonterminal splash closes here as before. Lethal player
             * admission must retain the monster/drop/RNG snapshot until
             * PlayerDeath_armWithReservedRand succeeds. */
            if (barrelPreflightMonsters != 0U && barrelPlayerLethal == 0U) {
                if (!EspNativeGameplayMonsterCombat_commitSplashSequence(
                        pending.sequence)) {
                    DRPG_LOGE("[BARRELRADIUS] FAILED seq=%u reason=monster-sequence-commit contract=broken\n",
                              (unsigned int)pending.sequence);
                    return 0;
                }
            }
            if (barrelInterleaved != 0U) {
                printf("[BARRELRADIUS] INTERLEAVED-COMMIT seq=%u blasts=%u monsterHits=%u firstWordPreview=exact laterWords=live-after-monster-consequences rollback=closed\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)barrelBlastRngCalls,
                       (unsigned int)barrelImpact.monsterHits);
            }
        }

        /* Every type-1 origin has already resolved through the shared waves. */
        if ((isBarrel || barrelExplosionArmed != 0U) && barrelPlayerLethal == 0U) {
            printf("[BARREL] COMMIT seq=%u sprite=%u weapon=%u ammoType=%u ammo=%u->%u playerFNV=%08x->%08x loops=%u hits=%u rngCombat=%u rngBlast=%u damage=%ld armorDamage=%ld removed=%u chainRemoved=%u playerRadiusHits=%u explosion=%s sound=%u-attack-visible barrelExplosionSound=5061-first-wave-visible radius=barrel-chain-wave+player-owned/other-hurtable-families-fail-closed turnAdvance=PLAYER_ATTACK-requested rollback=closed\n",
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
            /* Only NOW is the full barrel + multi-wave monster transaction
             * irreversible. The native monster combat owner already consumed
             * the exact legacy death/pain sound RNG; publish its bounded
             * sound intents once, never on a rolled-back blast. The visible
             * 5061 explosion remains anchored to wave-1 frame 0 above. */
            if (barrelImpact.monsterHits != 0U) {
                if (!EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
                        pending.sequence)) {
                    printf("[BARRELRADIUS] SFX-DEFER seq=%u reason=monster-audio-sequence-invalid worldCommitted=yes\n",
                           (unsigned int)pending.sequence);
                }
                EspEntranceMusic_pumpGameplay();
            }
        }

        if (isFireMiss) {
            /* No RNG is spent by range-rejected CombatEntity_calcHit().
             * Non-enemy Combat_playerSeq() nevertheless invokes
             * CombatEntity_calcDamage(), which draws exactly one byte.
             * Read it only AFTER successful redraw and turn preflight so an
             * aborted render has no hidden RNG-refill state to roll back. */
            uint8_t ignoredDamageRoll = DoomRPG_randNextByte(EspNativeProcessRng_live());
            printf("[FIREMISS] COMMIT seq=%u sprite=%u weapon=1 distance=%u ammo=%u->%u playerFNV=%08x->%08x hitRng=0 damageRng=1 damageByte=%u worldMutation=no feedback=\"No effect!\" turnAdvance=PLAYER_ATTACK-requested rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)pending.distance,
                   (unsigned int)ammoBefore,
                   (unsigned int)ammoAfter,
                   (unsigned int)playerFNVBefore,
                   (unsigned int)playerFNVAfter,
                   (unsigned int)ignoredDamageRoll);
        }
        else if (isFire) {
            printf("[ACTIONENGINE] FIRE-COMMIT seq=%u sprite=%u ammoType=%u ammo=%u->%u playerFNV=%08x->%08x xp=2-deferred sound=5045-deferred turnAdvance=PLAYER_ATTACK-requested rollback=closed\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)ACTION_EXTINGUISHER_AMMO_TYPE,
                   (unsigned int)ammoBefore,
                   (unsigned int)ammoAfter,
                   (unsigned int)playerFNVBefore,
                   (unsigned int)playerFNVAfter);
            /* P2: only after a committed fire removal, never rollback. */
            (void)EspNativeAudioIntent_publish(5045U,0U,3U);
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
        if (isCrate && barrelPlayerLethal == 0U) {
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
                   barrelExplosionArmed != 0U ? "shared-type1-waves" : "none",
                   (unsigned int)rootBlastDamage,
                   barrelExplosionArmed != 0U ? "shared-radius" : "n/a");
            (void)effectiveParm;
            /* SFX-P1: publish only a successfully committed pistol/crate
             * hit. No I/O here, and no audio on rollback/invalid outcome. */
            if (barrelExplosionArmed == 0U && crateWeapon != NULL && crateWeapon->resourceId==5046U &&
                crateOutcome != ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID)
                (void)EspNativeAudioIntent_publish(5046U,0U,3U);
        }

        if (animateWeapon) {
            EspNativeGameplayFrameStats settle;
            memset(&settle, 0, sizeof(settle));
            if (pending.feedback != ACTION_FEEDBACK_NONE) {
                actionState.feedbackPending = 1U;
                actionState.feedbackKind = pending.feedback;
            }
            if (EspNativeGameplayFrame_renderTurn(
                    EspNativeRootOwners_live()->render, (uint8_t)view->viewAngle, &settle)) {
                logActionFrame(&pending, "settle-idle", &settle);
                printf("[ACTIONENGINE] ATTACK seq=%u weapon=%u frame=1->0 generic=yes worldCommitted=%s\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.weapon,
                       isFireMiss ||
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
        if (barrelExplosionArmed != 0U && barrelPlayerLethal == 0U &&
            barrelImpact.monsterHits != 0U &&
            !EspNativeGameplayMonsterCombat_finishSplashPresentation(
                pending.sequence)) {
            printf("[BARRELRADIUS] PRESENT-FAILED seq=%u reason=level-up-modal-after-settle worldCommitted=yes recovery=required\n",
                   (unsigned int)pending.sequence);
            memset(&actionState.pending, 0, sizeof(actionState.pending));
            return 0;
        }

        if (barrelExplosionArmed != 0U && barrelPlayerLethal != 0U) {
            /* Whole-chain terminal boundary, optionally with monster victims.
             * Their snapshot, deferred SFX and level-up popup stay provisional
             * until the death owner accepts the one reserved byte. */
            const int validTile =
                view->destX >= 0 && view->destY >= 0 &&
                ((uint32_t)view->destX >> 6) < MAP_WIDTH &&
                ((uint32_t)view->destY >> 6) < MAP_WIDTH;
            const uint16_t deathTile = validTile
                ? (uint16_t)((((uint32_t)view->destY >> 6) * MAP_WIDTH) +
                             ((uint32_t)view->destX >> 6))
                : 0xffffU;
            if (!validTile || barrelImpact.deathRandValid == 0U ||
                !EspNativeGameplayPlayerDeath_armWithReservedRand(
                    doomRpgBase, pending.sequence, deathTile,
                    barrelImpact.deathRand)) {
                int rollbackOk = 1;
                if (barrelTurnRequested != 0U &&
                    !EspNativeGameplayMonsterTurn_cancelPlayerAttack(
                        pending.sequence)) rollbackOk = 0;
                if (!barrelRollbackRemoved(doomRpg, barrelChain,
                                           barrelChainCount)) rollbackOk = 0;
                if (!EspNativeGameplayPlayerState_restore(
                        &playerBefore)) rollbackOk = 0;
                (*EspNativeProcessRng_live()) = randomBefore;
                actionState.feedbackPending = 0U;
                actionState.feedbackKind = ACTION_FEEDBACK_NONE;
                actionState.feedbackText[0] = '\0';
                actionState.viewportFlashPending = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                memset(&frame, 0, sizeof(frame));
                if (rollbackOk &&
                    EspNativeGameplayFacingLabel_refresh(
                        "BARREL-DEATH-ROLLBACK")) {
                    rollbackOk = EspNativeGameplayFrame_renderTurn(
                        EspNativeRootOwners_live()->render, (uint8_t)view->viewAngle, &frame);
                }
                printf("[BARRELRADIUS] LETHAL-ROLLBACK seq=%u root=%u cause=%s rollback=%s rng=yes player=yes world=yes death=not-armed\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex,
                       !validTile ? "invalid-player-tile" :
                           barrelImpact.deathRandValid == 0U
                               ? "missing-death-rng" : "player-death-arm",
                       rollbackOk ? "yes" : "NO");
                memset(&actionState.pending, 0, sizeof(actionState.pending));
                return rollbackOk ? 1 : 0;
            }
            if (barrelPreflightMonsters != 0U) {
                if (!EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(
                        pending.sequence)) {
                    DRPG_LOGE("[BARRELRADIUS] FAILED seq=%u reason=terminal-splash-commit death=armed contract=broken\n",
                              (unsigned int)pending.sequence);
                    return 0;
                }
                if (!EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
                        pending.sequence)) {
                    printf("[BARRELRADIUS] SFX-DEFER seq=%u reason=terminal-audio-sequence-invalid worldCommitted=yes\n",
                           (unsigned int)pending.sequence);
                }
                EspEntranceMusic_pumpGameplay();
            }
            /* Death's HUD and presentation have priority over transient cue
             * animations; it is not safe to keep painting on a fading world. */
            if (!Esp32PlatformVideo_present()) {
                printf("[BARRELRADIUS] LETHAL-PRESENT-DEFER seq=%u death=armed recovery=death-service\n",
                       (unsigned int)pending.sequence);
            }
            printf("[BARRELRADIUS] LETHAL-COMMIT seq=%u root=%u blastCount=%u playerHP=0 tile=%u deathOwner=armed rollback=closed rng=radius-cell-death-once\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)barrelBlastRngCalls,
                   (unsigned int)deathTile);
        }
        else if (barrelExplosionArmed != 0U && barrelPlayerHits != 0U) {
            uint8_t i;
            int32_t stepX = 0;
            int32_t stepY = 0;
            if (EspPlayerView_cardinalBasis(
                    (uint8_t)view->viewAngle, NULL, NULL,
                    &stepX, &stepY, NULL)) {
                for (i = 0U; i < barrelPlayerHits &&
                             i < ACTION_BARREL_CHAIN_MAX; ++i) {
                    EspMapSprite blast;
                    EspNativeGameplayDamageSide side;
                    if (!EspMapRuntime_getMapSpriteBase(
                            barrelPlayerHitSources[i], &blast)) continue;
                    side = EspNativeGameplayDamageSide_fromWorld(
                        view->viewX, view->viewY, stepX, stepY,
                        blast.x, blast.y);
                    if (side != ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE) {
                        /* High bit namespaces barrel action sequences away
                         * from ordinary monster attack-probe identifiers. */
                        (void)EspNativeGameplayActionEngine_queueDirectionalDamage(
                            0x80000000UL | (pending.sequence & 0x7fffffffUL),
                            barrelPlayerHitSources[i], side);
                    }
                }
            }
        }
    }

    memset(&actionState.pending, 0, sizeof(actionState.pending));
    return 1;
}

#if defined(ARDUINO) && defined(DOOMRPG_ESP32)
int EspNativeGameplayActionEngine_moveFrameQuiet(void) {
    return !actionState.feedbackPending && !actionState.feedbackVisible &&
           !actionState.feedbackFollowupCount &&
           !actionState.viewportFlashPending && !actionState.viewportFlashVisible &&
           !actionState.framebufferFresh && !directionCue.pendingValid &&
           !directionCue.stageNonlethalBorder &&
           !EspNativeGameplayFacingLabel_isDirty();
}
#endif
