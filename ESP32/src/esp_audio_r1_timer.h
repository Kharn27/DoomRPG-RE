#ifndef ESP_AUDIO_R1_TIMER_H
#define ESP_AUDIO_R1_TIMER_H

/* AUDIO-R1k temporary experiment, strictly GPIO26 DAC2.
 * Compare direct timer-sampled PCM against I2S and native DAC cosine. */
void EspAudioR1_runTimerColdBootProbe(void);

#endif
