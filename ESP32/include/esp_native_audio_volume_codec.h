#ifndef ESP_NATIVE_AUDIO_VOLUME_CODEC_H
#define ESP_NATIVE_AUDIO_VOLUME_CODEC_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Separate 10-byte versioned DRAU preference, never migrate or rewrite the
 * existing 12-byte /DoomRPG-ESP32.cfg video/orientation/theme record. */
#define ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES 10U
int EspNativeAudioVolumeCodec_valid(uint8_t percent);
int EspNativeAudioVolumeCodec_encode(
    uint8_t percent, uint8_t out[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES]);
int EspNativeAudioVolumeCodec_decode(const uint8_t* in, uint32_t bytes,
                                      uint8_t* percent);
#ifdef __cplusplus
}
#endif
#endif
