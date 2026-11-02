#ifndef DOOMRPG_ESP32_NATIVE_CONFIG_H
#define DOOMRPG_ESP32_NATIVE_CONFIG_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Native SD-backed preferences, independent of legacy Config, SAVE V11 and
 * PAK. Boot defaults to 100% brightness/NORMAL and never writes defaults. */
void EspNativeConfig_loadAtBoot(int sdMounted);

/* Durable verified v2 transaction (tmp -> backup -> primary, readback).
 * Neither function changes hardware; callers apply hardware only after
 * successful persistence. Settings preserve the other current preference. */
int EspNativeConfig_saveBrightness(uint8_t percent);
int EspNativeConfig_saveOrientation(uint8_t orientation);

#ifdef __cplusplus
}
#endif
#endif
