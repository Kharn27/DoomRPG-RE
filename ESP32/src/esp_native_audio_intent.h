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
int EspNativeAudioIntent_publish(uint16_t resourceId,
                                 uint8_t flags,
                                 uint8_t priority);
const EspNativeAudioIntent* EspNativeAudioIntent_last(void);
uint32_t EspNativeAudioIntent_count(void);

#ifdef __cplusplus
}
#endif

#endif
