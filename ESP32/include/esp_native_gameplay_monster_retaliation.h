#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_RETALIATION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_RETALIATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef struct EspNativeGameplayMonsterRetaliationView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t observedAttackProbes;
    uint32_t committedAttacks;
    uint32_t committedMisses;
    uint32_t renderRollbacks;
    uint32_t lethalDeferred;
    uint32_t dogFamiliarDeferred;
    uint16_t lastAttackerSpriteIndex;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayMonsterRetaliationView;

void EspNativeGameplayMonsterRetaliation_reset(void);
void EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* doomRpg);
const EspNativeGameplayMonsterRetaliationView*
EspNativeGameplayMonsterRetaliation_view(void);

#ifdef __cplusplus
}
#endif

#endif
