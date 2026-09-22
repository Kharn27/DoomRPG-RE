#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_CRATE_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_CRATE_STATE_H

#include <stdint.h>

#include "esp_map_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS 64U
#define ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_MAX_SPRITE_BYTES 128U
#define ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_CODE_BYTES 32U

typedef enum EspNativeGameplayCrateOutcome_e {
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE = 1,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM = 2,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE = 3
} EspNativeGameplayCrateOutcome;

typedef struct EspNativeGameplayCrateTransformSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t stateFNV1a;
    uint16_t spriteCount;
    uint16_t transformedCount;
    uint16_t transformedBytes;
    uint8_t targetMapId;
    uint8_t reserved0;
    uint8_t transformedBits[ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_MAX_SPRITE_BYTES];
    /*
     * One 4-bit target code per transformed sprite, ordered by ascending
     * spriteIndex among set transformedBits. Codes 1..9 map to the exact
     * allowed crate pickup targets; 0 and 10..15 are invalid.
     */
    uint8_t defCodes[ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_CODE_BYTES];
} EspNativeGameplayCrateTransformSnapshot;

typedef struct EspNativeGameplayCrateStateView_s {
    uint32_t sourceArenaFNV1a;
    uint16_t spriteCount;
    uint16_t crateCount;
    uint16_t transformedCount;
    uint8_t targetMapId;
    uint8_t active;
    uint8_t fatal;
    uint8_t reserved;
} EspNativeGameplayCrateStateView;

/*
 * Compact map-local overlay for legacy type-12/subtype-2 crate transformations.
 * Immutable BSP sprite records and topology are never mutated. Each transformed
 * crate stores only {spriteIndex,effective EntityDef tile}; renderer/topology/
 * pickup consumers project that semantic state through their existing wrappers.
 */
void EspNativeGameplayCrateState_reset(void);
int EspNativeGameplayCrateState_ensure(void);
const EspNativeGameplayCrateStateView* EspNativeGameplayCrateState_view(void);

/*
 * Pointer-free checkpoint state. Snapshot encoding is canonical and compact:
 * a sprite bitset plus packed 4-bit target codes in ascending sprite order.
 * Transient probe/allocator state is never persisted.
 */
int EspNativeGameplayCrateState_snapshot(
    EspNativeGameplayCrateTransformSnapshot* outSnapshot);
int EspNativeGameplayCrateState_restore(
    const EspNativeGameplayCrateTransformSnapshot* snapshot);
int EspNativeGameplayCrateState_snapshotShapeValid(
    const EspNativeGameplayCrateTransformSnapshot* snapshot,
    uint32_t expectedArenaFNV1a,
    uint8_t expectedTargetMapId);
uint32_t EspNativeGameplayCrateState_fingerprint(void);

int EspNativeGameplayCrateState_isTransformed(uint32_t spriteIndex);
int EspNativeGameplayCrateState_effectiveDefTile(uint32_t spriteIndex,
                                                 uint16_t* outDefTile);
int EspNativeGameplayCrateState_transform(uint16_t spriteIndex,
                                          uint16_t effectiveDefTile);
int EspNativeGameplayCrateState_rollbackTransform(uint16_t spriteIndex,
                                                  uint16_t effectiveDefTile);

/* Projection helpers. Call only after the underlying immutable owner returned
 * successfully. They do not allocate and do not mutate source records. */
int EspNativeGameplayCrateState_applyMapSprite(uint32_t spriteIndex,
                                               EspMapSprite* ioSprite);
int EspNativeGameplayCrateState_applyEntity(uint32_t spriteIndex,
                                            uint8_t* ioType,
                                            uint8_t* ioSubtype,
                                            uint16_t* ioLinkState);

/* Exact Entity_died(type12/subtype2) consequence classifier. The caller owns
 * gameplay RNG ordering. 150..212 requires the second byte; every other bucket
 * must pass secondValid=0 and consumes no second consequence byte. */
int EspNativeGameplayCrateState_requiresSecondRng(uint8_t first);
int EspNativeGameplayCrateState_resolveOutcome(
    uint8_t first,
    uint8_t second,
    uint8_t secondValid,
    EspNativeGameplayCrateOutcome* outOutcome,
    uint16_t* outEffectiveDefTile);

#ifdef __cplusplus
}
#endif

#endif
