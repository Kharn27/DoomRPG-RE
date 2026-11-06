#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_native_gameplay_dog_blood.h"

#define VIEW_TOP 20
#define VIEW_BOTTOM 99
#define FRAME_WIDTH 160
#define FRAME_HEIGHT 120

static uint32_t nextVisual(uint32_t* state) {
    uint32_t x = *state;
    if (x == 0U) x = 0x6d2b79f5U;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = x;
    return x;
}

int EspNativeDogBlood_sample(uint32_t seed, uint8_t index,
                             EspNativeDogBloodParticle* out) {
    uint8_t i;
    if (out == NULL || index >= ESP_NATIVE_DOG_BLOOD_COUNT) return 0;
    /* Each ParticleNode is reconstructed in the legacy order of its five
     * independent draws. The original consumes the gameplay RNG; the native
     * render MUST NOT, so the identical bounded ranges use local visual RNG. */
    for (i = 0U; i <= index; ++i) {
        const uint32_t sx = nextVisual(&seed);
        const uint32_t sy = nextVisual(&seed);
        const uint32_t vx = nextVisual(&seed);
        const uint32_t vy = nextVisual(&seed);
        const uint32_t sz = nextVisual(&seed);
        if (i == index) {
            out->startX = (int16_t)(10 + (sx % 21U));
            out->startY = (int16_t)(15 + (sy % 21U));
            out->velX = (int16_t)(-150 + (int)(vx % 226U));
            out->velY = (int16_t)(-100 + (int)(vy % 51U));
            out->size = (uint8_t)(1U + sz % 4U);
            out->reserved = 0U;
        }
    }
    return 1;
}

void EspNativeDogBlood_project(const EspNativeDogBloodParticle* particle,
                                uint32_t ageMs, int16_t* outX,
                                int16_t* outY, uint8_t* outRadius) {
    int32_t x, y, a;
    if (particle == NULL || outX == NULL || outY == NULL ||
        outRadius == NULL) return;
    if (ageMs > ESP_NATIVE_DOG_BLOOD_WINDOW_MS)
        ageMs = ESP_NATIVE_DOG_BLOOD_WINDOW_MS;
    a = (int32_t)ageMs;
    /* ParticleSystem_render() uses fixed point, v/1000 and gravity
     * 8 * elapsed^2 / 25600. The original integrates incrementally per
     * draw; evaluate one bounded elapsed sample here so our picture does
     * not depend on TFT present FPS or consume gameplay random bytes. */
    x = 80 + particle->startX + (particle->velX * a) / 1000;
    y = 60 + particle->startY + (particle->velY * a) / 1000 +
        (8 * a * a) / 25600;
    *outX = (int16_t)x;
    *outY = (int16_t)y;
    *outRadius = particle->size >> 1U;
}

uint32_t EspNativeDogBlood_paint(uint16_t* framebuffer, uint32_t width,
                                 uint32_t height, uint32_t seed,
                                 uint32_t ageMs) {
    uint32_t pixels = 0U;
    uint8_t i;
    if (framebuffer == NULL || width != FRAME_WIDTH ||
        height != FRAME_HEIGHT ||
        ageMs >= ESP_NATIVE_DOG_BLOOD_WINDOW_MS) return 0U;
    for (i = 0U; i < ESP_NATIVE_DOG_BLOOD_COUNT; ++i) {
        EspNativeDogBloodParticle particle;
        int16_t x, y;
        uint8_t radius;
        int dy, dx;
        if (!EspNativeDogBlood_sample(seed, i, &particle)) return 0U;
        EspNativeDogBlood_project(&particle, ageMs, &x, &y, &radius);
        /* DoomRPG_fillCircle(radius = particleSize>>1) is the legacy
         * geometry. Clipping to the original gameplay viewport ensures the
         * palette/HUD overlays retain their own owners. */
        for (dy = -(int)radius; dy <= (int)radius; ++dy) {
            const int py = y + dy;
            if (py < VIEW_TOP || py > VIEW_BOTTOM) continue;
            for (dx = -(int)radius; dx <= (int)radius; ++dx) {
                const int px = x + dx;
                if (px < 0 || px >= FRAME_WIDTH ||
                    dx * dx + dy * dy > radius * radius) continue;
                framebuffer[py * FRAME_WIDTH + px] =
                    ESP_NATIVE_DOG_BLOOD_COLOR565;
                ++pixels;
            }
        }
    }
    return pixels;
}
