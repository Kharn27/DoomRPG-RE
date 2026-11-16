#ifndef ESP_NATIVE_SFX_PCM_MIX_H
#define ESP_NATIVE_SFX_PCM_MIX_H
#include <stdint.h>

/* SFX-P1: pure deterministic nearest-neighbor 8k->22050 mono DAC overlay.
 * Only short SFX fitting completely into unconsumed music ring are admitted.
 * No heap, SDL, float, SD or WAV decoding during the timer ISR. */
static inline uint32_t EspNativeSfxPcm_outputFrames(uint32_t inputFrames,
                                                     uint32_t outputRate) {
    if (inputFrames==0U || outputRate==0U) return 0U;
    const uint64_t n=(uint64_t)inputFrames*outputRate+7999ULL;
    const uint64_t out=n/8000ULL;
    return out<=UINT32_MAX?(uint32_t)out:0U;
}
static inline int16_t EspNativeSfxPcm_le16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));
}
static inline uint8_t EspNativeSfxPcm_mix(uint8_t music,int16_t sfx) {
    /* Moderate SFX gain, mixed on main producer into already staged DAC8.
     * Preserve DAC rail reserve established by R2d/R2e: 16..240. */
    int32_t code=(int32_t)music+(int32_t)sfx/512;
    if (code<16) code=16;
    if (code>240) code=240;
    return (uint8_t)code;
}
#endif
