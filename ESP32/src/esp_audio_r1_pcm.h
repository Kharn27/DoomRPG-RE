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

/* 128 is DAC midpoint. R1c calibrates the NEW board independently:
 * compare peaks 8/32/80 of 127 possible, up to ~63% DAC full-scale,
 * without assuming the gain of the prior board with zero-ohm resistors.
 * This is NOT a music mixer. */
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

#endif
