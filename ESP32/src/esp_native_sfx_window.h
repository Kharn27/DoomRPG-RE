#ifndef ESP_NATIVE_SFX_WINDOW_H
#define ESP_NATIVE_SFX_WINDOW_H
#include <stdint.h>

/* AUDIO #382: consume ALL published future samples while a PAK PCM voice
 * is active. A fixed 8192-frame limit stranded a ready tail between main
 * services, allowing the DAC reader to catch up after a long visual frame.
 *
 * This only sizes one main-task mix; no ring, rate, timer, ISR, SD reader,
 * audio storage, or game state is added. Never mix unpublished/overwritten
 * ring memory. The caller retains its live 128-frame DAC reader guard. */
static inline uint32_t EspNativeSfxPcm_publishedSpan(
        uint32_t first, uint32_t writer, uint32_t remaining,
        uint32_t capacity) {
    if (remaining==0U || capacity==0U ||
        (int32_t)(writer-first)<=0) return 0U;
    const uint32_t published=writer-first;
    if (published>capacity) return 0U; /* invalid/overwritten window */
    return published<remaining?published:remaining;
}
#endif
