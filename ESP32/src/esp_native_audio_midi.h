#ifndef ESP_NATIVE_AUDIO_MIDI_H
#define ESP_NATIVE_AUDIO_MIDI_H
#ifdef __cplusplus
extern "C" {
#endif
/* Loop-thread PAK MIDI pump; actual 16kHz DAC output lives in an independent
 * I2S/DMA task pinned to core 0. Called AFTER touch polling, once per loop. */
void EspNativeAudioMidi_service(void);
/* Safe before audio startup and while playing; only 25,50,75,100 accepted. */
uint8_t EspNativeAudioMidi_getVolume(void);
int EspNativeAudioMidi_setVolume(uint8_t percent);
#ifdef __cplusplus
}
#endif
#endif
