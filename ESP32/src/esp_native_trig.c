#include <stdint.h>
#include <string.h>
#include "esp_native_trig.h"
/* Process-owned 1 KiB, not a duplicate Render_t or map-wide allocation. */
static int32_t nativeSin[ESP_NATIVE_TRIG_COUNT];
static uint8_t ready;
void EspNativeTrig_reset(void) {
    ready=0U;
    memset(nativeSin,0,sizeof(nativeSin));
}
int EspNativeTrig_installLe(const uint8_t* data, size_t length) {
    ready=0U;
    if (data==NULL || length!=ESP_NATIVE_TRIG_BYTES ||
        sizeof(nativeSin[0])!=4U) return 0;
    for (uint32_t i=0U;i<ESP_NATIVE_TRIG_COUNT;++i) {
        uint32_t bits=(uint32_t)data[4U*i] |
           ((uint32_t)data[4U*i+1U]<<8U) |
           ((uint32_t)data[4U*i+2U]<<16U) |
           ((uint32_t)data[4U*i+3U]<<24U);
        memcpy(&nativeSin[i],&bits,sizeof(bits));
    }
    ready=1U;
    return 1;
}
const int32_t* EspNativeTrig_view(void) {
    return ready ? nativeSin : NULL;
}
