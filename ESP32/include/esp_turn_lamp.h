#ifndef ESP_TURN_LAMP_H
#define ESP_TURN_LAMP_H
#include <stdint.h>

/* Pure presentation clock. A full LCD repaint replaces the physical lamp but
 * does not restart the blink. Unsigned elapsed handles millis() wrap. */
typedef struct {
    uint32_t startMs;
    uint8_t busy;
    uint8_t drawn;
} EspTurnLampState;

static inline void EspTurnLamp_reset(EspTurnLampState* s) {
    s->startMs = 0U;
    s->busy = s->drawn = 0U;
}

static inline void EspTurnLamp_replaced(EspTurnLampState* s) {
    s->drawn = 0U;
}

/* +1 draw red; -1 restore last physically presented patch; 0 no SPI. */
static inline int EspTurnLamp_update(EspTurnLampState* s, uint32_t now,
                                    int busy) {
    if (busy && !s->busy) s->startMs = now;
    s->busy = busy != 0;
    uint8_t desired = s->busy &&
        (((now - s->startMs) / 200U) & 1U) == 0U;
    if (desired == s->drawn) return 0;
    s->drawn = desired;
    return desired ? 1 : -1;
}
#endif
