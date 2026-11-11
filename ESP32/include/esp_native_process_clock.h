#ifndef DOOMRPG_ESP32_NATIVE_PROCESS_CLOCK_H
#define DOOMRPG_ESP32_NATIVE_PROCESS_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Arduino millis() parity: milliseconds since boot, truncated to uint32_t. */
uint32_t EspNativeProcessClock_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
