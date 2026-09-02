#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TRACE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayMonsterTraceStatus_e {
    ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_CLEAR = 2,
    ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_BLOCKED_OTHER = 3,
    ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND = 4
} EspNativeGameplayMonsterTraceStatus;

typedef struct EspNativeGameplayMonsterTarget_s {
    uint32_t worldDistance;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t distance;
    uint8_t type;
    uint8_t reserved;
} EspNativeGameplayMonsterTarget;

/* Reproduce the resident event/entity forward trace generically. The first
 * trace-blocking entity wins; only type=1 is returned as FOUND. Dead native
 * monster records are skipped, while every other blocking entity terminates the
 * trace exactly so combat can never shoot through doors, humans or props. */
EspNativeGameplayMonsterTraceStatus EspNativeGameplayMonsterTrace_forward(
    EspNativeGameplayMonsterTarget* outTarget);

#ifdef __cplusplus
}
#endif

#endif
