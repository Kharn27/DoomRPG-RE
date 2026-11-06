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

/* Exact Hud_drawBottomBar face indices and Render_getSpanMode palette
 * saturating channel additions, converted to the native RGB565 order. */
uint8_t EspNativePlayerPainVisual_face(uint8_t health, uint8_t maxHealth,
                                      uint8_t gotFace, uint32_t nowMs);
uint16_t EspNativePlayerPainVisual_weaponColor(uint16_t rgb565);

/* The original Hud_drawEffects n.bmp strip is three 18x18 frames.
 * 0x30 centers the frame at its supplied screen position. */
int EspNativePlayerPainVisual_arrow(uint32_t nowMs,
                                    int16_t* outX, int16_t* outY,
                                    uint16_t* outSourceY);

#ifdef __cplusplus
}
#endif
#endif
