#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_FEEDBACK_MS 120U
/* The largest native HUB actions are the 128x28 SAVE/LOAD cards. Their two
 * outlines plus the centered action glyph require 625 reversible pixel edits.
 * 640 keeps bounded headroom while the compact 4-byte edit journal avoids
 * consuming the contiguous boot heap needed by legacy menu.bsp structures. */
#define ESP_NATIVE_GAMEPLAY_FEEDBACK_MAX_EDITS 640U

typedef struct EspNativeGameplayControlsStats_s {
    uint32_t baselineFNV;
    uint32_t overlayFNV;
    uint32_t restoredFNV;
    uint16_t edits;
    uint16_t conflicts;
    uint8_t action;
    uint8_t zone;
} EspNativeGameplayControlsStats;

/*
 * Permanent form of the hardware-validated Junction touch feedback contract:
 * controls are invisible at rest. A routed touch may temporarily decorate only
 * its hit rectangle with the recovered row-coded neon double-ring and action
 * glyph. Every edited RGB565 pixel is saved in a bounded static edit list and
 * restored after 120 ms unless a newer overlay has since claimed that exact
 * pixel. No allocation, PAK IO or gameplay mutation.
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
