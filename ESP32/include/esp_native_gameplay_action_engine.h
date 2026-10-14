#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H

#include "esp_native_gameplay_action.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_ACTION_REMOVED_SNAPSHOT_MAX_BYTES 128U
#define ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_QUEUE_MAX 5U
#define ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_TEXT_BYTES 24U

typedef struct EspNativeGameplayActionRemovedSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint16_t spriteCount;
    uint16_t removedBytes;
    uint16_t removedCount;
    uint8_t targetMapId;
    uint8_t reserved0;
    uint8_t removedBits[ESP_NATIVE_GAMEPLAY_ACTION_REMOVED_SNAPSHOT_MAX_BYTES];
} EspNativeGameplayActionRemovedSnapshot;

typedef enum EspNativeGameplayActionFeedback_e {
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NONE = 0,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NOTHING = 1,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_FIRE_CLEARED = 2,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DOOR_CLEARED = 3,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN = 4,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP = 5,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE = 6,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT = 7,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NO_EFFECT = 8,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_TRAPPED = 9,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NO_AMMO = 10,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_COMBAT_TEXT = 11,
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT = 12
} EspNativeGameplayActionFeedback;

/* Allocation-free transient top-bar queue shared by native gameplay actions.
 * Queue/cancel form a tiny transaction: cancel succeeds only while the same
 * feedback is still pending and has not yet been presented. */
int EspNativeGameplayActionEngine_queueFeedback(
    EspNativeGameplayActionFeedback feedback);
/* Dynamic text remains a bounded transient presentation lease. Pickup feedback
 * requests the recovered 2 px / 500 ms white viewport border; incoming damage
 * uses the recovered legacy 0xBB0000 red border. Player-hit text is top-bar
 * only: enemy blood is owned by a separate bounded viewport overlay. The 944
 * RGB565 border pixels are snapshotted in 1888 B bounded static storage,
 * never heap. */
int EspNativeGameplayActionEngine_queueTextFeedback(
    EspNativeGameplayActionFeedback feedback,
    const char* text,
    uint16_t viewportFlashMs);
/*
 * Monster -> player impact direction in the player's CURRENT cardinal view.
 * The pure contract uses world positions from MonsterPosition and the exact
 * 64-unit step from EspPlayerView_cardinalBasis(). No trig, RNG or allocation.
 * Ties resolve forward/back before left/right. Coincident positions fail closed.
 */
typedef enum EspNativeGameplayDamageSide_e {
    ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE = 0,
    ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT = 1,
    ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT = 2,
    ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK = 3,
    ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT = 4
} EspNativeGameplayDamageSide;

static inline EspNativeGameplayDamageSide
EspNativeGameplayDamageSide_fromWorld(int32_t playerX, int32_t playerY,
                                      int32_t stepX, int32_t stepY,
                                      int32_t attackerX, int32_t attackerY) {
    int64_t dx;
    int64_t dy;
    int64_t front;
    int64_t right;
    uint64_t absFront;
    uint64_t absRight;
    /* Check the cardinal basis before any multiplication: unknown callers
     * must not overflow the pure projection by supplying arbitrary steps. */
    if (!((stepX == 64 || stepX == -64) && stepY == 0) &&
        !((stepY == 64 || stepY == -64) && stepX == 0)) {
        return ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE;
    }
    dx = (int64_t)attackerX - playerX;
    dy = (int64_t)attackerY - playerY;
    if (dx == 0 && dy == 0) return ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE;
    front = dx * stepX + dy * stepY;
    right = dy * stepX - dx * stepY;
    absFront = (uint64_t)(front < 0 ? -front : front);
    absRight = (uint64_t)(right < 0 ? -right : right);
    if (absFront >= absRight) {
        return front > 0 ? ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT
                         : ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK;
    }
    return right > 0 ? ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT
                     : ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT;
}

/* Bounded, allocation-free FIFO. The presentation owner consumes the oldest
 * committed attack first; repeated sides remain distinct when attacker/probe
 * IDs differ. This small pure contract also runs as a host regression. */
#define ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP 8U
typedef struct EspNativeGameplayDamageCue_s {
    uint32_t probe;
    uint16_t attackerSprite;
    uint8_t side;
    uint8_t reserved;
} EspNativeGameplayDamageCue;
typedef struct EspNativeGameplayDamageCueQueue_s {
    EspNativeGameplayDamageCue entries[ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP];
    uint8_t head;
    uint8_t count;
} EspNativeGameplayDamageCueQueue;

static inline int EspNativeGameplayDamageCueQueue_push(
    EspNativeGameplayDamageCueQueue* fifo,
    uint32_t probe, uint16_t attackerSprite,
    EspNativeGameplayDamageSide side) {
    uint8_t tail;
    if (fifo == NULL || probe == 0U || attackerSprite == 0xffffU ||
        side < ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT ||
        side > ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT ||
        fifo->count >= ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP) return 0;
    tail = (uint8_t)((fifo->head + fifo->count) %
                     ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP);
    fifo->entries[tail].probe = probe;
    fifo->entries[tail].attackerSprite = attackerSprite;
    fifo->entries[tail].side = (uint8_t)side;
    fifo->entries[tail].reserved = 0U;
    ++fifo->count;
    return 1;
}

static inline int EspNativeGameplayDamageCueQueue_pop(
    EspNativeGameplayDamageCueQueue* fifo,
    EspNativeGameplayDamageCue* outCue) {
    if (fifo == NULL || outCue == NULL || fifo->count == 0U ||
        fifo->count > ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP) return 0;
    *outCue = fifo->entries[fifo->head];
    fifo->head = (uint8_t)((fifo->head + 1U) %
                           ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP);
    --fifo->count;
    return 1;
}

/* Independent presentation-only FIFO. Every committed monster hit keeps
 * its existing red 500 ms border; the direction band blinks concurrently on
 * top of it. Post-COMMIT enqueue tries to present the first pulse immediately
 * through the existing compositor, before the next member can consume the
 * border lease. Touch/PAK/level-up/transaction barriers defer that UI paint;
 * presentation failure is retried and never rolls back committed gameplay.
 * No wait for the blink cadence or new world render is introduced. */
int EspNativeGameplayActionEngine_queueDirectionalDamage(
    uint32_t attackProbe, uint16_t attackerSprite,
    EspNativeGameplayDamageSide side);

/* Integer 50%-alpha RGB565 blend, called on the existing framebuffer only.
 * A band snapshot is taken AFTER the base red border: restoring it returns
 * the red border, then its own owner restores the original world pixels. */
static inline uint16_t EspNativeGameplayDamageCue_blend565(
    uint16_t base, uint16_t overlay) {
    return (uint16_t)(((base & 0xf7deU) >> 1U) +
                      ((overlay & 0xf7deU) >> 1U));
}

/* Legacy Hud keeps at most five queued messages. This bounded batch API is
 * intentionally narrow: it lets one committed multi-pickup tile publish the
 * same ordered "Got ..." sequence without importing Hud_t or heap ownership. */
int EspNativeGameplayActionEngine_queuePickupFeedbackBatch(
    const char* const* texts,
    uint8_t count,
    uint16_t viewportFlashMs);
int EspNativeGameplayActionEngine_cancelQueuedFeedback(
    EspNativeGameplayActionFeedback feedback);
/* Full native frame producers call this immediately before present so an active
 * viewport flash can refresh its small border snapshot from fresh world pixels. */
void EspNativeGameplayActionEngine_markFreshFrame(void);

/*
 * Permanent native SELECT fallback behind the already-validated tile-event
 * action path. The legacy ordering remains event first, then an 8-tile trace.
 * This owner contains only compact map-local action state; it never imports
 * legacy Entity_t/Player_t/Combat_t ownership.
 */
void EspNativeGameplayActionEngine_reset(void);
int EspNativeGameplayActionEngine_service(struct DoomRPG_s* doomRpg);

/* Pointer-free semantic snapshot of map-local sprite removals owned by the
 * action engine (currently fire clears; future destructible removals may reuse
 * the same owner). Transient feedback/animation/counters are never persisted. */
int EspNativeGameplayActionEngine_snapshotRemoved(
    EspNativeGameplayActionRemovedSnapshot* outSnapshot);
int EspNativeGameplayActionEngine_restoreRemoved(
    const EspNativeGameplayActionRemovedSnapshot* snapshot);
uint32_t EspNativeGameplayActionEngine_removedFingerprint(void);

/*
 * The linker has one public --wrap owner for EspNativeGameplayAction_executeSelect.
 * Keep the existing action-engine layer as a private chain leaf so bounded
 * event families (currently native map transition) can run before the generic
 * entity trace without duplicating or bypassing the proven fallback.
 */
EspNativeGameplayActionStatus EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);

/* Read-only cross-layer diagnostic witness: when the generic trace selects an
 * enemy, the native combat trace must resolve the same sprite before arming.
 * Populated only by that specific generic route; no extra world trace or RNG. */
int EspNativeGameplayActionEngine_deferredMonsterTarget(
    uint32_t sequence, uint16_t* outSprite, uint16_t* outTile,
    uint8_t* outSubtype);
#define __wrap_EspNativeGameplayAction_executeSelect \
    EspNativeGameplayActionEngine_executeSelect

/*
 * The action engine already owns the older fire/destructible topology overlay.
 * Rename its linker-wrapper implementations into private leaves so permanent
 * native monster combat can compose a second, explicit monster-liveness/pain
 * overlay in front of them without duplicating the actual topology owner.
 */
int EspNativeGameplayActionEngine_getVisualState(uint32_t spriteIndex,
                                                 uint8_t* outVisualState);
int EspNativeGameplayActionEngine_getEntity(uint32_t spriteIndex,
                                            uint8_t* outType,
                                            uint8_t* outSubType,
                                            uint16_t* outLinkState,
                                            uint16_t* outLinkOrder);
#define __wrap_EspMapSpriteTopology_getVisualState \
    EspNativeGameplayActionEngine_getVisualState
#define __wrap_EspMapSpriteTopology_getEntity \
    EspNativeGameplayActionEngine_getEntity

/*
 * esp_native_gameplay_present_gate.c remains the sole linker --wrap owner for
 * Esp32PlatformVideo_present(). The historical action implementation is now a
 * private base leaf: compact native presentation families may compose in front
 * of it without becoming a second linker wrapper or bypassing action feedback.
 */
int EspNativeGameplayActionEngine_present(void);
int EspNativeGameplayActionEngine_presentBase(void);
#define __wrap_Esp32PlatformVideo_present EspNativeGameplayActionEngine_presentBase

#ifdef __cplusplus
}
#endif

#endif
