#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_COMBAT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_COMBAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayMonsterCombatView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t attacks;
    uint32_t hits;
    uint32_t crits;
    uint32_t misses;
    uint32_t kills;
    uint32_t deferredXp;
    uint32_t currentMonsterFNV1a;
    uint16_t pendingSpriteIndex;
    uint16_t painSpriteIndex;
    uint8_t pending;
    uint8_t active;
    uint8_t weaponFamiliesOwned;
    uint8_t reserved;
} EspNativeGameplayMonsterCombatView;

void EspNativeGameplayMonsterCombat_reset(void);
int EspNativeGameplayMonsterCombat_isReady(void);
const EspNativeGameplayMonsterCombatView* EspNativeGameplayMonsterCombat_view(void);

#ifdef __cplusplus
}
#endif

#endif
