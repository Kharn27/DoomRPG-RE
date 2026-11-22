#ifndef ESP_AUDIO_PICKUP_MODAL_GATE_H
#define ESP_AUDIO_PICKUP_MODAL_GATE_H

#include <stdint.h>
#include "esp_native_audio_intent.h"

/* AUDIO-PICKUP #419: local, allocation-free policy for a single 5060 cue.
 * A pickup may only hand off its *own* next FIFO record, with enough already
 * published music samples to mix its entire original PCM before the modal
 * takes the PAK lease. No cursor advance, SD/flash I/O, ring write or new
 * voice happens inside this predicate. Failure leaves the FIFO untouched. */
typedef struct EspAudioPickupModalGate_s {
    const EspNativeAudioIntent* next;
    uint32_t expectedSequence;
    uint32_t consumerSequence;
    uint32_t reader;
    uint32_t writer;
    uint32_t ringCapacity;
    uint32_t outputFrames;
    uint32_t leadFrames;
    uint8_t playbackReady;
    uint8_t packFree;
    uint8_t voicesIdle;
} EspAudioPickupModalGate;

static inline int EspAudioPickupModal_canMix(
    const EspAudioPickupModalGate* gate) {
    if (gate == 0 || gate->next == 0 ||
        gate->playbackReady == 0U || gate->packFree == 0U ||
        gate->voicesIdle == 0U ||
        gate->expectedSequence == 0U ||
        gate->consumerSequence == UINT32_MAX ||
        gate->next->sequence != gate->expectedSequence ||
        gate->next->sequence != gate->consumerSequence + 1U ||
        gate->next->resourceId != 5060U || gate->next->flags != 0U ||
        gate->reader > gate->writer ||
        gate->ringCapacity == 0U ||
        gate->writer - gate->reader > gate->ringCapacity ||
        gate->outputFrames == 0U ||
        gate->leadFrames >= gate->ringCapacity ||
        gate->outputFrames > gate->ringCapacity - gate->leadFrames) {
        return 0;
    }
    return gate->writer - gate->reader >=
           gate->outputFrames + gate->leadFrames;
}

#endif
