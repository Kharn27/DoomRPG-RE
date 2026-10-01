#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_DEATH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_DEATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef enum EspNativeGameplayPlayerDeathPhase_e {
    ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_IDLE = 0,
    ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_FALL = 1,
    ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_FADE = 2,
    ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_MENU_READY = 3
} EspNativeGameplayPlayerDeathPhase;

void EspNativeGameplayPlayerDeath_reset(void);
int EspNativeGameplayPlayerDeath_isActive(void);
int EspNativeGameplayPlayerDeath_isMenuReady(void);

int EspNativeGameplayPlayerDeath_arm(struct DoomRPG_s* doomRpg,
                                     uint32_t sequence,
                                     uint16_t tileIndex);
int EspNativeGameplayPlayerDeath_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
