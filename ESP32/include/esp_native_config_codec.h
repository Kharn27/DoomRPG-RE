#ifndef DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H
#define DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DRCF byte layout remains exactly 12 bytes:
 * 0..3 magic, 4 version, 5 brightness, 6 orientation, 7 reserved=0,
 * 8..11 little-endian FNV-1a of bytes 0..7. V1 requires both 6 and 7
 * to be zero, and decodes as NORMAL without losing brightness. */
#define ESP_NATIVE_CONFIG_BYTES 12U
#define ESP_NATIVE_CONFIG_VERSION_V1 1U
#define ESP_NATIVE_CONFIG_VERSION 2U

int EspNativeConfigCodec_encode(uint8_t percent, uint8_t orientation,
                               uint8_t output[ESP_NATIVE_CONFIG_BYTES]);
int EspNativeConfigCodec_decode(const uint8_t* bytes, uint32_t length,
                               uint8_t* outPercent, uint8_t* outOrientation);

/* Compatibility with tests/consumers of the old brightness-only codec.
 * New encodes always produce v2 with default NORMAL. */
int EspNativeConfigCodec_encodeBrightness(uint8_t percent,
                                         uint8_t output[ESP_NATIVE_CONFIG_BYTES]);
int EspNativeConfigCodec_decodeBrightness(const uint8_t* bytes,
                                         uint32_t length, uint8_t* outPercent);

#ifdef __cplusplus
}
#endif
#endif
