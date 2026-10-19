#ifndef ESP_NATIVE_AMBIENT_SPRITE_H
#define ESP_NATIVE_AMBIENT_SPRITE_H
#include <stdint.h>
/* Time-only visual policy extracted from desktop Render_drawSprite.
 * Never mutate BSP, gameplay RNG, monster state or turns. */
#define AMBIENT_RAPID_MS 250U
#define AMBIENT_FAST_MS 375U
#define AMBIENT_SLOW_MS 4000U
static inline uint8_t EspNativeAmbientSprite_visual(uint32_t info) {
    return (uint8_t)((info >> 9) & 15U);
}
static inline int EspNativeAmbientSprite_eligible(uint32_t info,uint8_t type) {
    uint8_t v=EspNativeAmbientSprite_visual(info);
    return !(info & 0x80000000UL) && (type != 1U || v == 0U || v == 3U);
}
/* Render_drawSprite: mType 4 (Phantom / Lost Soul / Nightmare)
 * alternates 0/3 after ~250ms, other monsters after ~4000ms.
 * This ESP-native wall-clock phase never mutates monster timers. */
static inline int EspNativeAmbientSprite_rapid(uint8_t type,uint8_t subtype) {
    return type == 1U && subtype == 4U;
}
static inline int EspNativeAmbientSprite_fast(uint8_t type) {
    return type != 1U && type != 2U;
}
/* Admission into a BSP leaf or successful projection does not imply a pixel.
 * Wall occlusion, backface and viewport clipping must not arm idle redraw. */
static inline int EspNativeAmbientSprite_rasterCandidate(uint32_t info,
                                                         uint8_t type,
                                                         uint32_t before,
                                                         uint32_t after) {
    return after > before && EspNativeAmbientSprite_eligible(info,type);
}
static inline uint8_t EspNativeAmbientSprite_frame(uint32_t info,uint8_t type,
                                                   uint8_t subtype,
                                                   uint16_t index,uint32_t ms) {
    uint32_t phase=(uint32_t)(index & 255U)*8U;
    if (!EspNativeAmbientSprite_eligible(info,type))
        return EspNativeAmbientSprite_visual(info);
    if (type == 1U) {
        const uint32_t cadence = EspNativeAmbientSprite_rapid(type,subtype)
                                     ? AMBIENT_RAPID_MS : AMBIENT_SLOW_MS;
        const uint32_t stagger = EspNativeAmbientSprite_rapid(type,subtype)
                                     ? (uint32_t)(index & 255U) : phase;
        return (((ms+stagger)/cadence)&1U) ? 3U : 0U;
    }
    if (type == 2U)
        return (uint8_t)(((ms+(uint32_t)(index & 255U)*256U)/6000U)&1U);
    return (uint8_t)(((ms+phase)/250U)&3U);
}
static inline int EspNativeAmbientSprite_due(uint32_t ms,uint32_t last,
                                             uint8_t fast,uint8_t rapid,
                                             uint8_t any) {
    return any && (uint32_t)(ms-last)>=
        (rapid ? AMBIENT_RAPID_MS :
         fast ? AMBIENT_FAST_MS : AMBIENT_SLOW_MS);
}
#endif
