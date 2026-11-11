#ifndef ESP_BT_TEST_MIDI_H
#define ESP_BT_TEST_MIDI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Streams SMF note/tempo events from the native PAK on the Arduino main
 * thread; the A2DP PCM callback NEVER touches SD, PAK, heap or Serial.
 * Timbre: bounded 12-voice triangle synth, NOT desktop FluidSynth/SF2. */
int EspBtTestMidi_begin(void);
void EspBtTestMidi_pump(void);
int32_t EspBtTestMidi_fill(uint8_t *pcm, int32_t len);
int EspBtTestMidi_isReady(void);
void EspBtTestMidi_report(void);
#ifdef __cplusplus
}
#endif
#endif
