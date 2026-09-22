#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HIT_FEEDBACK_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HIT_FEEDBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Presentation-only player->monster impact cue.
 *
 * The combat owner may provisionally arm the cue immediately before its attack
 * frame so blood is painted on the same physical frame as pain/death. If that
 * render transaction fails, cancel() is called before the rollback redraw.
 * Neither operation mutates combat state or consumes gameplay RNG.
 */
int EspNativeGameplayHitFeedback_arm(uint32_t sequence,
                                     uint16_t spriteIndex,
                                     uint8_t distance,
                                     int32_t healthBefore,
                                     int32_t armorBefore,
                                     int32_t totalDamage,
                                     int32_t totalArmorDamage);
int EspNativeGameplayHitFeedback_cancel(uint32_t sequence);

#ifdef __cplusplus
}
#endif

#endif
