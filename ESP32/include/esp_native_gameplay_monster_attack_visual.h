#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ATTACK_VISUAL_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ATTACK_VISUAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef struct EspNativeGameplayMonsterAttackVisualView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t observedAttackProbes;
    uint32_t presentedAttacks;
    uint32_t deferredAlternate;
    uint32_t deferredMultiLoop;
    uint32_t renderRollbacks;
    uint32_t expiryRetries;
    uint32_t activeProbe;
    uint32_t clearAtMs;
    uint16_t activeSpriteIndex;
    uint8_t active;
    uint8_t poseActive;
} EspNativeGameplayMonsterAttackVisualView;

/*
 * Presentation-only bridge for bounded single-loop monster attack poses.
 *
 * Legacy Combat_performAttack() selects attackFrame=1 for a primary monster
 * attack and attackFrame=5 for alternate/special attacks. Combat_monsterSeq()
 * holds each attack frame for 150 ms. This owner recovers both frame families
 * when NUMSHOTS==1. It never mutates MonsterState, topology, player state,
 * gameplay RNG, BSP sprites, projectiles or audio.
 *
 * Multi-loop (three-shot) presentation remains fail-closed until its own
 * bounded milestone. Projectiles, attack messages and sound remain deferred.
 */
void EspNativeGameplayMonsterAttackVisual_reset(void);
void EspNativeGameplayMonsterAttackVisual_service(struct DoomRPG_s* doomRpg);

/* Called by the existing monster visual-state composition leaf. Returns 1 only
 * when this owner currently overrides the requested live enemy to its leased
 * legacy attack frame (1 primary or 5 alternate). */
int EspNativeGameplayMonsterAttackVisual_apply(uint32_t spriteIndex,
                                               uint8_t* ioVisualState);

/* Renderer-side helper used to promote only the actively owned attack pose into
 * the native FIXED_ANIM frame-offset contract. */
int EspNativeGameplayMonsterAttackVisual_isPoseSprite(uint32_t spriteIndex);

const EspNativeGameplayMonsterAttackVisualView*
EspNativeGameplayMonsterAttackVisual_view(void);

#ifdef __cplusplus
}
#endif

#endif
