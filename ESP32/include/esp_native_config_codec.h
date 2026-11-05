#ifndef DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H
#define DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DRCF record is still exactly 12 bytes: magic[0..3], version[4],
 * brightness[5], orientation[6], theme[7], FNV-1a[8..11].
 * V1: orientation/theme=0. V2: theme=0. V3: theme=0/1.
 * Historical files decode without writes; next SAVE migrates to V3. */
#define ESP_NATIVE_CONFIG_BYTES 12U
#define ESP_NATIVE_CONFIG_VERSION_V1 1U
#define ESP_NATIVE_CONFIG_VERSION_V2 2U
#define ESP_NATIVE_CONFIG_VERSION 3U

int EspNativeConfigCodec_encodeWithTheme(uint8_t percent, uint8_t orientation,
                                        uint8_t theme,
                                        uint8_t output[ESP_NATIVE_CONFIG_BYTES]);
int EspNativeConfigCodec_decodeWithTheme(const uint8_t* bytes, uint32_t length,
                                        uint8_t* outPercent, uint8_t* outOrientation,
                                        uint8_t* outTheme);

/* Older callers stay valid: they encode CLASSIC and decode ignoring theme. */
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
