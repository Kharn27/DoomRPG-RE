#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_CONTROLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayControlsStats_s {
    uint32_t pixelsTouched;
    uint32_t borderPixels;
    uint32_t glyphPixels;
    uint8_t zonesDrawn;
    uint8_t activeActions;
    uint8_t deferredActions;
    uint8_t reserved;
} EspNativeGameplayControlsStats;

/*
 * Draw the permanent CYD virtual gameplay pad as the final framebuffer layer.
 * Geometry comes exclusively from EspNativeGameplayInput_zoneAt(). Pixels are
 * additive RGB565 outlines/glyphs: world, sprites and HUD remain visible below.
 * No allocation, PAK IO, presentation or gameplay owner mutation occurs here.
 */
int EspNativeGameplayControls_draw(
    uint16_t* framebuffer,
    uint32_t framebufferPixels,
    EspNativeGameplayControlsStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
