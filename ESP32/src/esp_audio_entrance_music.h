#ifndef ESP_AUDIO_ENTRANCE_MUSIC_H
#define ESP_AUDIO_ENTRANCE_MUSIC_H
#include <stdbool.h>
/* AUDIO-R2d: Entrance SD music, 22.05kHz IMA-ADPCM -> DAC2/GPIO26.
 * One synchronous SD owner in Arduino gameplay task; IRAM ISR is DRAM-only.
 * The cooperative pump is ONLY for completed renderer-frame boundaries after
 * native PAK has closed. It never owns gameplay, input, video or the bus. */
#ifdef __cplusplus
extern "C" {
#endif
void EspEntranceMusic_service(bool gameplayActive,bool sdReady);
void EspEntranceMusic_pumpGameplay(void);
#ifdef __cplusplus
}
#endif
#endif
