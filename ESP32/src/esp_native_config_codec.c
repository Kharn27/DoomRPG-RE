#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_config_codec.h"

static int validPercent(uint8_t percent) {
    return percent == 25U || percent == 50U ||
           percent == 75U || percent == 100U;
}

static uint32_t recordChecksum(const uint8_t* bytes) {
    uint32_t checksum = 2166136261U;
    int i;
    for (i = 0; i < 8; ++i) {
        checksum ^= bytes[i];
        checksum *= 16777619U;
    }
    return checksum;
}

int EspNativeConfigCodec_encodeBrightness(uint8_t percent,
                                         uint8_t output[ESP_NATIVE_CONFIG_BYTES]) {
    uint32_t checksum;
    if (output == NULL || !validPercent(percent)) return 0;

    memset(output, 0, ESP_NATIVE_CONFIG_BYTES);
    output[0] = 'D';
    output[1] = 'R';
    output[2] = 'C';
    output[3] = 'F';
    output[4] = ESP_NATIVE_CONFIG_VERSION;
    output[5] = percent;
    checksum = recordChecksum(output);
    output[8] = (uint8_t)checksum;
    output[9] = (uint8_t)(checksum >> 8);
    output[10] = (uint8_t)(checksum >> 16);
    output[11] = (uint8_t)(checksum >> 24);
    return 1;
}

int EspNativeConfigCodec_decodeBrightness(const uint8_t* bytes,
                                         uint32_t length,
                                         uint8_t* outPercent) {
    uint32_t recorded;
    if (bytes == NULL || outPercent == NULL ||
        length != ESP_NATIVE_CONFIG_BYTES ||
        bytes[0] != 'D' || bytes[1] != 'R' ||
        bytes[2] != 'C' || bytes[3] != 'F' ||
        bytes[4] != ESP_NATIVE_CONFIG_VERSION ||
        !validPercent(bytes[5]) ||
        bytes[6] != 0U || bytes[7] != 0U) return 0;

    recorded = (uint32_t)bytes[8] |
               ((uint32_t)bytes[9] << 8) |
               ((uint32_t)bytes[10] << 16) |
               ((uint32_t)bytes[11] << 24);
    if (recorded != recordChecksum(bytes)) return 0;

    *outPercent = bytes[5];
    return 1;
}
