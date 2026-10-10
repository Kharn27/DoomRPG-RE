#ifndef DOOMRPG_ESP32_NATIVE_LEVEL_GATE_H
#define DOOMRPG_ESP32_NATIVE_LEVEL_GATE_H

#include <stdint.h>

/* Exact legacy EV_CHECK_COMPLETED_LEVEL (39) loop:
 * start = high16(arg1) - 1; seek first completion bit up to MAP_ITEMS=11.
 * The lower 16 bits of arg1 identify the BSP warning dialog string.
 * This pure predicate makes no assumption about historical SAVE provenance:
 * a bit that was never proven completed cannot grant entry credit. */
#define ESP_NATIVE_LEVEL_GATE_MAP_ITEMS 11U
#define ESP_NATIVE_LEVEL_GATE_COMPLETION_BITS 0x07ffU

static inline int EspNativeLevelGate_evaluate(
    uint32_t arg1,
    uint16_t completedLevels,
    uint16_t* outEligibleMask,
    uint8_t* outNeedsDialog) {
    const uint32_t firstMapId = arg1 >> 16U;
    uint32_t mask;
    if (outEligibleMask != 0) *outEligibleMask = 0U;
    if (outNeedsDialog != 0) *outNeedsDialog = 0U;
    if (outEligibleMask == 0 || outNeedsDialog == 0 ||
        firstMapId == 0U || firstMapId > ESP_NATIVE_LEVEL_GATE_MAP_ITEMS ||
        (completedLevels & ~ESP_NATIVE_LEVEL_GATE_COMPLETION_BITS) != 0U) {
        return 0;
    }
    mask = ESP_NATIVE_LEVEL_GATE_COMPLETION_BITS &
           ~((1UL << (firstMapId - 1U)) - 1UL);
    *outEligibleMask = (uint16_t)mask;
    *outNeedsDialog = (uint8_t)((completedLevels & mask) == 0U);
    return 1;
}

#endif
