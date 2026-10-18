#ifndef DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H
#define DOOMRPG_ESP32_NATIVE_CONFIG_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Independent fixed-size native CYD config. No desktop v23 Config/SDL ABI.
 * Each record has magic DRCF, version=1, brightness, zero reserved and
 * LE32 FNV-1a over bytes 0..7. Exact length is mandatory. */
#define ESP_NATIVE_CONFIG_BYTES 12U
#define ESP_NATIVE_CONFIG_VERSION 1U

int EspNativeConfigCodec_encodeBrightness(uint8_t percent,
                                         uint8_t output[ESP_NATIVE_CONFIG_BYTES]);
int EspNativeConfigCodec_decodeBrightness(const uint8_t* bytes,
                                         uint32_t length,
                                         uint8_t* outPercent);

#ifdef __cplusplus
}
#endif

#endif
