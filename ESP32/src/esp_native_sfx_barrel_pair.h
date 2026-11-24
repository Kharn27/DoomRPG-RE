#ifndef ESP_NATIVE_SFX_BARREL_PAIR_H
#define ESP_NATIVE_SFX_BARREL_PAIR_H
#include <stdint.h>

/* One committed barrel shot, two independent sounds: finish the original
 * priority-5 weapon WAV before the priority-4 explosion begins.  The PCM
 * mixer retains only absolute DAC indices.  A future start is legal because
 * the existing progressive voice waits for ring publication; no sleep,
 * heap, extra PCM queue, music reader, ISR modification or world mutation.
 * A sequence gap means an unrelated 5061 keeps its normal starting time. */
static inline uint32_t EspNativeSfxBarrelPair_explosionStart(
        uint32_t normalStart, uint32_t gunEnd,
        uint32_t gunSequence, uint32_t explosionSequence) {
    if (gunSequence == 0U || gunSequence == UINT32_MAX ||
        explosionSequence != gunSequence + 1U)
        return normalStart;
    return (int32_t)(gunEnd - normalStart) > 0
        ? gunEnd : normalStart;
}
#endif
