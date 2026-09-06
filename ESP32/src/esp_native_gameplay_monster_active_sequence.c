#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"

#define ACTIVESEQ_TYPE_ENEMY 1U
#define ACTIVESEQ_SUBTYPE_SPECIAL_AI 10U
#define ACTIVESEQ_SUBTYPE_LIMIT 14U
#define ACTIVESEQ_NO_SPRITE 0xffffU
#define ACTIVESEQ_SPRITE_NO_ACTIVATE 0x01000000UL

typedef struct ActiveMovementSequencer_s {
    uint32_t sourceArenaFNV1a;
    uint32_t actualNoAttackSeen;
    uint32_t actualMovementSeen;
    uint32_t expandedNoAttack;
    uint32_t expandedMovement;
    uint32_t expandedTurns;
    uint32_t deliveredMembers;
    uint32_t deferredTurns;
    uint8_t active;
    uint8_t reserved[3];
} ActiveMovementSequencer;

static ActiveMovementSequencer activeSeq;

int __real_EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex);
int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);
void __real_EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* doomRpg);
const EspNativeGameplayMonsterTurnView*
__real_EspNativeGameplayMonsterTurn_view(void);

static uint16_t read16le(const uint8_t* bytes) {
    if (bytes == NULL) return 0U;
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

/* Legacy Render_renderSpriteObject() calls Game_activate() as soon as an
 * admitted non-hidden monster sprite reaches the sprite-object renderer, before
 * near-plane/column clipping. The native BSP-visible admission point is the
 * equivalent permanent boundary, so activation is observed here rather than by
 * the old center-ray approximation. */
int __wrap_EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex) {
    const EspMapSpriteTopologyView* topology;
    EspMapSprite sprite;
    uint16_t linkState;
    uint16_t tile;
    uint8_t type;
    uint8_t subtype;
    int visible = __real_EspNativeBspVisibility_mapSpriteVisible(
        state, mapSpriteIndex, outLeafIndex);

    if (!visible || mapSpriteIndex > 0xffffU) return visible;
    topology = EspMapSpriteTopology_view();
    if (topology == NULL || topology->entityTypes == NULL ||
        topology->entitySubTypes == NULL || topology->linkStatesLE == NULL ||
        mapSpriteIndex >= topology->spriteCount) {
        return visible;
    }

    type = topology->entityTypes[mapSpriteIndex];
    subtype = (uint8_t)(topology->entitySubTypes[mapSpriteIndex] & 0x7fU);
    linkState = read16le(&topology->linkStatesLE[mapSpriteIndex * 2U]);
    if (type != ACTIVESEQ_TYPE_ENEMY ||
        (linkState & (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                      ESP_MAP_SPRITE_TOPOLOGY_ALIVE)) !=
            (ESP_MAP_SPRITE_TOPOLOGY_LINKED |
             ESP_MAP_SPRITE_TOPOLOGY_ALIVE) ||
        !__real_EspMapRuntime_getMapSprite(mapSpriteIndex, &sprite) ||
        (sprite.info & ACTIVESEQ_SPRITE_NO_ACTIVATE) != 0U) {
        return visible;
    }

    tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
    (void)EspNativeGameplayMonsterActivation_observeVisible(
        (uint16_t)mapSpriteIndex, subtype, tile);
    return visible;
}

static void clearCompositionOverrides(void) {
    EspNativeGameplayMonsterActivation_clearSelection();
    EspNativeGameplayMonsterActivation_clearTurnCounterOverride();
}

static void callMovementMember(struct DoomRPG_s* doomRpg,
                               uint16_t spriteIndex,
                               int selected) {
    if (selected) {
        EspNativeGameplayMonsterActivation_selectOnly(spriteIndex);
    }
    else {
        EspNativeGameplayMonsterActivation_clearSelection();
    }
    EspNativeGameplayMonsterActivation_overrideTurnCounters(
        activeSeq.expandedMovement, activeSeq.expandedNoAttack);
    __real_EspNativeGameplayMonsterMovement_service(doomRpg);
    clearCompositionOverrides();
}

static void resetSequencer(const EspNativeGameplayMonsterTurnView* actual,
                           struct DoomRPG_s* doomRpg) {
    memset(&activeSeq, 0, sizeof(activeSeq));
    if (actual == NULL) return;
    activeSeq.sourceArenaFNV1a = actual->sourceArenaFNV1a;
    activeSeq.actualNoAttackSeen = actual->noAttackTurns;
    activeSeq.actualMovementSeen = actual->movementDeferredTurns;
    activeSeq.expandedNoAttack = actual->noAttackTurns;
    activeSeq.expandedMovement = actual->movementDeferredTurns;
    activeSeq.active = 1U;

    /* Prime the existing planner's private observed counters before any player
     * turn delta. This call is a no-op gameplay-wise because both synthetic
     * counters equal their current baselines. */
    callMovementMember(doomRpg, ACTIVESEQ_NO_SPRITE, 0);
    printf("[MONSTERACTIVESEQ] READY arena=%08x ownerBytes=%u activationOrder=first-render-activation planner=existing-single-candidate syntheticCounters=movement-private-only multiAttack=still-fail-closed allocation=no\n",
           (unsigned int)activeSeq.sourceArenaFNV1a,
           (unsigned int)sizeof(activeSeq));
}

/* Expand one legacy no-immediate-attack monster turn into one call of the
 * already-proven movement planner per active-list member, in first-activation
 * order. Each call sees exactly one activation bit and one monotonically
 * synthetic noAttack counter. RNG/topology publication therefore remains owned
 * by the existing planner/publisher, and the second monster observes the first
 * monster's committed position exactly like Game_monsterAI()'s sequential loop.
 *
 * The ranged >=217 producer remains on its previous single-candidate boundary;
 * simultaneous attack-ready ordering is also deliberately still fail-closed.
 */
void __wrap_EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* doomRpg) {
    const EspNativeGameplayMonsterTurnView* actual =
        __real_EspNativeGameplayMonsterTurn_view();
    uint32_t activationCount;
    uint32_t ordinal;
    uint32_t delivered = 0U;

    if (actual == NULL || actual->active != 1U ||
        actual->sourceArenaFNV1a == 0U) {
        clearCompositionOverrides();
        __real_EspNativeGameplayMonsterMovement_service(doomRpg);
        return;
    }

    if (activeSeq.active == 0U ||
        activeSeq.sourceArenaFNV1a != actual->sourceArenaFNV1a ||
        actual->noAttackTurns < activeSeq.actualNoAttackSeen ||
        actual->movementDeferredTurns < activeSeq.actualMovementSeen) {
        resetSequencer(actual, doomRpg);
        return;
    }

    if (actual->noAttackTurns == activeSeq.actualNoAttackSeen &&
        actual->movementDeferredTurns == activeSeq.actualMovementSeen) {
        return;
    }

    if (actual->noAttackTurns != activeSeq.actualNoAttackSeen &&
        actual->movementDeferredTurns != activeSeq.actualMovementSeen) {
        ++activeSeq.deferredTurns;
        printf("[MONSTERACTIVESEQ] DEFER reason=dual-trigger noAttack=%u->%u rangedMove=%u->%u mutation=no rngConsumed=0\n",
               (unsigned int)activeSeq.actualNoAttackSeen,
               (unsigned int)actual->noAttackTurns,
               (unsigned int)activeSeq.actualMovementSeen,
               (unsigned int)actual->movementDeferredTurns);
        activeSeq.actualNoAttackSeen = actual->noAttackTurns;
        activeSeq.actualMovementSeen = actual->movementDeferredTurns;
        return;
    }

    if (actual->movementDeferredTurns != activeSeq.actualMovementSeen) {
        if (actual->movementDeferredTurns != activeSeq.actualMovementSeen + 1U) {
            ++activeSeq.deferredTurns;
            printf("[MONSTERACTIVESEQ] DEFER reason=ranged-trigger-gap observed=%u current=%u mutation=no rngConsumed=0\n",
                   (unsigned int)activeSeq.actualMovementSeen,
                   (unsigned int)actual->movementDeferredTurns);
            activeSeq.actualMovementSeen = actual->movementDeferredTurns;
            return;
        }
        activeSeq.actualMovementSeen = actual->movementDeferredTurns;
        ++activeSeq.expandedMovement;
        callMovementMember(doomRpg, ACTIVESEQ_NO_SPRITE, 0);
        return;
    }

    if (actual->noAttackTurns != activeSeq.actualNoAttackSeen + 1U) {
        ++activeSeq.deferredTurns;
        printf("[MONSTERACTIVESEQ] DEFER reason=no-attack-trigger-gap observed=%u current=%u mutation=no rngConsumed=0\n",
               (unsigned int)activeSeq.actualNoAttackSeen,
               (unsigned int)actual->noAttackTurns);
        activeSeq.actualNoAttackSeen = actual->noAttackTurns;
        return;
    }
    activeSeq.actualNoAttackSeen = actual->noAttackTurns;
    ++activeSeq.expandedTurns;

    activationCount = EspNativeGameplayMonsterActivation_count();
    for (ordinal = 0U; ordinal < activationCount; ++ordinal) {
        uint16_t spriteIndex = ACTIVESEQ_NO_SPRITE;
        const EspNativeGameplayMonsterRecord* monster;

        if (!EspNativeGameplayMonsterActivation_getOrdered(ordinal,
                                                            &spriteIndex)) {
            ++activeSeq.deferredTurns;
            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u activeCount=%u cause=activation-order-read mutation=no rngConsumed=0\n",
                   (unsigned int)activeSeq.expandedTurns,
                   (unsigned int)ordinal,
                   (unsigned int)activationCount);
            break;
        }
        monster = EspNativeGameplayMonsterState_find(spriteIndex);
        if (monster == NULL || monster->alive == 0U ||
            monster->subtype >= ACTIVESEQ_SUBTYPE_LIMIT ||
            monster->subtype == ACTIVESEQ_SUBTYPE_SPECIAL_AI) {
            continue;
        }

        ++activeSeq.expandedNoAttack;
        callMovementMember(doomRpg, spriteIndex, 1);
        ++delivered;
        ++activeSeq.deliveredMembers;
        printf("[MONSTERACTIVESEQ] MEMBER turn=%u ordinal=%u/%u sprite=%u subtype=%u syntheticNoAttack=%u sameMonsterTurn=yes planner=existing publication=existing\n",
               (unsigned int)activeSeq.expandedTurns,
               (unsigned int)(ordinal + 1U),
               (unsigned int)activationCount,
               (unsigned int)spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)activeSeq.expandedNoAttack);
    }

    if (delivered == 0U) {
        ++activeSeq.expandedNoAttack;
        callMovementMember(doomRpg, ACTIVESEQ_NO_SPRITE, 0);
    }
    printf("[MONSTERACTIVESEQ] COMPLETE turn=%u reason=%u activeCount=%u delivered=%u sameMonsterTurn=yes ordered=yes multiAttack=deferred\n",
           (unsigned int)activeSeq.expandedTurns,
           (unsigned int)actual->lastReason,
           (unsigned int)activationCount,
           (unsigned int)delivered);
}
