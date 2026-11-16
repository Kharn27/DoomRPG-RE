#ifndef ESP_AUDIO_CLOCK_GUARD_H
#define ESP_AUDIO_CLOCK_GUARD_H
#include <stdint.h>

/* AUDIO-R2f: pure hardware-rate guard calculation.
 * Count and elapsed time MUST describe the same timer interval, NOT
 * a tick counter stopped at a target plus a delayed main-loop timestamp.
 * This function is called only in the Arduino task, never in the ISR.
 * Zero / non-representable measurements fail closed. */
static inline uint32_t EspAudioClockGuard_rate(uint32_t ticks,
                                               uint64_t elapsedUs) {
    if (ticks==0U || elapsedUs==0ULL) return 0U;
    const uint64_t hz=((uint64_t)ticks*1000000ULL)/elapsedUs;
    return hz<=UINT32_MAX?(uint32_t)hz:0U;
}
#endif
