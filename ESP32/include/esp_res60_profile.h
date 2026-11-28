#ifndef DOOMRPG_ESP_RES60_PROFILE_H
#define DOOMRPG_ESP_RES60_PROFILE_H
#include <stdint.h>
#include "platform_video_config.h"
/* Time source is the platform owner (Arduino micros) in RES60 only.
 * Host C11 oracles supply deterministic monotonic/wrapping fake ticks.
 * No allocation, global mutable sample history or timer in gameplay. */
#if DOOMRPG_RES60_P11_PROFILE
#ifdef __cplusplus
extern "C" {
#endif
uint32_t EspRes60Profile_nowUs(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
