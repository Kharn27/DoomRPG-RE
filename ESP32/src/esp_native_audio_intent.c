#include <stdio.h>
#include <string.h>

#include "esp_native_audio_intent.h"

static EspNativeAudioIntent lastIntent;
static uint32_t intentCount;
static EspNativeAudioIntent pending[ESP_NATIVE_AUDIO_INTENT_QUEUE_CAP];
static uint8_t pendingHead;
static uint8_t pendingCount;

static int enqueue(EspNativeAudioIntent intent) {
    uint8_t tail;
    if (pendingCount >= ESP_NATIVE_AUDIO_INTENT_QUEUE_CAP) return 0;
    tail = (uint8_t)((pendingHead + pendingCount) %
                     ESP_NATIVE_AUDIO_INTENT_QUEUE_CAP);
    pending[tail] = intent;
    ++pendingCount;
    return 1;
}

int EspNativeAudioIntent_takeNext(EspNativeAudioIntent* outIntent) {
    if (outIntent == NULL || pendingCount == 0U) return 0;
    *outIntent = pending[pendingHead];
    pendingHead = (uint8_t)((pendingHead + 1U) %
                            ESP_NATIVE_AUDIO_INTENT_QUEUE_CAP);
    --pendingCount;
    return 1;
}

void EspNativeAudioIntent_reset(void) {
    memset(&lastIntent, 0, sizeof(lastIntent));
    intentCount = 0U;
    pendingHead = 0U;
    pendingCount = 0U;
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

    if (!enqueue(lastIntent)) {
        printf("[AUDIOINTENT] DROP resource=%u reason=fifo-full gameplay=unaffected\n",
               (unsigned int)resourceId);
        return 0;
    }
    printf("[AUDIOINTENT] PUBLISH seq=%u resource=%u flags=%u priority=%u backend=queued order=call-order\n",
           (unsigned int)lastIntent.sequence,
           (unsigned int)lastIntent.resourceId,
           (unsigned int)lastIntent.flags,
           (unsigned int)lastIntent.priority);
    return 1;
}

/* Stop is a command, not an audio resource. Retain the same 8-byte record
 * and monotonic call-order sequence so a future backend can consume it. */
int EspNativeAudioIntent_stopAll(void) {
    if (intentCount != UINT32_MAX) ++intentCount;
    lastIntent.sequence = intentCount;
    lastIntent.resourceId = 0U;
    lastIntent.flags = ESP_NATIVE_AUDIO_STOP_ALL_FLAG;
    lastIntent.priority = 0U;

    if (pendingCount == ESP_NATIVE_AUDIO_INTENT_QUEUE_CAP) {
        pendingHead = 0U; /* STOP is authoritative even under saturation. */
        pendingCount = 0U;
    }
    (void)enqueue(lastIntent);
    printf("[AUDIOINTENT] STOP_ALL seq=%u backend=queued order=call-order\n",
           (unsigned int)lastIntent.sequence);
    return 1;
}

const EspNativeAudioIntent* EspNativeAudioIntent_last(void) {
    return intentCount != 0U ? &lastIntent : NULL;
}

uint32_t EspNativeAudioIntent_count(void) {
    return intentCount;
}
