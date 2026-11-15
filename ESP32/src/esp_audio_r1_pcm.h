#ifndef ESP_AUDIO_R1_PCM_H
#define ESP_AUDIO_R1_PCM_H

#include <stdint.h>

/* AUDIO-R1 only: tiny 8-bit sine lookup in read-only flash, no heap. The
 * classic ESP32's internal DAC consumes the HIGH byte of each 16-bit slot.
 * Explicit full stereo I2S frames are sent; only DAC2 / GPIO26 is enabled. */
static const int8_t kEspAudioR1Sine256[256] = {
       0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
      49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
      90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
     117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
     127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
     117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
      90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
      49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
       0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
     -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
     -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
    -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
    -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
    -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
     -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
     -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3
};

/* DAC midpoint 128. This diagnostic keeps the R1c legacy quantizer intact
 * for a direct A/B test against an independent 8-bit reconstruction path.
 * Hard limit remains +/-80 of 127 possible codes; no heap or music mixer. */
static inline uint8_t EspAudioR1Pcm_dacCode(uint32_t phase, uint8_t peakCodes) {
    int32_t delta;
    if (peakCodes > 80U) peakCodes = 80U;
    delta = ((int32_t)kEspAudioR1Sine256[phase >> 24] *
             (int32_t)peakCodes) / 127;
    return (uint8_t)(128 + delta);
}

static inline uint16_t EspAudioR1Pcm_word(uint32_t phase, uint8_t peakCodes) {
    return (uint16_t)((uint16_t)EspAudioR1Pcm_dacCode(phase, peakCodes) << 8);
}

/* R1d reference: interpolate the 256-entry sine before quantizing to the
 * same 8-bit DAC, then feed back the 1-LSB quantizer error at each sample.
 * This first-order error-feedback path is NOT extra DAC resolution. It is a
 * bounded A/B diagnostic for signal-correlated distortion on real hardware.
 * Only one state record per producer, no randomness or dynamic allocation. */
typedef struct EspAudioR1PcmShapeState {
    int32_t errorQ16;
} EspAudioR1PcmShapeState;

static inline uint16_t EspAudioR1Pcm_shapedWord(
    uint32_t phase, uint8_t peakCodes, EspAudioR1PcmShapeState* state) {
    const uint32_t index = phase >> 24;
    const int32_t a = (int32_t)kEspAudioR1Sine256[index];
    const int32_t b = (int32_t)kEspAudioR1Sine256[(index + 1U) & 255U];
    const int32_t frac = (int32_t)((phase >> 8) & 65535U);
    if (peakCodes > 80U) peakCodes = 80U;
    /* Signed multiplication rather than left shift (negative-shift UB).
     * All intermediate bounds stay strictly within signed 32-bit:
     * |a| <=127, |b-a| <=4, |interp| <= 127*65536,
     * |interp*peak| <= 127*65536*80. */
    const int32_t interpolatedQ16 = a * 65536 + (b - a) * frac;
    const int32_t targetQ16 =
        128 * 65536 + (interpolatedQ16 * (int32_t)peakCodes) / 127;
    const int32_t corrected = targetQ16 + state->errorQ16;
    const int32_t quantized = (corrected + 32768) / 65536;
    state->errorQ16 = corrected - quantized * 65536;
    return (uint16_t)((uint16_t)quantized << 8);
}

#endif
