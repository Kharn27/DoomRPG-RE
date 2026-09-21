#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HIT_FEEDBACK_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HIT_FEEDBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Presentation-only player->monster impact cue.
 *
 * Arm only after the monster-combat rollback boundary has closed. The owner
 * never mutates combat state or consumes gameplay RNG; it derives a bounded
 * local visual stream from already-committed semantic inputs.
 */
int EspNativeGameplayHitFeedback_arm(uint32_t sequence,
                                     uint16_t spriteIndex,
                                     uint8_t distance,
                                     int32_t healthBefore,
                                     int32_t armorBefore,
                                     int32_t totalDamage,
                                     int32_t totalArmorDamage);

#ifdef __cplusplus
}
#endif

#endif
