#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_CRATE_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_CRATE_STATE_H

#include <stdint.h>

#include "esp_map_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS 64U

typedef enum EspNativeGameplayCrateOutcome_e {
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE = 1,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM = 2,
    ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE = 3
} EspNativeGameplayCrateOutcome;

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
