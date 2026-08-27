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
    uint8_t reserved;
} EspNativeGameplayWeaponStats;

/*
 * Paint the current idle first-person weapon into the resident gameplay
 * viewport after world sprites and before HUD composition.
 *
 * This is the native equivalent of the idle Combat_drawWeapon() path only:
 * logical sprite = 240 + weapon and the original wpinfo idle offsets are
 * preserved.  The renderer reads bounded ranges from DoomRPG-ESP32.pak into a
 * fixed workspace; it never materializes legacy shapeData/mediaTexels and does
 * not mutate gameplay/world state.
 */
int EspNativeGameplayWeapon_render(
    struct Render_s* render,
    uint8_t weapon,
    uint8_t weaponsPresent,
    EspNativeGameplayWeaponStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
