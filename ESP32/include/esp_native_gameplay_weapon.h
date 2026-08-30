#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_WEAPON_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_WEAPON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeGameplayWeaponStats_s {
    uint32_t packReads;
    uint32_t pixelsWritten;
    uint32_t activePixels;
    uint32_t frameBytes;
    uint16_t logicalSprite;
    uint16_t actualSprite;
    int16_t anchorX;
    int16_t anchorY;
    uint8_t weapon;
    uint8_t drawn;
    uint8_t skipped;
    uint8_t animationFrame;
} EspNativeGameplayWeaponStats;

/*
 * Paint the current first-person weapon into the resident gameplay viewport
 * after world sprites and before HUD composition.
 *
 * The normal pose is legacy animation frame 0. A bounded one-shot attack can
 * arm frame 1 for the next complete gameplay render; both poses use the
 * original Combat.c wpinfo offsets. The renderer reads bounded PAK ranges into
 * one reusable workspace and never materializes legacy shapeData/mediaTexels.
 */
int EspNativeGameplayWeapon_render(
    struct Render_s* render,
    uint8_t weapon,
    uint8_t weaponsPresent,
    EspNativeGameplayWeaponStats* outStats);

/* Arm the legacy frame-1 attack pose for exactly the next successful render of
 * this weapon. This is presentation state only: ammo, damage, sound and combat
 * consequences remain owned by their gameplay backends. */
int EspNativeGameplayWeapon_armAttack(uint8_t weapon);
void EspNativeGameplayWeapon_cancelAttack(void);

#ifdef __cplusplus
}
#endif

#endif
