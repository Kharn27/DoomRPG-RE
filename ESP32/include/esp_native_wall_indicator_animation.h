#ifndef ESP_NATIVE_WALL_INDICATOR_ANIMATION_H
#define ESP_NATIVE_WALL_INDICATOR_ANIMATION_H
#include <stdint.h>

/* Original Render_drawLeaf() selects four physical texel pairs from the
 * logical texture base. Limit live clocking to EV_UNLOCK's 9/10 indicator
 * family; all other wall textures retain their tested fixed-phase route. */
static inline int EspNativeWallIndicator_isAnimated(uint16_t logical) {
    return logical == 9U || logical == 10U;
}
static inline uint8_t EspNativeWallIndicator_phase(uint16_t logical,
                                                  uint32_t nowMs) {
    /* 32-bit wrapping multiplication by 0x400000 >> 30, exactly desktop. */
    uint32_t tick = nowMs + (uint32_t)logical * 3U;
    return (uint8_t)((tick >> 8U) & 3U);
}
#endif
