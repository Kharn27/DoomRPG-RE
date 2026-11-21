#ifndef ESP_AUDIO_ENTRANCE_HEAP_GUARD_H
#define ESP_AUDIO_ENTRANCE_HEAP_GUARD_H

#include <stdint.h>

/* One shared 16 KiB INTERNAL|8BIT SD/ADPCM/DAC ring. No second allocation.
 *
 * AUDIO-LOAD V11 CYD witness: NEW GAME has 49388 free bytes after the ring;
 * restored LOAD has 48568. The former 49152-byte post-ring floor rejected
 * valid LOAD by just 584 bytes despite both pre-ring totals and contiguous
 * blocks satisfying the existing preflight and the gameplay RAM budget.
 *
 * Retain the original 60 KiB preflight and 16 KiB largest-block guards.
 * A bounded 46 KiB post-ring free floor still leaves >=32 KiB general
 * headroom plus >=14 KiB additional free 8-bit RAM. Reject low-memory
 * systems and failed INTERNAL ring allocation before starting the timer.
 * This policy has no state, heap, PAK access, or ISR responsibilities.
 */
enum {
    ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 = 61440U,
    ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8 = 24576U,
    ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 47104U,
    ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 16384U
};

static inline int EspAudioMusic_heapBeforeRingOk(uint32_t free8,
                                                  uint32_t largest8) {
    return free8 >= ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 &&
           largest8 >= ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8;
}

static inline int EspAudioMusic_heapAfterRingOk(int ringAllocated,
                                                 uint32_t free8,
                                                 uint32_t largest8) {
    return ringAllocated &&
           free8 >= ESP_AUDIO_MUSIC_POST_MIN_HEAP8 &&
           largest8 >= ESP_AUDIO_MUSIC_POST_MIN_LARGEST8;
}

#endif
