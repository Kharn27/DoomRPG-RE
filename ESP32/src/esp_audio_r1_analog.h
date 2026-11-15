#ifndef ESP_AUDIO_R1_ANALOG_H
#define ESP_AUDIO_R1_ANALOG_H

/* AUDIO-R1 one-shot analog diagnostic only. The existing silent native
 * AUDIOINTENT owner stays untouched. No SD, PAK or gameplay state access. */
void EspAudioR1_tryStartInGameplay(void);
void EspAudioR1_requestStop(void);
void EspAudioR1_poll(void);

#endif
