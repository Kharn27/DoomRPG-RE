#ifndef DOOMRPG_ESP32_MAP_TRANSITION_PREFLIGHT_H
#define DOOMRPG_ESP32_MAP_TRANSITION_PREFLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspMapTransitionPreflightStatus_e {
    ESP_MAP_TRANSITION_PREFLIGHT_INVALID = 0,
    ESP_MAP_TRANSITION_PREFLIGHT_PACK_BUSY = 1,
    ESP_MAP_TRANSITION_PREFLIGHT_PACK_OPEN_FAILED = 2,
    ESP_MAP_TRANSITION_PREFLIGHT_ENTRY_NOT_FOUND = 3,
    ESP_MAP_TRANSITION_PREFLIGHT_BSP_INVALID = 4,
    ESP_MAP_TRANSITION_PREFLIGHT_ID_MISMATCH = 5,
    ESP_MAP_TRANSITION_PREFLIGHT_OK = 6
} EspMapTransitionPreflightStatus;

/*
 * Caller-owned summary proving that one catalog target exists in the native
 * pack and that its complete original BSP can be structurally inventoried and
 * CRC-verified before any source-map teardown occurs.
 *
 * No pointers or map-sized data are retained. persistentPlanBytes is the
 * existing compact-plan estimate produced by EspBspReader and is advisory for
 * the later destructive lifecycle handoff.
 */
typedef struct EspMapTransitionPreflightResult_s {
    uint32_t nameHash;
    uint32_t entryOffset;
    uint32_t sourceBytes;
    uint32_t sourceCrc32;
    uint32_t sourceFNV1a;
    uint32_t persistentPlanBytes;
    uint32_t nodes;
    uint32_t lines;
    uint32_t mapSprites;
    uint32_t events;
    uint32_t byteCodes;
    uint32_t strings;
    uint32_t stringDataBytes;
    uint8_t targetMapId;
    uint8_t headerLoadMapId;
    uint8_t ready;
    uint8_t reserved;
} EspMapTransitionPreflightResult;

/*
 * Run a read-only target preflight against /DoomRPG-ESP32.pak.
 *
 * The pack must be closed on entry. The function opens it, performs a bounded
 * SD-index lookup and full streaming BSP inventory through the existing 256 B
 * reader window, then closes it before returning. It never resets or mutates
 * the current native map runtime/overlays and retains no allocation.
 */
EspMapTransitionPreflightStatus EspMapTransitionPreflight_run(
    uint8_t targetMapId,
    EspMapTransitionPreflightResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
