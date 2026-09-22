#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_ENGINE_H

#include "esp_native_gameplay_action.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_ACTION_REMOVED_SNAPSHOT_MAX_BYTES 128U

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
    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_NO_AMMO = 10
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
