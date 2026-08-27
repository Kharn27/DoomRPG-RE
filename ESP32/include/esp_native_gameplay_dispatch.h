#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_DISPATCH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_DISPATCH_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayDispatchStatus_e {
    ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED = 1,
    ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY = 2,
    ESP_NATIVE_GAMEPLAY_DISPATCH_ALREADY_ACTIVE = 3,
    ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED = 4,
    ESP_NATIVE_GAMEPLAY_DISPATCH_STALE = 5,
    ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED = 6,
    ESP_NATIVE_GAMEPLAY_DISPATCH_OK = 7,
    ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK = 8,
    ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_BLOCKED = 9,
    ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_UNSUPPORTED = 10
} EspNativeGameplayDispatchStatus;

/* Permanent runtime orientation owner for executed gameplay turns. The older
 * EspPlayerOrientationState remains the hardware-proven fresh-map
 * finishRotation owner; gameplay dispatch does not rewrite that historical
 * boundary. */
typedef struct EspNativeGameplayTurnState_s {
    int32_t viewSin;
    int32_t viewCos;
    int32_t viewStepX;
    int32_t viewStepY;
    uint32_t sequence;
    uint8_t destAngle;
    uint8_t lastAction;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayTurnState;

typedef struct EspNativeGameplayDispatchResult_s {
    uint32_t sequence;
    int8_t angleDelta;
    uint8_t action;
    uint8_t angleBefore;
    uint8_t angleAfter;
    uint8_t prepared;
    uint8_t committed;
    uint8_t rolledBack;
    uint8_t reserved;
} EspNativeGameplayDispatchResult;

typedef struct EspNativeGameplayMoveResult_s {
    uint32_t sequence;
    int32_t deltaX;
    int32_t deltaY;
    uint16_t sourceTile;
    uint16_t destTile;
    uint16_t blockerSpriteIndex;
    uint8_t action;
    uint8_t blockerType;
    uint8_t collisionStatus;
    uint8_t prepared;
    uint8_t committed;
    uint8_t rolledBack;
} EspNativeGameplayMoveResult;

void EspNativeGameplayDispatch_reset(void);
int EspNativeGameplayDispatch_isReady(void);
const EspNativeGameplayTurnState* EspNativeGameplayDispatch_view(void);

/* Adopt the already hardware-proven settled player/view orientation once when
 * native gameplay input becomes active. No world/render/legacy state changes. */
EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_adoptView(void);

/* Prepare only TURN_LEFT / TURN_RIGHT from one already-consumed semantic
 * intent. Every other known action returns DEFERRED with no player/view change. */
EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_prepareTurn(
    const EspNativeGameplayInputState* intent,
    EspPlayerViewState* outBeforeView,
    EspPlayerViewState* outAfterView,
    EspNativeGameplayTurnState* outBeforeTurn,
    EspNativeGameplayTurnState* outAfterTurn,
    EspNativeGameplayDispatchResult* outResult);

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_commitTurn(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    const EspNativeGameplayTurnState* expectedBeforeTurn,
    const EspNativeGameplayTurnState* preparedAfterTurn,
    EspNativeGameplayDispatchResult* ioResult);

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_rollbackTurn(
    const EspPlayerViewState* expectedAfterView,
    const EspPlayerViewState* restoreBeforeView,
    const EspNativeGameplayTurnState* expectedAfterTurn,
    const EspNativeGameplayTurnState* restoreBeforeTurn,
    EspNativeGameplayDispatchResult* ioResult);

/*
 * Prepare cardinal translation for FORWARD/BACK/STRAFE only. The permanent
 * orientation owner supplies the exact 64-unit step vectors. Collision is
 * checked before an EspPlayerViewState candidate is exposed. Closed native
 * line entities block; hardware validation proved an EspMapLineState-open line
 * is skipped so the same mutable door owner drives both render and collision.
 * Source EXIT / destination ENTER tile-event execution is layered around the
 * commit by the bounded move-event owner and remains fail-closed outside its
 * explicitly supported opcode families.
 */
EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_prepareMove(
    const EspNativeGameplayInputState* intent,
    EspPlayerViewState* outBeforeView,
    EspPlayerViewState* outAfterView,
    EspNativeGameplayMoveResult* outResult);

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_commitMove(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    EspNativeGameplayMoveResult* ioResult);

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_rollbackMove(
    const EspPlayerViewState* expectedAfterView,
    const EspPlayerViewState* restoreBeforeView,
    EspNativeGameplayMoveResult* ioResult);

#ifdef __cplusplus
}
#endif

#endif
