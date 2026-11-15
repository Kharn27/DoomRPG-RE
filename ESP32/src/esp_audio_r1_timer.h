#ifndef ESP_AUDIO_R1_TIMER_H
#define ESP_AUDIO_R1_TIMER_H

/* AUDIO-R1k temporary experiment, strictly GPIO26 DAC2.
 * Compare direct timer-sampled PCM against I2S and native DAC cosine. */
void EspAudioR1_runTimerColdBootProbe(void);

/* R1l: nonblocking 8-second sine test while real gameplay/touch/render run.
 * One attempt per boot; service must run every main loop iteration to enforce
 * fail-closed shutdown if the native gameplay session ends. */
void EspAudioR1_timerTryStartInGameplay(void);
void EspAudioR1_timerService(bool gameplayActive);

#endif
