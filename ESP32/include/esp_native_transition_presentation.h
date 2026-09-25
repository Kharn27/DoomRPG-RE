#ifndef DOOMRPG_ESP32_NATIVE_TRANSITION_PRESENTATION_H
#define DOOMRPG_ESP32_NATIVE_TRANSITION_PRESENTATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct EspNativeGameplayTransitionState_s;

/*
 * Full-frame transition UI owned outside the legacy Menu/DoomCanvas state
 * machine. Stats presentation is source-map only; loading presentation keeps
 * one bounded indexed-BMP descriptor for the original c.bmp starfield and
 * streams its pixels from whichever PAK lease is already authoritative.
 */
int EspNativeTransitionPresentation_showStats(
    const struct EspNativeGameplayTransitionState_s* transition);
int EspNativeTransitionPresentation_beginLoading(uint8_t targetMapId);
void EspNativeTransitionPresentation_progress(uint8_t phase,
                                              uint32_t completed,
                                              uint32_t total);
void EspNativeTransitionPresentation_endLoading(void);
void EspNativeTransitionPresentation_reset(void);

#ifdef __cplusplus
}
#endif

#endif
