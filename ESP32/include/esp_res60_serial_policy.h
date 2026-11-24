#ifndef DOOMRPG_ESP32_RES60_SERIAL_POLICY_H
#define DOOMRPG_ESP32_RES60_SERIAL_POLICY_H
#include "platform_video_config.h"
#include <stdint.h>
#if DOOMRPG_RES60_P20_COMPACT_SERIAL
/* Pure diagnostic routing. Unknown/mixed lease conditions retain the
 * verbose fail-closed witness; only verified warm pairs are compact. */
static inline uint8_t EspRes60Serial_compactWarm(uint8_t mapLeaseHit,
                                                  uint8_t textureCacheHit) {
    return (uint8_t)((mapLeaseHit == 1U && textureCacheHit == 1U) ? 1U : 0U);
}
#endif
#endif
