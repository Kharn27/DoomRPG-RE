#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_monster_trace.h"
#include "esp_native_gameplay_monster_turn.h"

#define MONSTER_ACTIVATION_MAX_SPRITES 1024U
#define MONSTER_ACTIVATION_BYTES (MONSTER_ACTIVATION_MAX_SPRITES / 8U)
#define MONSTER_ACTIVATION_NO_SPRITE 0xffffU

typedef struct MonsterActivationGateOwner_s {
    EspNativeGameplayMonsterTurnView filtered;
    uint8_t activeBits[MONSTER_ACTIVATION_BYTES];
    uint32_t sourceArenaFNV1a;
    uint32_t actualAttackProbesSeen;
    uint32_t deliveredAttackProbes;
    uint32_t activatedCount;
    uint32_t deferredCount;
    uint8_t active;
    uint8_t reserved[3];
} MonsterActivationGateOwner;

static MonsterActivationGateOwner activationOwner;

const EspNativeGameplayMonsterTurnView*
__real_EspNativeGameplayMonsterTurn_view(void);

static int isActivated(uint16_t spriteIndex) {
    return spriteIndex < MONSTER_ACTIVATION_MAX_SPRITES &&
           ((activationOwner.activeBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setActivated(uint16_t spriteIndex) {
    uint8_t mask;
    if (spriteIndex >= MONSTER_ACTIVATION_MAX_SPRITES ||
        isActivated(spriteIndex)) {
        return;
    }
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    activationOwner.activeBits[spriteIndex >> 3] |= mask;
    ++activationOwner.activatedCount;
}

static void resetForArena(uint32_t arena) {
    memset(&activationOwner, 0, sizeof(activationOwner));
    activationOwner.sourceArenaFNV1a = arena;
    activationOwner.filtered.lastAttackerSpriteIndex =
        MONSTER_ACTIVATION_NO_SPRITE;
    activationOwner.active = 1U;
    printf("[MONSTERACT] READY arena=%08x ownerBytes=%u bitset=%uB maxSprites=%u source=forward-visible conservative=yes persistence=map-session inactiveAttack=fail-closed legacyRenderActivation=yes\n",
           (unsigned int)arena,
           (unsigned int)sizeof(activationOwner),
           (unsigned int)MONSTER_ACTIVATION_BYTES,
           (unsigned int)MONSTER_ACTIVATION_MAX_SPRITES);
}

static void observeForwardVisible(void) {
    EspNativeGameplayMonsterTarget target;
    EspNativeGameplayMonsterTraceStatus status;

    memset(&target, 0, sizeof(target));
    target.spriteIndex = MONSTER_ACTIVATION_NO_SPRITE;
    status = EspNativeGameplayMonsterTrace_forward(&target);
    if (status != ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND ||
        target.spriteIndex == MONSTER_ACTIVATION_NO_SPRITE ||
        isActivated(target.spriteIndex)) {
        return;
    }

    setActivated(target.spriteIndex);
    printf("[MONSTERACT] ACTIVE sprite=%u subtype=%u tile=%u distance=%u source=forward-visible activeCount=%u persistence=map-session mutation=activation-bit-only gameplayRng=untouched\n",
           (unsigned int)target.spriteIndex,
           (unsigned int)target.subtype,
           (unsigned int)target.tileIndex,
           (unsigned int)target.distance,
           (unsigned int)activationOwner.activatedCount);
}

const EspNativeGameplayMonsterTurnView*
__wrap_EspNativeGameplayMonsterTurn_view(void) {
    const EspNativeGameplayMonsterTurnView* actual =
        __real_EspNativeGameplayMonsterTurn_view();
    uint32_t newProbeCount;

    if (actual == NULL || actual->active != 1U ||
        actual->sourceArenaFNV1a == 0U) {
        return actual;
    }

    if (activationOwner.active == 0U ||
        activationOwner.sourceArenaFNV1a != actual->sourceArenaFNV1a) {
        resetForArena(actual->sourceArenaFNV1a);
    }

    observeForwardVisible();
    activationOwner.filtered = *actual;

    if (actual->attackProbes < activationOwner.actualAttackProbesSeen) {
        /* A producer reset inside the same arena should never happen. Reset the
         * gate rather than replaying an unknown historical probe. */
        activationOwner.actualAttackProbesSeen = actual->attackProbes;
        activationOwner.deliveredAttackProbes = 0U;
        printf("[MONSTERACT] RESET producerProbes=%u cause=producer-counter-regressed failClosed=yes\n",
               (unsigned int)actual->attackProbes);
    }

    newProbeCount = actual->attackProbes - activationOwner.actualAttackProbesSeen;
    if (newProbeCount > 1U) {
        activationOwner.actualAttackProbesSeen = actual->attackProbes;
        ++activationOwner.deferredCount;
        printf("[MONSTERACT] DEFER actualProbe=%u gap=%u cause=probe-sequence-gap delivered=%u mutation=no rngConsumed=0\n",
               (unsigned int)actual->attackProbes,
               (unsigned int)newProbeCount,
               (unsigned int)activationOwner.deliveredAttackProbes);
    }
    else if (newProbeCount == 1U) {
        activationOwner.actualAttackProbesSeen = actual->attackProbes;
        if (actual->lastAttackerSpriteIndex != MONSTER_ACTIVATION_NO_SPRITE &&
            isActivated(actual->lastAttackerSpriteIndex)) {
            ++activationOwner.deliveredAttackProbes;
            printf("[MONSTERACT] DELIVER actualProbe=%u deliveredProbe=%u sprite=%u reason=%u activated=yes\n",
                   (unsigned int)actual->attackProbes,
                   (unsigned int)activationOwner.deliveredAttackProbes,
                   (unsigned int)actual->lastAttackerSpriteIndex,
                   (unsigned int)actual->lastReason);
        }
        else {
            ++activationOwner.deferredCount;
            printf("[MONSTERACT] ACTIVATION-DEFER actualProbe=%u sprite=%u reason=%u active=no deferred=%u mutation=no rngConsumed=0\n",
                   (unsigned int)actual->attackProbes,
                   (unsigned int)actual->lastAttackerSpriteIndex,
                   (unsigned int)actual->lastReason,
                   (unsigned int)activationOwner.deferredCount);
        }
    }

    activationOwner.filtered.attackProbes =
        activationOwner.deliveredAttackProbes;
    return &activationOwner.filtered;
}
