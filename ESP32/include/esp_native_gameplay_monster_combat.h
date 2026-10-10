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
    uint32_t xpApplied;
    uint32_t currentMonsterFNV1a;
    uint16_t pendingSpriteIndex;
    uint16_t painSpriteIndex;
    uint8_t pending;
    uint8_t active;
    uint8_t standardWeaponsOwned;
    uint8_t reserved;
} EspNativeGameplayMonsterCombatView;

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_SPLASH_MAX_HITS 8U

typedef struct EspNativeGameplaySplashHit_s {
    uint16_t spriteIndex;
    uint8_t healthComponent;
    uint8_t armorComponent;
} EspNativeGameplaySplashHit;

/* One-barrel blast family: stage every monster hit until after exact explosion
 * RNG preview and all wave frames. Apply damage/deaths/drops as one atomic
 * native transaction, with snapshots and rollback on failure. No world-wide
 * collision bypass and no legacy CombatEntity/Entity heap graph. */
int EspNativeGameplayMonsterCombat_applySplash(
    struct DoomRPG_s* runtime,
    const EspNativeGameplaySplashHit* hits,
    uint8_t count,
    uint32_t sequence);

/* Present any XP level-up popup only after the initiating action's
 * final settle frame; never let the action renderer overwrite the modal. */
int EspNativeGameplayMonsterCombat_finishSplashPresentation(
    uint32_t sequence);

void EspNativeGameplayMonsterCombat_reset(void);
int EspNativeGameplayMonsterCombat_isReady(void);
const EspNativeGameplayMonsterCombatView* EspNativeGameplayMonsterCombat_view(void);

#ifdef __cplusplus
}
#endif

#endif
