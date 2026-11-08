#ifndef DOOMRPG_ESP32_NATIVE_HELP_SCENE_H
#define DOOMRPG_ESP32_NATIVE_HELP_SCENE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_HELP_BREATH_STEP_MS 80U
#define ESP_NATIVE_HELP_BREATH_STEPS 32U

/* A 128x64 RGB565 snapshot of a REAL already-presented native gameplay
 * viewport, captured once per map without PAK/BSP/world ownership or heap.
 * 16384 bytes static BSS; no second 160x120 frame. Only a 160x80 viewport
 * rendered by the canonical native compositor may call this. */
int EspNativeHelpScene_captureGameplayViewport(const uint16_t* framebuffer,
                                               uint8_t mapId);
/* HELP owns only the display; closing it preserves last completed snapshot. */
void EspNativeHelpScene_begin(uint8_t cachedMapId);
void EspNativeHelpScene_reset(void);
int EspNativeHelpScene_isReady(void);
int EspNativeHelpScene_hasGameplaySnapshot(void);
uint8_t EspNativeHelpScene_snapshotMapId(void);

/* One mathematically smooth, symmetric 2.56s breath for all illustrations. */
uint8_t EspNativeHelpScene_breathStep(uint32_t elapsedMs);
uint8_t EspNativeHelpScene_breathLevel(uint8_t step);
uint8_t EspNativeHelpScene_breathPower(uint8_t level);

/* Paint directly into the only 160x120 RGB565 frame, never touch the HUD
 * footer or live game world. At cold boot: starfield, no fake corridor. */
int EspNativeHelpScene_paint(uint16_t* framebuffer, uint8_t level);
int EspNativeHelpScene_paintRow(uint16_t* framebuffer, uint8_t sourceRow,
                                uint8_t level);

#ifdef __cplusplus
}
#endif
#endif
