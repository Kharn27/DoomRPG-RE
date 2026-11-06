#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_DOG_BLOOD_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_DOG_BLOOD_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact inputs from Combat_spawnDogBloodParticles() in src/Combat.c.
 * This is an immutable sprite-field recipe, NOT enemy-dog claw graphics.
 * Resolution is the 160x120 shared framebuffer; all particles clip to the
 * 20..99 viewport and never touch either HUD band. */
#define ESP_NATIVE_DOG_BLOOD_COUNT 15U
#define ESP_NATIVE_DOG_BLOOD_COLOR565 0x8800U
#define ESP_NATIVE_DOG_BLOOD_WINDOW_MS 500U

typedef struct EspNativeDogBloodParticle_s {
    int16_t startX, startY;
    int16_t velX, velY;
    uint8_t size;
    uint8_t reserved;
} EspNativeDogBloodParticle;

/* Pure deterministic presentation RNG: no DoomRPG Random_t or allocations.
 * Range oracle is source Combat_spawnDogBloodParticles() (15 particles):
 * startX 10..30, startY 15..35, velX -150..75, velY -100..-50,
 * size 1..4, gravity 8, color 0x880000. */
int EspNativeDogBlood_sample(uint32_t seed, uint8_t index,
                             EspNativeDogBloodParticle* out);
void EspNativeDogBlood_project(const EspNativeDogBloodParticle* particle,
                                uint32_t ageMs, int16_t* outX,
                                int16_t* outY, uint8_t* outRadius);
uint32_t EspNativeDogBlood_paint(uint16_t* framebuffer, uint32_t width,
                                 uint32_t height, uint32_t seed,
                                 uint32_t ageMs);
#ifdef __cplusplus
}
#endif
#endif
