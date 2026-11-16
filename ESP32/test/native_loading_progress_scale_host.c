#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_loading_progress_scale.h"

static void check_phases(uint8_t deferred) {
    const uint8_t phases[3] = {
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE,
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY,
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY
    };
    uint8_t previous = deferred ? 20U : 0U;
    for (unsigned p = 0U; p < 3U; ++p) {
        for (uint32_t n = 0U; n <= 100U; ++n) {
            const uint8_t next = EspNativeLoadingProgress_flashPercent(
                phases[p], n, 100U, deferred);
            assert(next >= previous);
            assert(next <= (deferred ? 85U : 100U));
            previous = next;
        }
    }
    assert(previous == (deferred ? 85U : 100U));
}
int main(void) {
    const uint8_t phases[3] = {
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE,
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY,
        ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY
    };
    const uint8_t normal[4] = {0U, 20U, 80U, 100U};
    const uint8_t late[4] = {20U, 33U, 72U, 85U};
    assert(EspNativeLoadingProgress_checkpointPercent(85U, 0U) == 85U);
    assert(EspNativeLoadingProgress_checkpointPercent(10U, 1U) == 2U);
    assert(EspNativeLoadingProgress_checkpointPercent(30U, 1U) == 7U);
    assert(EspNativeLoadingProgress_checkpointPercent(60U, 1U) == 14U);
    assert(EspNativeLoadingProgress_checkpointPercent(75U, 1U) == 17U);
    assert(EspNativeLoadingProgress_checkpointPercent(85U, 1U) == 20U);
    assert(EspNativeLoadingProgress_checkpointPercent(90U, 1U) == 90U);
    assert(EspNativeLoadingProgress_checkpointPercent(93U, 1U) == 93U);
    assert(EspNativeLoadingProgress_checkpointPercent(96U, 1U) == 96U);
    assert(EspNativeLoadingProgress_checkpointPercent(100U, 1U) == 100U);
    assert(EspNativeLoadingProgress_checkpointPercent(255U, 1U) == 100U);
    for (unsigned i = 0U; i < 3U; ++i) {
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 0U, 100U, 0U)
               == normal[i]);
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 100U, 100U, 0U)
               == normal[i + 1U]);
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 0U, 100U, 1U)
               == late[i]);
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 100U, 100U, 1U)
               == late[i + 1U]);
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 101U, 100U, 1U)
               == late[i + 1U]);
        assert(EspNativeLoadingProgress_flashPercent(phases[i], 1U, 0U, 1U)
               == 0U);
    }
    assert(EspNativeLoadingProgress_flashPercent(99U, 50U, 100U, 1U) == 0U);
    check_phases(0U);
    check_phases(1U);
    puts("loading progress scales: immediate/deferred, monotonic, zero total PASS");
    return 0;
}
