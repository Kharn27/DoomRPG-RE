#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_PAIN_VISUAL_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_PAIN_VISUAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hud.c / Player_painEvent(): 500ms, and the 0/1/2/3 damageDir mapping.
 * Direction NONE is valid when an attacker position is unavailable. */
#define ESP_NATIVE_PLAYER_PAIN_MS 500U

typedef enum EspNativePlayerPainSide_e {
    ESP_NATIVE_PLAYER_PAIN_NONE = 0,
    ESP_NATIVE_PLAYER_PAIN_FRONT = 1,
    ESP_NATIVE_PLAYER_PAIN_RIGHT = 2,
    ESP_NATIVE_PLAYER_PAIN_BACK = 3,
    ESP_NATIVE_PLAYER_PAIN_LEFT = 4
} EspNativePlayerPainSide;

/* Presentation-only two-phase lease. STAGE may tint the transactional frame;
 * a failed render MUST cancel it, a successful world COMMIT publishes it.
 * Neither path reads the gameplay RNG or modifies combat/player state. */
void EspNativePlayerPainVisual_reset(void);
int EspNativePlayerPainVisual_stage(uint32_t probe,
                                   EspNativePlayerPainSide side,
                                   uint32_t nowMs);
int EspNativePlayerPainVisual_cancel(uint32_t probe);
int EspNativePlayerPainVisual_commit(uint32_t probe);
int EspNativePlayerPainVisual_isActive(uint32_t nowMs);
uint32_t EspNativePlayerPainVisual_tag(uint32_t nowMs);
int EspNativePlayerPainVisual_expiryDue(uint32_t nowMs);
int EspNativePlayerPainVisual_ackExpiry(uint32_t nowMs);

/* DoomCanvas_startShake(500,2,150) uses [-2,+1] offsets on both axes.
 * A 75ms bounded presentation cadence keeps the game RNG untouched. */
#define ESP_NATIVE_PLAYER_PAIN_SHAKE_STEP_MS 75U
int EspNativePlayerPainVisual_shakeOffsets(uint32_t nowMs,
                                          int8_t* outX, int8_t* outY,
                                          uint8_t* outStep);
int EspNativePlayerPainVisual_shakeRefreshDue(uint32_t nowMs);
int EspNativePlayerPainVisual_shakeComposed(uint8_t step);
int EspNativePlayerPainVisual_shiftViewport(uint16_t* framebuffer,
                                             uint32_t width, uint32_t height,
                                             int8_t dx, int8_t dy);

/* Exact four Hud_drawEffects() DoomRPG_fillRect calls: RGB #BB0000,
 * viewport x=0..159, y=20..99, source retains x=159,y=99 corner. */
int EspNativePlayerPainVisual_paintBorder(uint16_t* framebuffer,
                                         uint32_t width, uint32_t height,
                                         uint32_t nowMs);

/* Exact Hud_drawBottomBar face indices and Render_getSpanMode palette
 * saturating channel additions, converted to the native RGB565 order. */
uint8_t EspNativePlayerPainVisual_face(uint8_t health, uint8_t maxHealth,
                                      uint8_t gotFace, uint32_t nowMs);
uint16_t EspNativePlayerPainVisual_weaponColor(uint16_t rgb565);

/* Distinct, source-backed Combat_spawnDogBloodParticles family. The familiar
 * particle layer is permitted ONLY for a successfully resolved familiar
 * absorption; an enemy dog attack never selects it. These functions extend
 * the existing reversible visual lease, not the game or ammunition owner. */
int EspNativePlayerPainVisual_stageDog(uint32_t probe,
                                      EspNativePlayerPainSide side,
                                      uint32_t nowMs);
int EspNativePlayerPainVisual_dogInfo(uint32_t nowMs,
                                     uint32_t* outSeed,
                                     uint32_t* outAgeMs);

/* The original Hud_drawEffects n.bmp strip is three 18x18 frames.
 * 0x30 centers the frame at its supplied screen position. */
int EspNativePlayerPainVisual_arrow(uint32_t nowMs,
                                    int16_t* outX, int16_t* outY,
                                    uint16_t* outSourceY);

#ifdef __cplusplus
}
#endif
#endif
