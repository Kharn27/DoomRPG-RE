#ifndef DOOMRPG_ESP32_NATIVE_HELP_SCENE_H
#define DOOMRPG_ESP32_NATIVE_HELP_SCENE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_HELP_BREATH_STEP_MS 80U
#define ESP_NATIVE_HELP_BREATH_STEPS 32U

/* ORIGINAL 2026-11-08 visual restored: one bounded 64x64 PAK-native 4bpp
 * wall material + 16 palette colors (2080 B). No BSP world, no plane map,
 * no screenshots, no extra frame. The active flash map ID and HELP opening
 * epoch choose among validated logical textures from the CURRENT PAK. */
int EspNativeHelpScene_begin(uint8_t flashCachedMapId, uint32_t helpEpochMs);
void EspNativeHelpScene_reset(void);
int EspNativeHelpScene_isReady(void);
uint8_t EspNativeHelpScene_previewMapId(void);
uint8_t EspNativeHelpScene_materialCount(void);
uint16_t EspNativeHelpScene_textureId(void);

/* The same non-blinking 2.56 s smoothstep for all native HELP illustrations.
 * In the 3x3 scene, original per-action RGB contours and labels breathe;
 * neither the scene backing nor entire button regions are tinted. */
uint8_t EspNativeHelpScene_breathStep(uint32_t elapsedMs);
uint8_t EspNativeHelpScene_breathLevel(uint8_t step);
uint8_t EspNativeHelpScene_breathPower(uint8_t level);

int EspNativeHelpScene_paint(uint16_t* framebuffer, uint8_t level);
int EspNativeHelpScene_paintRow(uint16_t* framebuffer, uint8_t sourceRow,
                                uint8_t level);

#ifdef __cplusplus
}
#endif
#endif
