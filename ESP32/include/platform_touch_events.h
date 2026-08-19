#ifndef DOOMRPG_ESP32_PLATFORM_TOUCH_EVENTS_H
#define DOOMRPG_ESP32_PLATFORM_TOUCH_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PlatformTapCallback)(int16_t screenX,
                                    int16_t screenY,
                                    uint16_t pressure,
                                    uint16_t rawX,
                                    uint16_t rawY);

/* Register the single high-level tap consumer. The platform touch driver still
 * owns XPT2046 sampling/calibration; this callback is emitted once per physical
 * press and is re-armed only after a stable release.
 */
void PlatformInput_setTapCallback(PlatformTapCallback callback);

#ifdef __cplusplus
}
#endif

#endif
