#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_PAIN_TINT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_PAIN_TINT_H

#include <stdint.h>

#define ESP_NATIVE_MONSTER_PAIN_POSE_MS 250U
#define ESP_NATIVE_MONSTER_BLEND_MS 100U

/* Immutable source Render_renderSpriteObject() palette selection:
 * Combat_spawnBloodParticles sets damageBlendTime=now+100ms.
 * Native MonsterCombat keeps the same event's pain pose until now+250ms.
 * Independent of sprite frame, damage amount and gameplay RNG. */
static inline int EspNativeMonsterPainTint_active(
    uint32_t nowMs, uint32_t painUntilMs,
    uint16_t painSpriteIndex, uint16_t candidateSpriteIndex) {
    if (painSpriteIndex == 0xffffU ||
        candidateSpriteIndex != painSpriteIndex ||
        painUntilMs == 0U) return 0;
    return (int32_t)(nowMs -
        (painUntilMs -
         (ESP_NATIVE_MONSTER_PAIN_POSE_MS - ESP_NATIVE_MONSTER_BLEND_MS))) < 0;
}
#endif
