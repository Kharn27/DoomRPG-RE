#include <stddef.h>
#include <string.h>
#include "esp_native_audio_volume_codec.h"

int EspNativeAudioVolumeCodec_valid(uint8_t percent) {
    return percent == 25U || percent == 50U ||
           percent == 75U || percent == 100U;
}
static uint32_t fnv(const uint8_t* bytes) {
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < 6U; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}
int EspNativeAudioVolumeCodec_encode(
    uint8_t percent, uint8_t out[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES]) {
    uint32_t checksum;
    if (!out || !EspNativeAudioVolumeCodec_valid(percent)) return 0;
    memcpy(out, "DRAU", 4);
    out[4] = 1U;
    out[5] = percent;
    checksum = fnv(out);
    for (unsigned i=0; i<4U; ++i)
        out[6U+i] = (uint8_t)(checksum >> (8U*i));
    return 1;
}
int EspNativeAudioVolumeCodec_decode(
    const uint8_t* in, uint32_t bytes, uint8_t* percent) {
    uint32_t recorded = 0U;
    if (!in || !percent || bytes != ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES ||
        memcmp(in, "DRAU", 4) != 0 || in[4] != 1U ||
        !EspNativeAudioVolumeCodec_valid(in[5])) return 0;
    for (unsigned i=0; i<4U; ++i)
        recorded |= ((uint32_t)in[6U+i]) << (8U*i);
    if (recorded != fnv(in)) return 0;
    *percent = in[5]; /* Fail-closed: never mutate output on failure. */
    return 1;
}
