#ifndef DOOMRPG_ESP32_NATIVE_CONFIG_H
#define DOOMRPG_ESP32_NATIVE_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SD-backed CYD settings, separate from desktop "Config", game saves and PAK.
 * First boot without a file is normal: display remains at 100% and nothing
 * is written until the user actually changes a setting. Validity is checked
 * before applying a persisted PWM value. */
void EspNativeConfig_loadAtBoot(int sdMounted);

/* Commit one setting as a verified 12-byte file via tmp/backup rename.
 * Returns 0 without changing PWM if persistence cannot be guaranteed.
 * Caller applies the hardware value after a successful write. */
int EspNativeConfig_saveBrightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
