#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_trace.h"

int __real_EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter);

/*
 * The current activation owner deliberately exposes no mutable bitset API.
 * Movement therefore uses a stricter safe subset for this first probe: the
 * candidate must be the monster visible in the player's current forward native
 * trace at the instant the position transaction is prepared. Every such monster
 * is activated by the existing gate; previously-activated but no-longer-forward
 * monsters stay deferred until activation/order gets a dedicated shared API.
 */
int __wrap_EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter) {
    EspNativeGameplayMonsterTarget target;
    EspNativeGameplayMonsterTraceStatus status;

    memset(&target, 0, sizeof(target));
    target.spriteIndex = ESP_NATIVE_GAMEPLAY_MONSTER_POSITION_NO_SPRITE;
    status = EspNativeGameplayMonsterTrace_forward(&target);
    if (status != ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND ||
        target.spriteIndex != spriteIndex) {
        printf("[MONSTERMOVEACT] DEFER sprite=%u forwardStatus=%u forwardSprite=%u activation=current-forward-required mutation=no\n",
               (unsigned int)spriteIndex,
               (unsigned int)status,
               (unsigned int)target.spriteIndex);
        return 0;
    }

    printf("[MONSTERMOVEACT] ALLOW sprite=%u tile=%u activation=current-forward-visible conservative=yes mutation=position-probe-only\n",
           (unsigned int)spriteIndex,
           (unsigned int)target.tileIndex);
    return __real_EspNativeGameplayMonsterPosition_prepareCardinalMove(
        spriteIndex, deltaX, deltaY, outBefore, outAfter);
}
