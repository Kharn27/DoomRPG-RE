#ifndef ESP_AUDIO_ENTRANCE_MUSIC_H
#define ESP_AUDIO_ENTRANCE_MUSIC_H
/* AUDIO-R2a: IMA-ADPCM WAV 22050Hz M_STALKS on SD in Entrance.
 * The service runs once per Arduino main-loop iteration; ISR uses DRAM only.
 * Output is DAC2 GPIO26; GPIO25 remains reserved for touch. */
void EspEntranceMusic_service(bool gameplayActive,bool sdReady);
#endif
