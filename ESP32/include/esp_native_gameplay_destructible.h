#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_DESTRUCTIBLE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_DESTRUCTIBLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_DEATH_RUN_FLAGS 0x00000100UL

typedef enum EspNativeGameplayDestructibleStatus_e {
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EVENT = 2,
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT = 3,
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EFFECT = 4,
    ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK = 5
} EspNativeGameplayDestructibleStatus;

typedef struct EspNativeGameplayDestructibleResult_s {
    uint16_t eventIndex;
    uint16_t lineIndex;
    uint16_t globalCommandIndex;
    uint8_t commandOffset;
    uint8_t codeId;
    uint8_t openBefore;
    uint8_t openAfter;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t mutated;
    uint8_t rollbackAvailable;
    uint8_t reserved[2];
} EspNativeGameplayDestructibleResult;

/*
 * Recover the exact bounded Game_remove(line-entity) consequence without
 * allocating or touching legacy Entity/Render state. The legacy route invokes
 * Game_executeTile(line midpoint, 0x100). For this milestone we own only the
 * safe permanent subset where that filtered event contains exactly one
 * eligible EV_OPENLINE targeting the same line. Anything more complex remains
 * fail-closed for its dedicated milestone.
 */
EspNativeGameplayDestructibleStatus
EspNativeGameplayDestructible_preflightLineDeath(uint16_t eventTile,
                                                 uint16_t expectedLineIndex,
                                                 EspNativeGameplayDestructibleResult* outResult);

EspNativeGameplayDestructibleStatus
EspNativeGameplayDestructible_executeLineDeath(uint16_t eventTile,
                                               uint16_t expectedLineIndex,
                                               EspNativeGameplayDestructibleResult* outResult);

int EspNativeGameplayDestructible_rollbackLineDeath(
    const EspNativeGameplayDestructibleResult* result);

const char* EspNativeGameplayDestructible_statusName(
    EspNativeGameplayDestructibleStatus status);

#ifdef __cplusplus
}
#endif

#endif
