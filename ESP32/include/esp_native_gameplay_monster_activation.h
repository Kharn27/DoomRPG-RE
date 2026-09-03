#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MONSTER_ACTIVATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read-only map-session activation query owned by the existing conservative
 * forward-visible gate. Movement may consult this bitset, but it never mutates
 * activation state or manufactures per-monster behavior.
 */
int EspNativeGameplayMonsterActivation_isActive(uint16_t spriteIndex);
uint32_t EspNativeGameplayMonsterActivation_count(void);

#ifdef __cplusplus
}
#endif

#endif
