#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_config_codec.h"
#include "esp_native_orientation.h"
#include "esp_main_menu_skin.h"

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

int EspNativeConfigCodec_encodeWithTheme(uint8_t percent, uint8_t orientation,
                                        uint8_t theme,
                                        uint8_t output[ESP_NATIVE_CONFIG_BYTES]) {
    uint32_t checksum;
    if (output == NULL || !validPercent(percent) ||
        !Esp32Orientation_valid(orientation) ||
        theme > ESP_MAIN_MENU_THEME_PHOSPHORE) return 0;
    memset(output, 0, ESP_NATIVE_CONFIG_BYTES);
    output[0] = 'D';
    output[1] = 'R';
    output[2] = 'C';
    output[3] = 'F';
    output[4] = ESP_NATIVE_CONFIG_VERSION;
    output[5] = percent;
    output[6] = orientation;
    output[7] = theme;
    checksum = recordChecksum(output);
    output[8] = (uint8_t)checksum;
    output[9] = (uint8_t)(checksum >> 8);
    output[10] = (uint8_t)(checksum >> 16);
    output[11] = (uint8_t)(checksum >> 24);
    return 1;
}

int EspNativeConfigCodec_decodeWithTheme(const uint8_t* bytes, uint32_t length,
                                        uint8_t* outPercent, uint8_t* outOrientation,
                                        uint8_t* outTheme) {
    uint32_t recorded;
    uint8_t version, orientation, theme = ESP_MAIN_MENU_THEME_CLASSIC;
    if (bytes == NULL || outPercent == NULL || outOrientation == NULL ||
        outTheme == NULL ||
        length != ESP_NATIVE_CONFIG_BYTES ||
        bytes[0] != 'D' || bytes[1] != 'R' ||
        bytes[2] != 'C' || bytes[3] != 'F' ||
        !validPercent(bytes[5])) return 0;

    version = bytes[4];
    if (version == ESP_NATIVE_CONFIG_VERSION_V1) {
        if (bytes[6] != 0U || bytes[7] != 0U) return 0;
        orientation = ESP32_ORIENTATION_NORMAL;
    } else if (version == ESP_NATIVE_CONFIG_VERSION_V2) {
        orientation = bytes[6];
        if (!Esp32Orientation_valid(orientation) || bytes[7] != 0U) return 0;
    } else if (version == ESP_NATIVE_CONFIG_VERSION) {
        orientation = bytes[6];
        theme = bytes[7];
        if (!Esp32Orientation_valid(orientation) ||
            theme > ESP_MAIN_MENU_THEME_PHOSPHORE) return 0;
    } else return 0;

    recorded = (uint32_t)bytes[8] |
               ((uint32_t)bytes[9] << 8) |
               ((uint32_t)bytes[10] << 16) |
               ((uint32_t)bytes[11] << 24);
    if (recorded != recordChecksum(bytes)) return 0;

    /* Commit outputs only after all checks, never partially update on error. */
    *outPercent = bytes[5];
    *outOrientation = orientation;
    *outTheme = theme;
    return 1;
}

int EspNativeConfigCodec_encode(uint8_t percent, uint8_t orientation,
                                uint8_t output[ESP_NATIVE_CONFIG_BYTES]) {
    return EspNativeConfigCodec_encodeWithTheme(
        percent, orientation, ESP_MAIN_MENU_THEME_CLASSIC, output);
}

int EspNativeConfigCodec_decode(const uint8_t* bytes, uint32_t length,
                                uint8_t* outPercent, uint8_t* outOrientation) {
    uint8_t theme = ESP_MAIN_MENU_THEME_CLASSIC;
    return EspNativeConfigCodec_decodeWithTheme(
        bytes, length, outPercent, outOrientation, &theme);
}

int EspNativeConfigCodec_encodeBrightness(uint8_t percent,
                                         uint8_t output[ESP_NATIVE_CONFIG_BYTES]) {
    return EspNativeConfigCodec_encode(percent, ESP32_ORIENTATION_NORMAL, output);
}

int EspNativeConfigCodec_decodeBrightness(const uint8_t* bytes,
                                         uint32_t length, uint8_t* outPercent) {
    uint8_t orientation = 0U;
    return EspNativeConfigCodec_decode(bytes, length, outPercent, &orientation);
}
