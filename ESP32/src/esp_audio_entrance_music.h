#ifndef ESP_AUDIO_ENTRANCE_MUSIC_H
#define ESP_AUDIO_ENTRANCE_MUSIC_H
#include <stdbool.h>
#include <stdint.h>
/* AUDIO-R2d: Entrance SD music, 22.05kHz IMA-ADPCM -> DAC2/GPIO26.
 * One synchronous SD owner in Arduino gameplay task; IRAM ISR is DRAM-only.
 * The cooperative pump is ONLY for completed renderer-frame boundaries after
 * native PAK has closed. It never owns gameplay, input, video or the bus. */
#ifdef __cplusplus
extern "C" {
#endif
/* Hold DAC2/GPIO26 at digital silence (128) already in the main menu when
 * SOUND is ON. No timer, SD, music ring, ISR, allocations or music playback.
 * SOUND OFF gates and disables the idle DAC. Safe only on Arduino main task. */
void EspEntranceMusic_syncIdleOutput(void);
/* Final ST_INTRO Continue: close M_IN_CIT WAV/SD/ring BEFORE BSP/map load. */
void EspEntranceMusic_stopIntroAtHandoff(void);
/* I3: called only from confirmed new-game ST_INTRO after story assets
 * prepared and BEFORE first ST_INTRO frame. Starts the existing single
 * IMA-ADPCM reader, with its verified clock preflight, on this Arduino task.
 * Returns 1 if music is already playing; 0 is a silent nonfatal fallback.
 * Never allocate a second ring, hold native PAK open or touch renderer. */
int EspEntranceMusic_prestartIntroBeforeFirstFrame(void);
/* LOAD-L1: stop any prior level music BEFORE the next loading screen/PACK
 * reconfiguration. Main Arduino task only; no second SD owner or ISR. */
void EspEntranceMusic_stopBeforeLoading(void);
/* MENU-M1: close title SD/ring/timer only for real game handoff. */
void EspEntranceMusic_stopMenuBeforeAction(void);
/* MENU-M1: synchronous bounded prestart before FIRST physical title present. */
int EspEntranceMusic_prestartMenuBeforeFirstFrame(void);
/* LOAD-L1: one-time bounded Entrance music prestart after all map/cache/HUD
 * work, while the physical 100% READY loading frame is still visible.
 * Invoked immediately before first gameplay present. No fallback track for
 * unknown map IDs; 0 means silent/deferred safe fallback. */
int EspEntranceMusic_prestartGameplayBeforeFirstFrame(uint8_t targetMapId);
void EspEntranceMusic_service(bool gameplayActive,bool sdReady);
void EspEntranceMusic_pumpGameplay(void);
#ifdef __cplusplus
}
#endif
#endif
