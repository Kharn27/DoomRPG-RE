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
/* Bounded FIFO witness for consecutive events in one turn, without losing
 * weapon+pain or three monster-attack shots to last-intent overwrite. */
int EspNativeAudioIntent_nextAfter(uint32_t sequence,
                                   EspNativeAudioIntent* outIntent);
uint32_t EspNativeAudioIntent_count(void);

/* One playback consumer owns the cursor. A peek never removes a record.
 * ACK only AFTER processing the intent, so a PAK/SD lease deferral cannot
 * accidentally consume it; batch order and STOP_ALL remain unchanged. */
void EspNativeAudioIntent_acknowledgeThrough(uint32_t sequence);
/* Intents from a prior soundtrack/session must not replay on the next map.
 * Record these explicit skips separately from SFX playback failures. */
void EspNativeAudioIntent_discardThrough(uint32_t sequence);
void EspNativeAudioIntent_noteConsumerGap(uint32_t before, uint32_t received);
void EspNativeAudioIntent_notePlaybackDrop(void);
typedef struct EspNativeAudioIntentStats_s {
    uint32_t queueOverflow;       /* evicted while still pending, never hidden */
    uint32_t consumerCursorGap;   /* sequence IDs skipped by active consumer */
    uint32_t audibleSfxDropped;   /* rejected/partial PCM mixes, not DAC proof */
    uint32_t consumed;            /* acknowledged by the playback consumer */
    uint32_t handoffDiscarded;    /* intentionally skipped at track boundary */
    uint8_t pending;
} EspNativeAudioIntentStats;
void EspNativeAudioIntent_getStats(EspNativeAudioIntentStats* out);

/* Shared compact music volume owner: 0/25/50/75/100, boot default 100.
 * C UI sets it; native ADPCM producer reads it. No ISR or SD writes.
 * This is explicitly runtime-only, no silent V3 config schema change. */
uint8_t EspNativeAudioIntent_musicVolumePercent(void);
int EspNativeAudioIntent_setMusicVolumePercent(uint8_t percent);
/* Master SOUND toggle, boot-session only; default ON. Unlike volume=0,
 * OFF must explicitly silence all music/SFX while retaining volume preset.
 * Native config V3 is deliberately unchanged. */
int EspNativeAudioIntent_isSoundEnabled(void);
int EspNativeAudioIntent_setSoundEnabled(int enabled);

#ifdef __cplusplus
}
#endif

#endif
