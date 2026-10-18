#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_config_codec.h"

static void expectRejected(const uint8_t* data, uint32_t size) {
    uint8_t chosen = 137U;
    assert(!EspNativeConfigCodec_decodeBrightness(data, size, &chosen));
    assert(chosen == 137U);
}

int main(void) {
    static const uint8_t levels[] = {25U, 50U, 75U, 100U};
    uint8_t packet[ESP_NATIVE_CONFIG_BYTES];
    uint8_t corrupted[ESP_NATIVE_CONFIG_BYTES];
    uint8_t choice = 0U;

    assert(ESP_NATIVE_CONFIG_BYTES == 12U);
    assert(ESP_NATIVE_CONFIG_VERSION == 1U);
    assert(!EspNativeConfigCodec_encodeBrightness(0U, packet));
    assert(!EspNativeConfigCodec_encodeBrightness(1U, packet));
    assert(!EspNativeConfigCodec_encodeBrightness(26U, packet));
    assert(!EspNativeConfigCodec_encodeBrightness(101U, packet));
    assert(!EspNativeConfigCodec_encodeBrightness(100U, NULL));
    expectRejected(NULL, 12U);
    expectRejected(packet, 0U);
    assert(!EspNativeConfigCodec_decodeBrightness(packet, 12U, NULL));

    for (unsigned i = 0; i < sizeof(levels); ++i) {
        assert(EspNativeConfigCodec_encodeBrightness(levels[i], packet));
        assert(packet[0] == 'D' && packet[1] == 'R' &&
               packet[2] == 'C' && packet[3] == 'F');
        assert(packet[4] == 1U && packet[5] == levels[i]);
        assert(packet[6] == 0U && packet[7] == 0U);
        assert(EspNativeConfigCodec_decodeBrightness(packet, 12U, &choice));
        assert(choice == levels[i]);
        for (uint32_t size = 0; size < ESP_NATIVE_CONFIG_BYTES; ++size)
            expectRejected(packet, size); /* truncated SD record */
        expectRejected(packet, 13U); /* unexpected trailing bytes */

        /* All header and payload bytes are covered by checksum/version. */
        for (uint32_t byte = 0; byte < ESP_NATIVE_CONFIG_BYTES; ++byte) {
            memcpy(corrupted, packet, sizeof(packet));
            corrupted[byte] ^= (uint8_t)(1U << (byte % 8));
            expectRejected(corrupted, sizeof(corrupted));
        }
        memcpy(corrupted, packet, sizeof(packet));
        corrupted[4] = 23U; /* desktop Config is a distinct format */
        expectRejected(corrupted, sizeof(corrupted));
        memcpy(corrupted, packet, sizeof(packet));
        corrupted[5] = 0U; /* never blank the touch-only screen */
        expectRejected(corrupted, sizeof(corrupted));
    }

    printf("PASS native CYD config codec levels=4 length=12 version=1 FNV/magic/reserved/invalid/truncated guarded\n");
    return 0;
}
