#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_config_codec.h"
#include "esp_native_orientation.h"

static void expectRejected(const uint8_t* data, uint32_t size) {
    uint8_t brightness = 137U, orientation = 137U;
    assert(!EspNativeConfigCodec_decode(data, size, &brightness, &orientation));
    assert(brightness == 137U && orientation == 137U);
}

static void resignLegacy(uint8_t* bytes) {
    uint32_t fnv = 2166136261U;
    for (unsigned i = 0; i < 8U; ++i) {
        fnv ^= bytes[i];
        fnv *= 16777619U;
    }
    for (unsigned i = 0; i < 4U; ++i) bytes[8U + i] = (uint8_t)(fnv >> (8U * i));
}

static void checkTouch(int16_t x, int16_t y) {
    int16_t nx = x, ny = y;
    assert(Esp32Orientation_transformTouch(ESP32_ORIENTATION_NORMAL, &nx, &ny));
    assert(nx == x && ny == y);
    assert(Esp32Orientation_transformTouch(ESP32_ORIENTATION_INVERSE, &nx, &ny));
    assert(nx == 319 - x && ny == 239 - y);
    assert(Esp32Orientation_transformTouch(ESP32_ORIENTATION_INVERSE, &nx, &ny));
    assert(nx == x && ny == y); /* mathematical involution, input calls once */
}
int main(void) {
    static const uint8_t levels[] = {25U, 50U, 75U, 100U};
    uint8_t packet[ESP_NATIVE_CONFIG_BYTES], corrupted[ESP_NATIVE_CONFIG_BYTES];
    uint8_t percent = 0U, orientation = 0U;

    assert(ESP_NATIVE_CONFIG_BYTES == 12U);
    assert(ESP_NATIVE_CONFIG_VERSION_V1 == 1U);
    assert(ESP_NATIVE_CONFIG_VERSION == 2U);
    assert(!EspNativeConfigCodec_encode(0U, 0U, packet));
    assert(!EspNativeConfigCodec_encode(101U, 0U, packet));
    assert(!EspNativeConfigCodec_encode(100U, 2U, packet));
    assert(!EspNativeConfigCodec_encode(100U, 255U, packet));
    assert(!EspNativeConfigCodec_encode(100U, 0U, NULL));
    expectRejected(NULL, 12U);
    expectRejected(packet, 0U);
    assert(!EspNativeConfigCodec_decode(packet, 12U, NULL, &orientation));
    assert(!EspNativeConfigCodec_decode(packet, 12U, &percent, NULL));

    for (unsigned p = 0; p < sizeof(levels); ++p) {
        for (uint8_t mode = 0; mode <= 1; ++mode) {
            assert(EspNativeConfigCodec_encode(levels[p], mode, packet));
            assert(packet[0] == 'D' && packet[1] == 'R' &&
                   packet[2] == 'C' && packet[3] == 'F');
            assert(packet[4] == 2U && packet[5] == levels[p]);
            assert(packet[6] == mode && packet[7] == 0U);
            assert(EspNativeConfigCodec_decode(packet, 12U, &percent, &orientation));
            assert(percent == levels[p] && orientation == mode);
            for (uint32_t size = 0; size < 12U; ++size) expectRejected(packet, size);
            expectRejected(packet, 13U);
            for (uint32_t i = 0; i < 12U; ++i) {
                memcpy(corrupted, packet, sizeof(packet));
                corrupted[i] ^= (uint8_t)(1U << (i % 8));
                expectRejected(corrupted, 12U);
            }
            memcpy(corrupted, packet, 12U);
            corrupted[4] = 23U; /* Desktop v23 is unrelated. */
            resignLegacy(corrupted);
            expectRejected(corrupted, 12U);
            memcpy(corrupted, packet, 12U);
            corrupted[6] = 2U;
            resignLegacy(corrupted); /* Even checksummed invalid values fail. */
            expectRejected(corrupted, 12U);
            memcpy(corrupted, packet, 12U);
            corrupted[7] = 1U;
            resignLegacy(corrupted);
            expectRejected(corrupted, 12U);
        }
        /* Valid historical v1 record: reserved bytes both zero, retain PWM. */
        assert(EspNativeConfigCodec_encodeBrightness(levels[p], packet));
        packet[4] = ESP_NATIVE_CONFIG_VERSION_V1;
        resignLegacy(packet);
        assert(EspNativeConfigCodec_decode(packet, 12U, &percent, &orientation));
        assert(percent == levels[p] && orientation == ESP32_ORIENTATION_NORMAL);
        assert(EspNativeConfigCodec_decodeBrightness(packet, 12U, &percent));
        assert(percent == levels[p]);
        packet[6] = 1U;
        resignLegacy(packet);
        expectRejected(packet, 12U);
    }

    checkTouch(0, 0);
    checkTouch(319, 0);
    checkTouch(0, 239);
    checkTouch(319, 239);
    checkTouch(160, 120);
    int16_t x = 0, y = 0;
    assert(!Esp32Orientation_transformTouch(2U, &x, &y));
    assert(!Esp32Orientation_transformTouch(0U, NULL, &y));
    assert(!Esp32Orientation_transformTouch(0U, &x, NULL));
    x = 320; assert(!Esp32Orientation_transformTouch(1U, &x, &y));
    x = 0; y = -1; assert(!Esp32Orientation_transformTouch(1U, &x, &y));

    puts("PASS native CYD config v1->v2 12B migration/corrupt/rollback-safe codec; touch corners+centre 180-degree");
    return 0;
}
