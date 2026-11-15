#ifndef ESP_NATIVE_AUDIO_INTENT_H
#define ESP_NATIVE_AUDIO_INTENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeAudioIntent_s {
    uint32_t sequence;
    uint16_t resourceId;
    uint8_t flags;
    uint8_t priority;
} EspNativeAudioIntent;

void EspNativeAudioIntent_reset(void);
/* Record code 0 + bit 7 is a STOP_ALL command, never a playable asset. */
#define ESP_NATIVE_AUDIO_STOP_ALL_FLAG 0x80U

int EspNativeAudioIntent_publish(uint16_t resourceId,
                                 uint8_t flags,
                                 uint8_t priority);
int EspNativeAudioIntent_stopAll(void);
const EspNativeAudioIntent* EspNativeAudioIntent_last(void);
uint32_t EspNativeAudioIntent_count(void);
/* Shared compact music volume owner: 0/25/50/75/100, boot default 100.
 * C UI sets it; native ADPCM producer reads it. No ISR or SD writes.
 * This is explicitly runtime-only, no silent V3 config schema change. */
uint8_t EspNativeAudioIntent_musicVolumePercent(void);
int EspNativeAudioIntent_setMusicVolumePercent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
