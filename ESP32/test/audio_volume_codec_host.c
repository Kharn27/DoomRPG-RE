#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_native_audio_volume_codec.h"

int main(void) {
    static const uint8_t levels[] = {25U,50U,75U,100U};
    uint8_t bytes[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES];
    uint8_t damaged[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES];
    uint8_t restored = 137U;
    assert(ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES == 10U);
    assert(!EspNativeAudioVolumeCodec_valid(0U));
    assert(!EspNativeAudioVolumeCodec_valid(101U));
    assert(!EspNativeAudioVolumeCodec_encode(0U, bytes));
    assert(!EspNativeAudioVolumeCodec_encode(101U, bytes));
    assert(!EspNativeAudioVolumeCodec_encode(50U, NULL));
    assert(!EspNativeAudioVolumeCodec_decode(NULL,sizeof(bytes),&restored));
    assert(!EspNativeAudioVolumeCodec_decode(bytes,sizeof(bytes),NULL));

    for (unsigned i=0;i<4U;++i) {
        const uint8_t level=levels[i];
        assert(EspNativeAudioVolumeCodec_valid(level));
        assert(EspNativeAudioVolumeCodec_encode(level,bytes));
        assert(memcmp(bytes,"DRAU",4)==0 && bytes[4]==1U && bytes[5]==level);
        assert(EspNativeAudioVolumeCodec_decode(bytes,sizeof(bytes),&restored));
        assert(restored==level);
        memcpy(damaged,bytes,sizeof(bytes));
        for(unsigned j=0;j<sizeof(bytes);++j) {
            damaged[j] ^= 1U;
            restored=137U;
            assert(!EspNativeAudioVolumeCodec_decode(damaged,sizeof(bytes),&restored));
            assert(restored==137U);
            damaged[j] ^= 1U;
        }
        restored=137U;
        assert(!EspNativeAudioVolumeCodec_decode(bytes,sizeof(bytes)-1U,&restored));
        assert(restored==137U);
    }
    puts("AUDIOVOLUME codec PASS 25/50/75/100 + checksum + fail-closed + 10B");
    return 0;
}
