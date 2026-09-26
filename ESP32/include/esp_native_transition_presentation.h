#ifndef DOOMRPG_ESP32_NATIVE_TRANSITION_PRESENTATION_H
#define DOOMRPG_ESP32_NATIVE_TRANSITION_PRESENTATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct EspNativeGameplayTransitionState_s;

/*
 * Full-frame transition UI owned outside the legacy Menu/DoomCanvas state
 * machine. Stats presentation is source-map only. Loading renders the original
 * c.bmp starfield once and keeps that first frame fixed while a bounded progress
 * bar is updated. No asset read occurs after the initial loading frame.
 */
int EspNativeTransitionPresentation_showStats(
    const struct EspNativeGameplayTransitionState_s* transition);
int EspNativeTransitionPresentation_beginLoading(uint8_t targetMapId);
void EspNativeTransitionPresentation_progress(uint8_t phase,
                                              uint32_t completed,
                                              uint32_t total);

/* Generic checkpoint-load progress over the same presentation owner. These
 * calls never read assets; beginLoading() must already have succeeded. */
void EspNativeTransitionPresentation_checkpointProgress(uint8_t percent,
                                                        const char* stage);
int EspNativeTransitionPresentation_isLoadingActive(void);
void EspNativeTransitionPresentation_abortLoading(const char* reason);

/* Release a checkpoint loading owner without repainting. The next gameplay
 * present must be a complete world/HUD frame and becomes the visible handoff. */
void EspNativeTransitionPresentation_releaseLoading(const char* reason);
void EspNativeTransitionPresentation_endLoading(void);
void EspNativeTransitionPresentation_reset(void);

#ifdef __cplusplus
}
#endif

#endif
