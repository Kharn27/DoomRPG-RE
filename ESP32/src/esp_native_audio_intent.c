#include <stdio.h>
#include <string.h>

#include "esp_native_audio_intent.h"

static EspNativeAudioIntent lastIntent;
static uint32_t intentCount;

void EspNativeAudioIntent_reset(void) {
    memset(&lastIntent, 0, sizeof(lastIntent));
    intentCount = 0U;
}

int EspNativeAudioIntent_publish(uint16_t resourceId,
                                 uint8_t flags,
                                 uint8_t priority) {
    if (resourceId == 0U) return 0;

    if (intentCount != UINT32_MAX) ++intentCount;
    lastIntent.sequence = intentCount;
    lastIntent.resourceId = resourceId;
    lastIntent.flags = flags;
    lastIntent.priority = priority;

    printf("[AUDIOINTENT] PUBLISH seq=%u resource=%u flags=%u priority=%u backend=silent order=call-order\n",
           (unsigned int)lastIntent.sequence,
           (unsigned int)lastIntent.resourceId,
           (unsigned int)lastIntent.flags,
           (unsigned int)lastIntent.priority);
    return 1;
}

const EspNativeAudioIntent* EspNativeAudioIntent_last(void) {
    return intentCount != 0U ? &lastIntent : NULL;
}

uint32_t EspNativeAudioIntent_count(void) {
    return intentCount;
}
