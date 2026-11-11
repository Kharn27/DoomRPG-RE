#ifndef ESP_NATIVE_AUDIO_PCM_MATH_H
#define ESP_NATIVE_AUDIO_PCM_MATH_H

#include <stdint.h>

/* AUDIO-A: all arithmetic is bounded and allocation-free. Keep Q8 until
 * AFTER master volume, otherwise 25% on the 8-bit ESP32 DAC quantizes each
 * voice separately and causes avoidable stair-step distortion. */
static inline int32_t EspNativeAudioPcm_voiceQ8(
    int32_t wave, uint8_t gain, int32_t envelope) {
    return (wave * (int32_t)gain * envelope) / 512;
}

/* A symmetrical 4:1 soft knee prevents 12 real MIDI voices from instantly
 * hard-clipping to an 8-bit rail. Values past the knee are compressed before
 * the final defensive clamp. */
static inline int32_t EspNativeAudioPcm_scaleLimitQ8(
    int32_t sumQ8, uint32_t masterPercent,
    uint8_t* limited, uint8_t* clipped) {
    const int32_t knee = 96 * 256;
    const int32_t rail = 127 * 256;
    int32_t value = (sumQ8 * (int32_t)masterPercent) / 100;
    uint8_t compressed = 0;
    uint8_t saturated = 0;
    if (value > knee) {
        value = knee + (value - knee) / 4;
        compressed = 1;
    } else if (value < -knee) {
        value = -knee + (value + knee) / 4;
        compressed = 1;
    }
    if (value > rail) {
        value = rail;
        saturated = 1;
    } else if (value < -rail) {
        value = -rail;
        saturated = 1;
    }
    if (limited) *limited = compressed;
    if (clipped) *clipped = saturated;
    return value;
}

/* ONE LAST listening-only software attenuation test for the CYD 8-ohm
 * speaker with incorrect R7/R8 gain network. 8 seconds each at 2%, 5%,
 * then 10% of full digital scale, reverting to a safe 2% after 24 s.
 * Deterministic sample clock (not wall time), untouched MIDI tempo/events.
 * The saved user Sound preset is an upper bound, never overwritten. */
#define ESP_NATIVE_AUDIO_GAIN_TEST_RATE 16000U
#define ESP_NATIVE_AUDIO_GAIN_TEST_STAGE_SAMPLES (8U * ESP_NATIVE_AUDIO_GAIN_TEST_RATE)
static inline uint8_t EspNativeAudioPcm_gainTestPhase(uint32_t sample) {
    const uint32_t stage = sample / ESP_NATIVE_AUDIO_GAIN_TEST_STAGE_SAMPLES;
    return (uint8_t)(stage > 3U ? 3U : stage);
}
static inline uint8_t EspNativeAudioPcm_gainTestPercent(uint32_t sample,
                                                         uint32_t masterPercent) {
    static const uint8_t percent[4] = {2U, 5U, 10U, 2U};
    const uint32_t pct = percent[EspNativeAudioPcm_gainTestPhase(sample)];
    return (uint8_t)(masterPercent < pct ? masterPercent : pct);
}

static inline uint16_t EspNativeAudioPcm_toDacWord(int32_t q8) {
    const int32_t rounded = q8 >= 0
        ? (q8 + 128) / 256 : -((-q8 + 128) / 256);
    const int32_t sample = rounded + 128;
    return (uint16_t)(sample << 8);
}

#endif
