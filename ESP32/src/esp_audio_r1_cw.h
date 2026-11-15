#ifndef ESP_AUDIO_R1_CW_H
#define ESP_AUDIO_R1_CW_H

/* Temporary AUDIO-R1h: hardware cosine 440Hz on DAC2/GPIO26.
 * Called only at cold boot after R1g I2S has been uninstalled.
 * No I2S, DMA, speaker rewire, SD, TFT or native gameplay touched. */
void EspAudioR1_runCosineColdBootProbe(void);

#endif
