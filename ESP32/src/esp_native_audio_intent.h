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

#ifdef __cplusplus
}
#endif

#endif
