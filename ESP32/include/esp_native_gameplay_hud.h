#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUD_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayHudStatus_e {
    ESP_NATIVE_GAMEPLAY_HUD_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT = 1,
    ESP_NATIVE_GAMEPLAY_HUD_DIRTY_NOT_READY = 2,
    ESP_NATIVE_GAMEPLAY_HUD_CLEAR_NOT_READY = 3,
    ESP_NATIVE_GAMEPLAY_HUD_ALREADY_ACTIVE = 4,
    ESP_NATIVE_GAMEPLAY_HUD_FRAMEBUFFER_INVALID = 5,
    ESP_NATIVE_GAMEPLAY_HUD_PACK_BUSY = 6,
    ESP_NATIVE_GAMEPLAY_HUD_PACK_OPEN_FAILED = 7,
    ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED = 8,
    ESP_NATIVE_GAMEPLAY_HUD_PAINT_CONSUME_FAILED = 9,
    ESP_NATIVE_GAMEPLAY_HUD_OK = 10
} EspNativeGameplayHudStatus;

/*
 * Pointer-free mutable values needed by the original compact gameplay HUD.
 * Message text/effects are deliberately not owned yet: this first painter only
 * accepts the hardware-proven fresh-map empty-message context.
 */
typedef struct EspNativeGameplayHudModel_s {
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t health;
    uint8_t maxHealth;
    uint8_t armor;
    uint8_t maxArmor;
    uint8_t ammo;
    uint8_t weapon;
    uint8_t ammoType;
    uint8_t weaponsPresent;
    uint8_t destAngle;
    uint8_t damageActive;
    uint8_t damageDir;
    uint8_t gotFace;
    uint8_t messageCount;
    uint8_t statBarMessagePresent;
    uint8_t logMessageLength;
} EspNativeGameplayHudModel;

typedef struct EspNativeGameplayHudState_s {
    EspNativeGameplayHudModel model;
    uint8_t faceState;
    uint8_t painted;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayHudState;

typedef struct EspNativeGameplayHudStats_s {
    uint32_t packReads;
    uint32_t bytesRead;
    uint32_t rowsRead;
    uint32_t pixelsWritten;
    uint16_t statusBarWidth;
    uint16_t statusBarHeight;
    uint16_t iconWidth;
    uint16_t iconHeight;
    uint16_t faceWidth;
    uint16_t faceHeight;
    uint8_t faceState;
    uint8_t resourcesValidated;
} EspNativeGameplayHudStats;

void EspNativeGameplayHud_reset(void);
int EspNativeGameplayHud_isReady(void);
const EspNativeGameplayHudState* EspNativeGameplayHud_view(void);

/* Pure validation/face selection; no framebuffer, PAK or owner mutation. */
EspNativeGameplayHudStatus EspNativeGameplayHud_prepareInitial(
    const EspNativeGameplayHudModel* model,
    EspNativeGameplayHudState* outState);

/*
 * Paint one initial gameplay HUD directly into the shared 160x120 RGB565
 * framebuffer using bounded indexed-BMP range reads from DoomRPG-ESP32.pak.
 * On success it consumes the matching EspHudRefresh dirty intent exactly once.
 */
EspNativeGameplayHudStatus EspNativeGameplayHud_routeInitial(
    const EspNativeGameplayHudModel* model,
    EspNativeGameplayHudStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
