#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_COLLISION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_COLLISION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_COLLISION_LEGACY_TRACE_MASK 0xf287U
#define ESP_NATIVE_GAMEPLAY_COLLISION_NO_SPRITE 0xffffU

typedef enum EspNativeGameplayCollisionStatus_e {
    ESP_NATIVE_GAMEPLAY_COLLISION_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_COLLISION_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES = 2,
    ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_WALL = 3,
    ESP_NATIVE_GAMEPLAY_COLLISION_BLOCKED_ENTITY = 4,
    ESP_NATIVE_GAMEPLAY_COLLISION_CLEAR = 5
} EspNativeGameplayCollisionStatus;

typedef struct EspNativeGameplayCollisionResult_s {
    uint16_t sourceTile;
    uint16_t destTile;
    uint16_t blockerSpriteIndex;
    uint8_t sourceFlags;
    uint8_t destFlags;
    uint8_t blockerType;
    uint8_t blockerSubType;
    uint8_t linkedBlockers;
    uint8_t openLineCount;
    uint8_t status;
    uint8_t reserved[3];
} EspNativeGameplayCollisionResult;

/*
 * Reproduce the collision-only prefix of DoomCanvas_attemptMove()/Game_trace()
 * for one cardinal 64-unit tile-center step. Current native gameplay has no
 * legacy Entity_t database; the query therefore uses the permanent map WALL
 * bit plus compact linked sprite/entity topology. The recovered Game_trace()
 * movement mask 62087 / 0xf287 blocks entity types 0,1,2,7,9,12,13,14,15.
 *
 * Door/line collision relinking remains a later milestone. To avoid allowing a
 * stale wall through an opened native line, any live open line makes this
 * function fail closed with UNSUPPORTED_DYNAMIC_LINES. No owner is mutated.
 */
EspNativeGameplayCollisionStatus EspNativeGameplayCollision_traceCardinalStep(
    int32_t sourceX,
    int32_t sourceY,
    int32_t destX,
    int32_t destY,
    EspNativeGameplayCollisionResult* outResult);

const char* EspNativeGameplayCollision_statusName(
    EspNativeGameplayCollisionStatus status);

#ifdef __cplusplus
}
#endif

#endif
