#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_position.h"

int __real_EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter);

/*
 * Movement may mutate only the compact position probe owner for monsters that
 * the already hardware-proven conservative activation gate has observed. The
 * activation bit persists for the map session, matching the recovered legacy
 * active-list lifetime more closely than requiring the monster to remain in the
 * player's current forward trace. Unknown/inactive monsters remain fail-closed.
 */
int __wrap_EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter) {
    if (!EspNativeGameplayMonsterActivation_isActive(spriteIndex)) {
        printf("[MONSTERMOVEACT] DEFER sprite=%u activation=inactive mutation=no\n",
               (unsigned int)spriteIndex);
        return 0;
    }

    printf("[MONSTERMOVEACT] ALLOW sprite=%u activation=map-session-active conservative=yes mutation=position-probe-only\n",
           (unsigned int)spriteIndex);
    return __real_EspNativeGameplayMonsterPosition_prepareCardinalMove(
        spriteIndex, deltaX, deltaY, outBefore, outAfter);
}
