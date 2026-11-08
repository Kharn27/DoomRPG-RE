#ifndef DOOMRPG_ESP32_NATIVE_HELP_SCENE_H
#define DOOMRPG_ESP32_NATIVE_HELP_SCENE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Map-dependent illustrative first-person corridor built from ONE real PAK wall
 * resource. No BSP, resident game state, ZIP, map-wide texels or framebuffer
 * duplication. The preview is not a screenshot of an actual map position. */
int EspNativeHelpScene_load(uint8_t cachedMapId, uint32_t activationSeed);
void EspNativeHelpScene_reset(void);
int EspNativeHelpScene_isReady(void);
uint16_t EspNativeHelpScene_textureId(void);

/* Draw x8..151 / y29..96 onto the existing 160x120 RGB565 framebuffer.
 * level is a 0..6 triangle intensity shared with the HELP HUD breathing. */
int EspNativeHelpScene_paint(uint16_t* framebuffer, uint8_t level);

/* Render only one 3-button row (0=top, 1=middle, 2=bottom), x8..151,
 * y39..61. The genuine PAK wall texels and feedback icons are reused.
 * Level 0 deliberately lights no buttons. Failure writes nothing. */
int EspNativeHelpScene_paintRow(uint16_t* framebuffer, uint8_t sourceRow,
                                uint8_t level);

#ifdef __cplusplus
}
#endif

#endif
