#ifndef ESP_NATIVE_TRIG_H
#define ESP_NATIVE_TRIG_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { ESP_NATIVE_TRIG_COUNT=256, ESP_NATIVE_TRIG_BYTES=1024 };
/* Install exact little-endian PAK payload once; no approximation or fallback. */
int EspNativeTrig_installLe(const uint8_t* data, size_t length);
const int32_t* EspNativeTrig_view(void);
void EspNativeTrig_reset(void);
#ifdef __cplusplus
}
#endif
#endif
