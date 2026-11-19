#include <stdio.h>
#include <string.h>

#include "esp_native_audio_intent.h"

static EspNativeAudioIntent lastIntent;
static uint32_t intentCount;
#define ESP_NATIVE_AUDIO_QUEUE_CAP 16U
static EspNativeAudioIntent intentQueue[ESP_NATIVE_AUDIO_QUEUE_CAP];
static uint8_t intentQueueFirst;
static uint8_t intentQueueCount;
static EspNativeAudioIntentStats intentStats;
static void addCount(uint32_t* value,uint32_t amount) {
    if (*value>UINT32_MAX-amount) *value=UINT32_MAX;
    else *value+=amount;
}
static void retainIntent(void) {
    if (intentQueueCount==ESP_NATIVE_AUDIO_QUEUE_CAP) {
        intentQueueFirst=(uint8_t)((intentQueueFirst+1U)%ESP_NATIVE_AUDIO_QUEUE_CAP);
        --intentQueueCount;
        addCount(&intentStats.queueOverflow,1U);
        printf("[AUDIOINTENT] OVERFLOW drop=oldest capacity=16 "
               "evictedSeq=%u queueOverflow=%u\n",
               (unsigned)intentQueue[(intentQueueFirst+
                   ESP_NATIVE_AUDIO_QUEUE_CAP-1U)%ESP_NATIVE_AUDIO_QUEUE_CAP].sequence,
               (unsigned)intentStats.queueOverflow);
    }
    intentQueue[(intentQueueFirst+intentQueueCount)%ESP_NATIVE_AUDIO_QUEUE_CAP]=lastIntent;
    ++intentQueueCount;
}
/* Audio music output preference, independent of incidental sound intents.
 * Runtime-only until native config evolves beyond its validated V3 ABI. */
static uint8_t musicVolumePercent=100U;
static uint8_t soundEnabled=1U; /* boot default ON, V3 config unchanged */

void EspNativeAudioIntent_reset(void) {
    memset(&lastIntent, 0, sizeof(lastIntent));
    intentCount = 0U;
    intentQueueFirst=0U;
    intentQueueCount=0U;
    memset(&intentStats,0,sizeof(intentStats));
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
    retainIntent();

    printf("[AUDIOINTENT] PUBLISH seq=%u resource=%u flags=%u priority=%u backend=silent order=call-order\n",
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
    retainIntent();

    printf("[AUDIOINTENT] STOP_ALL seq=%u backend=silent order=call-order\n",
           (unsigned int)lastIntent.sequence);
    return 1;
}

const EspNativeAudioIntent* EspNativeAudioIntent_last(void) {
    return intentCount != 0U ? &lastIntent : NULL;
}

int EspNativeAudioIntent_nextAfter(uint32_t sequence,
                                   EspNativeAudioIntent* outIntent) {
    if (outIntent==NULL) return 0;
    for (uint8_t i=0U;i<intentQueueCount;++i) {
        const EspNativeAudioIntent* item=
            &intentQueue[(intentQueueFirst+i)%ESP_NATIVE_AUDIO_QUEUE_CAP];
        if (item->sequence>sequence) {
            *outIntent=*item;
            return 1;
        }
    }
    return 0;
}

uint32_t EspNativeAudioIntent_count(void) {
    return intentCount;
}

/* The producer and its sole consumer execute on the Arduino main task.
 * Reading nextAfter() is intentionally side-effect-free: the consumer may
 * defer for a PAK lease and must not lose its cursor until actually handled. */
static void releaseThrough(uint32_t sequence,int handoff) {
    uint32_t cleared=0U;
    while (intentQueueCount!=0U &&
           intentQueue[intentQueueFirst].sequence<=sequence) {
        intentQueueFirst=(uint8_t)((intentQueueFirst+1U)%
                                     ESP_NATIVE_AUDIO_QUEUE_CAP);
        --intentQueueCount;
        ++cleared;
    }
    if (handoff) addCount(&intentStats.handoffDiscarded,cleared);
    else addCount(&intentStats.consumed,cleared);
}

void EspNativeAudioIntent_acknowledgeThrough(uint32_t sequence) {
    releaseThrough(sequence,0);
}

void EspNativeAudioIntent_discardThrough(uint32_t sequence) {
    releaseThrough(sequence,1);
}

void EspNativeAudioIntent_noteConsumerGap(uint32_t before,
                                          uint32_t received) {
    if (received<=before || received-before<=1U) return;
    const uint32_t missed=received-before-1U;
    addCount(&intentStats.consumerCursorGap,missed);
    printf("[AUDIOINTENT] CURSOR-GAP after=%u next=%u missed=%u "
           "consumerCursorGap=%u\n",(unsigned)before,(unsigned)received,
           (unsigned)missed,(unsigned)intentStats.consumerCursorGap);
}

void EspNativeAudioIntent_notePlaybackDrop(void) {
    addCount(&intentStats.audibleSfxDropped,1U);
}

void EspNativeAudioIntent_getStats(EspNativeAudioIntentStats* out) {
    if (out==NULL) return;
    *out=intentStats;
    out->pending=intentQueueCount;
}

uint8_t EspNativeAudioIntent_musicVolumePercent(void) {
    return musicVolumePercent;
}

int EspNativeAudioIntent_setMusicVolumePercent(uint8_t percent) {
    if (percent!=0U && percent!=25U && percent!=50U &&
        percent!=75U && percent!=100U) return 0;
    musicVolumePercent=percent;
    printf("[AUDIOVOLUME] SET percent=%u persistence=boot-session configV3=unchanged\n",
           (unsigned)percent);
    return 1;
}

int EspNativeAudioIntent_isSoundEnabled(void) {
    return soundEnabled != 0U;
}

int EspNativeAudioIntent_setSoundEnabled(int enabled) {
    if (enabled!=0 && enabled!=1) return 0;
    const uint8_t before=soundEnabled;
    soundEnabled=(uint8_t)enabled;
    printf("[AUDIOSWITCH] SET %s->%s volumeKept=%u persistence=boot-session configV3=unchanged\n",
           before?"ON":"OFF",soundEnabled?"ON":"OFF",
           (unsigned)musicVolumePercent);
    return 1;
}
