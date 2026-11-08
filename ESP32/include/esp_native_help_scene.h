#ifndef DOOMRPG_ESP32_NATIVE_HELP_SCENE_H
#define DOOMRPG_ESP32_NATIVE_HELP_SCENE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_HELP_BREATH_STEP_MS 80U
#define ESP_NATIVE_HELP_BREATH_STEPS 32U

/* An independent, pre-game-only 32x32 BSP blockmap and up to three real
 * 64x64 indexed wall materials from DoomRPG-ESP32.pak. All data is immutable,
 * bounded BSS. Uses Entrance when there is no flash-staged map; no gameplay
 * session, player, screenshot, ZIP, persistent map arena or heap renderer. */
int EspNativeHelpScene_begin(uint8_t flashCachedMapId);
void EspNativeHelpScene_reset(void);
int EspNativeHelpScene_isReady(void);
uint8_t EspNativeHelpScene_previewMapId(void);
uint8_t EspNativeHelpScene_materialCount(void);

/* One continuous 2.56s smoothstep shared by every HELP illustration.
 * Only the imaginary touch-zone dividers receive its fading accent. */
uint8_t EspNativeHelpScene_breathStep(uint32_t elapsedMs);
uint8_t EspNativeHelpScene_breathLevel(uint8_t step);
uint8_t EspNativeHelpScene_breathPower(uint8_t level);

/* Framebuffer-only read-only mini raycast of the actual BSP blockmap;
 * each of the three instructional rows has a different camera/view. */
int EspNativeHelpScene_paint(uint16_t* framebuffer, uint8_t level);
int EspNativeHelpScene_paintRow(uint16_t* framebuffer, uint8_t sourceRow,
                                uint8_t level);

#ifdef __cplusplus
}
#endif
#endif
