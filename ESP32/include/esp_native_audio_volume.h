#ifndef ESP_NATIVE_AUDIO_VOLUME_H
#define ESP_NATIVE_AUDIO_VOLUME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Independent tiny SD preference, safely leaves native video v3 config intact.
 * Boot never writes, and save commits before changing live I2S level. */
void EspNativeAudioVolume_loadAtBoot(int sdMounted);
int EspNativeAudioVolume_savePercent(uint8_t percent);
#ifdef __cplusplus
}
#endif
#endif
