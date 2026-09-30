#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_runtime.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_turn.h"

#define MONSTER_ACTIVATION_MAX_SPRITES \
    ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_SPRITES
#define MONSTER_ACTIVATION_BYTES \
    ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_BYTES
#define MONSTER_ACTIVATION_MAX_ORDER \
    ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_MAX_ORDER
#define MONSTER_ACTIVATION_NO_SPRITE 0xffffU

typedef struct MonsterActivationGateOwner_s {
    EspNativeGameplayMonsterTurnView filtered;
    uint8_t activeBits[MONSTER_ACTIVATION_BYTES];
    uint16_t activeOrder[MONSTER_ACTIVATION_MAX_ORDER];
    uint32_t sourceArenaFNV1a;
    uint32_t actualAttackProbesSeen;
    uint32_t deliveredAttackProbes;
    uint32_t activatedCount;
    uint32_t deferredCount;
    uint32_t producerMovementDeferredTurns;
    uint32_t producerNoAttackTurns;
    uint16_t selectedSprite;
    uint8_t activeOrderCount;
    uint8_t active;
    uint8_t selectionActive;
} MonsterActivationGateOwner;

static MonsterActivationGateOwner activationOwner;

static uint32_t activationSnapshotFNV(
    const EspNativeGameplayMonsterActivationSnapshot* snapshot) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (snapshot == NULL) return 0U;
    for (i = 0U; i < sizeof(snapshot->activeBits); ++i) {
        hash ^= snapshot->activeBits[i];
        hash *= 16777619U;
    }
    for (i = 0U; i < snapshot->activeOrderCount; ++i) {
        const uint16_t value = snapshot->activeOrder[i];
        hash ^= (uint8_t)(value & 0xffU);
        hash *= 16777619U;
        hash ^= (uint8_t)((value >> 8) & 0xffU);
        hash *= 16777619U;
    }
    hash ^= (uint8_t)(snapshot->activatedCount & 0xffU);
    hash *= 16777619U;
    hash ^= (uint8_t)((snapshot->activatedCount >> 8) & 0xffU);
    hash *= 16777619U;
    hash ^= snapshot->activeOrderCount;
    return hash * 16777619U;
}

int EspNativeGameplayDestructibleTurn_flush(void);

static int isActivated(uint16_t spriteIndex) {
    return spriteIndex < MONSTER_ACTIVATION_MAX_SPRITES &&
           ((activationOwner.activeBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void resetForArena(uint32_t arena) {
    memset(&activationOwner, 0, sizeof(activationOwner));
    activationOwner.sourceArenaFNV1a = arena;
    activationOwner.filtered.lastAttackerSpriteIndex =
        MONSTER_ACTIVATION_NO_SPRITE;
    activationOwner.selectedSprite = MONSTER_ACTIVATION_NO_SPRITE;
    activationOwner.active = 1U;
    printf("[MONSTERACT] READY arena=%08x ownerBytes=%u bitset=%uB orderBytes=%u maxSprites=%u maxOrdered=%u source=bsp-render-visible persistence=map-session inactiveAttack=fail-closed legacyRenderActivation=yes movementOrder=first-activation\n",
           (unsigned int)arena,
           (unsigned int)sizeof(activationOwner),
           (unsigned int)MONSTER_ACTIVATION_BYTES,
           (unsigned int)sizeof(activationOwner.activeOrder),
           (unsigned int)MONSTER_ACTIVATION_MAX_SPRITES,
           (unsigned int)MONSTER_ACTIVATION_MAX_ORDER);
}

static int ensureArena(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    if (runtime == NULL || runtime->arenaFNV1a == 0U) return 0;
    if (activationOwner.active == 0U ||
        activationOwner.sourceArenaFNV1a != runtime->arenaFNV1a) {
        resetForArena(runtime->arenaFNV1a);
    }
    return 1;
}

static int setActivated(uint16_t spriteIndex) {
    uint8_t mask;
    if (spriteIndex >= MONSTER_ACTIVATION_MAX_SPRITES) return 0;
    if (isActivated(spriteIndex)) return 1;
    if (activationOwner.activeOrderCount >= MONSTER_ACTIVATION_MAX_ORDER) {
        ++activationOwner.deferredCount;
        printf("[MONSTERACT] DEFER sprite=%u cause=activation-order-capacity max=%u mutation=no rngConsumed=0\n",
               (unsigned int)spriteIndex,
               (unsigned int)MONSTER_ACTIVATION_MAX_ORDER);
        return 0;
    }

    mask = (uint8_t)(1U << (spriteIndex & 7U));
    activationOwner.activeBits[spriteIndex >> 3] |= mask;
    activationOwner.activeOrder[activationOwner.activeOrderCount++] = spriteIndex;
    ++activationOwner.activatedCount;
    return 1;
}

int EspNativeGameplayMonsterActivation_observeVisible(uint16_t spriteIndex,
                                                      uint8_t subtype,
                                                      uint16_t tileIndex) {
    uint32_t before;
    if (!ensureArena()) return 0;
    if (isActivated(spriteIndex)) return 1;
    before = activationOwner.activatedCount;
    if (!setActivated(spriteIndex)) return 0;
    printf("[MONSTERACT] ACTIVE sprite=%u subtype=%u tile=%u source=bsp-render-visible activeCount=%u activationOrder=%u persistence=map-session mutation=activation-bit+order-only gameplayRng=untouched\n",
           (unsigned int)spriteIndex,
           (unsigned int)subtype,
           (unsigned int)tileIndex,
           (unsigned int)activationOwner.activatedCount,
           (unsigned int)before);
    return 1;
}

int EspNativeGameplayMonsterActivation_isActive(uint16_t spriteIndex) {
    if (activationOwner.active == 0U || !isActivated(spriteIndex)) return 0;
    if (activationOwner.selectionActive != 0U) {
        return activationOwner.selectedSprite == spriteIndex;
    }
    return 1;
}

uint32_t EspNativeGameplayMonsterActivation_count(void) {
    return activationOwner.active != 0U ? activationOwner.activatedCount : 0U;
}

int EspNativeGameplayMonsterActivation_getOrdered(uint32_t ordinal,
                                                  uint16_t* outSpriteIndex) {
    if (outSpriteIndex == NULL || activationOwner.active == 0U ||
        ordinal >= activationOwner.activeOrderCount) {
        return 0;
    }
    *outSpriteIndex = activationOwner.activeOrder[ordinal];
    return 1;
}

void EspNativeGameplayMonsterActivation_reset(void) {
    memset(&activationOwner, 0, sizeof(activationOwner));
    activationOwner.filtered.lastAttackerSpriteIndex =
        MONSTER_ACTIVATION_NO_SPRITE;
    activationOwner.selectedSprite = MONSTER_ACTIVATION_NO_SPRITE;
}

int EspNativeGameplayMonsterActivation_snapshotShapeValid(
    const EspNativeGameplayMonsterActivationSnapshot* snapshot,
    uint32_t expectedArenaFNV1a) {
    uint8_t expectedBits[MONSTER_ACTIVATION_BYTES];
    uint32_t i;

    if (snapshot == NULL || expectedArenaFNV1a == 0U ||
        snapshot->sourceArenaFNV1a != expectedArenaFNV1a ||
        snapshot->stateFNV1a == 0U || snapshot->reserved0 != 0U ||
        snapshot->activeOrderCount > MONSTER_ACTIVATION_MAX_ORDER ||
        snapshot->activatedCount != snapshot->activeOrderCount ||
        activationSnapshotFNV(snapshot) != snapshot->stateFNV1a) {
        return 0;
    }

    memset(expectedBits, 0, sizeof(expectedBits));
    for (i = 0U; i < snapshot->activeOrderCount; ++i) {
        const uint16_t spriteIndex = snapshot->activeOrder[i];
        const uint8_t mask = (uint8_t)(1U << (spriteIndex & 7U));
        if (spriteIndex >= MONSTER_ACTIVATION_MAX_SPRITES ||
            (expectedBits[spriteIndex >> 3] & mask) != 0U) {
            return 0;
        }
        expectedBits[spriteIndex >> 3] |= mask;
    }
    if (memcmp(expectedBits, snapshot->activeBits,
               sizeof(expectedBits)) != 0) {
        return 0;
    }
    for (i = snapshot->activeOrderCount;
         i < MONSTER_ACTIVATION_MAX_ORDER; ++i) {
        if (snapshot->activeOrder[i] != 0U) return 0;
    }
    return 1;
}

int EspNativeGameplayMonsterActivation_snapshot(
    EspNativeGameplayMonsterActivationSnapshot* outSnapshot) {
    if (outSnapshot == NULL || !ensureArena()) return 0;

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->sourceArenaFNV1a = activationOwner.sourceArenaFNV1a;
    memcpy(outSnapshot->activeBits, activationOwner.activeBits,
           sizeof(outSnapshot->activeBits));
    memcpy(outSnapshot->activeOrder, activationOwner.activeOrder,
           activationOwner.activeOrderCount *
               sizeof(outSnapshot->activeOrder[0]));
    outSnapshot->activatedCount =
        (uint16_t)activationOwner.activatedCount;
    outSnapshot->activeOrderCount = activationOwner.activeOrderCount;
    outSnapshot->stateFNV1a = activationSnapshotFNV(outSnapshot);
    return EspNativeGameplayMonsterActivation_snapshotShapeValid(
        outSnapshot, activationOwner.sourceArenaFNV1a);
}

int EspNativeGameplayMonsterActivation_restoreSnapshot(
    const EspNativeGameplayMonsterActivationSnapshot* snapshot) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        !EspNativeGameplayMonsterActivation_snapshotShapeValid(
            snapshot, runtime->arenaFNV1a)) {
        return 0;
    }

    EspNativeGameplayMonsterActivation_reset();
    activationOwner.sourceArenaFNV1a = runtime->arenaFNV1a;
    memcpy(activationOwner.activeBits, snapshot->activeBits,
           sizeof(activationOwner.activeBits));
    memcpy(activationOwner.activeOrder, snapshot->activeOrder,
           sizeof(activationOwner.activeOrder));
    activationOwner.activatedCount = snapshot->activatedCount;
    activationOwner.activeOrderCount = snapshot->activeOrderCount;
    activationOwner.active = 1U;

    printf("[MONSTERACT] RESTORE arena=%08x activeCount=%u orderCount=%u stateFNV=%08x source=checkpoint-v9 producerCounters=fresh selection=clear\n",
           (unsigned int)activationOwner.sourceArenaFNV1a,
           (unsigned int)activationOwner.activatedCount,
           (unsigned int)activationOwner.activeOrderCount,
           (unsigned int)snapshot->stateFNV1a);
    return 1;
}

void EspNativeGameplayMonsterActivation_selectOnly(uint16_t spriteIndex) {
    activationOwner.selectedSprite = spriteIndex;
    activationOwner.selectionActive = 1U;
}

void EspNativeGameplayMonsterActivation_clearSelection(void) {
    activationOwner.selectedSprite = MONSTER_ACTIVATION_NO_SPRITE;
    activationOwner.selectionActive = 0U;
}

void EspNativeGameplayMonsterActivation_overrideTurnCounters(
    uint32_t movementDeferredTurns,
    uint32_t noAttackTurns) {
    if (activationOwner.filtered.active != 1U) return;
    activationOwner.filtered.movementDeferredTurns = movementDeferredTurns;
    activationOwner.filtered.noAttackTurns = noAttackTurns;
}

void EspNativeGameplayMonsterActivation_clearTurnCounterOverride(void) {
    if (activationOwner.filtered.active != 1U) return;
    activationOwner.filtered.movementDeferredTurns =
        activationOwner.producerMovementDeferredTurns;
    activationOwner.filtered.noAttackTurns =
        activationOwner.producerNoAttackTurns;
}

int EspNativeGameplayMonsterActivation_serviceTurn(void) {
    const EspNativeGameplayMonsterTurnView* actual;
    uint32_t newProbeCount;

    /* A successfully committed destructible player attack uses the existing
     * pass-request transport only after its action-service rollback window is
     * closed. This explicit service runs immediately after the MonsterTurn
     * producer, replacing the historical side effect hidden in Turn_view(). */
    (void)EspNativeGameplayDestructibleTurn_flush();
    actual = EspNativeGameplayMonsterTurn_view();

    if (actual == NULL || actual->active != 1U ||
        actual->sourceArenaFNV1a == 0U) {
        activationOwner.filtered.active = 0U;
        return 0;
    }

    if (activationOwner.active == 0U ||
        activationOwner.sourceArenaFNV1a != actual->sourceArenaFNV1a) {
        resetForArena(actual->sourceArenaFNV1a);
    }

    activationOwner.filtered = *actual;
    activationOwner.producerMovementDeferredTurns =
        actual->movementDeferredTurns;
    activationOwner.producerNoAttackTurns = actual->noAttackTurns;

    if (actual->attackProbes < activationOwner.actualAttackProbesSeen) {
        /* A producer reset inside the same arena should never happen. Reset the
         * delivery counters rather than replaying an unknown historical probe. */
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
    return 1;
}

const EspNativeGameplayMonsterTurnView*
EspNativeGameplayMonsterActivation_turnView(void) {
    if (activationOwner.filtered.active != 1U ||
        activationOwner.filtered.sourceArenaFNV1a == 0U ||
        activationOwner.filtered.sourceArenaFNV1a !=
            activationOwner.sourceArenaFNV1a) {
        return NULL;
    }
    return &activationOwner.filtered;
}
