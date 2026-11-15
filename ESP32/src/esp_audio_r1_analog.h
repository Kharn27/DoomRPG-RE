#ifndef ESP_AUDIO_R1_ANALOG_H
#define ESP_AUDIO_R1_ANALOG_H

/* AUDIO-R1 one-shot analog diagnostic only. The existing silent native
 * AUDIOINTENT owner stays untouched. No SD, PAK or gameplay state access. */
/* R1g diagnostic: play the same 8s 440Hz waveform BEFORE SD, TFT or
 * Doom RPG startup. The normal firmware continues booting after the probe
 * and does not repeat playback in gameplay on the same boot. */
void EspAudioR1_runColdBootProbe(void);
void EspAudioR1_tryStartInGameplay(void);
void EspAudioR1_requestStop(void);
void EspAudioR1_poll(void);

#endif
