#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_SESSION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_SESSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Map-independent initial player/HUD values supplied by the caller. Map id,
 * gameplay load id, load type and direction are never configured here: they
 * come from the current permanent EspPlayerView owner.
 */
typedef struct EspNativeGameplaySessionConfig_s {
    uint8_t health;
    uint8_t maxHealth;
    uint8_t armor;
    uint8_t maxArmor;
    uint8_t ammo;
    uint8_t weapon;
    uint8_t ammoType;
    uint8_t weaponsPresent;
} EspNativeGameplaySessionConfig;

void EspNativeGameplaySession_reset(void);
int EspNativeGameplaySession_configure(
    const EspNativeGameplaySessionConfig* config);
void EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg);
int EspNativeGameplaySession_isActive(void);
int EspNativeGameplaySession_hasFailed(void);

#ifdef __cplusplus
}
#endif

#endif
