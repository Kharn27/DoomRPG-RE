#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_FEEDBACK_MS 120U
#define ESP_NATIVE_GAMEPLAY_FEEDBACK_MAX_EDITS 512U

typedef struct EspNativeGameplayControlsStats_s {
    uint32_t baselineFNV;
    uint32_t overlayFNV;
    uint16_t edits;
    uint8_t action;
    uint8_t zone;
} EspNativeGameplayControlsStats;

/*
 * Permanent form of the hardware-validated Junction touch feedback contract:
 * controls are invisible at rest. A routed touch may temporarily decorate only
 * its hit rectangle with the recovered row-coded neon double-ring and action
 * glyph. Every edited RGB565 pixel is saved in a bounded static edit list and
 * restored exactly after 120 ms. No allocation, PAK IO or gameplay mutation.
 */
void EspNativeGameplayControls_reset(void);

int EspNativeGameplayControls_begin(
    const EspNativeGameplayTouchHit* hit,
    EspNativeGameplayControlsStats* outStats);

int EspNativeGameplayControls_isActive(void);
int EspNativeGameplayControls_isExpired(void);

int EspNativeGameplayControls_restore(
    int present,
    EspNativeGameplayControlsStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
