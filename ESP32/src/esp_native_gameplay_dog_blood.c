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
    /* ParticleSystem_calculateParticles() calls randNextInt()%6 once for
     * randNext even with z=0, before the 5 draws per live particle. It uses
     * (randNextInt() & 0xffff) % range for starts, speeds and sizes. Preserve
     * that cadence and low-16 distribution using ONLY the visual RNG. */
    (void)nextVisual(&seed);
    for (i = 0U; i <= index; ++i) {
        const uint32_t sx = nextVisual(&seed) & 0xffffU;
        const uint32_t sy = nextVisual(&seed) & 0xffffU;
        const uint32_t vx = nextVisual(&seed) & 0xffffU;
        const uint32_t vy = nextVisual(&seed) & 0xffffU;
        const uint32_t sz = nextVisual(&seed) & 0xffffU;
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

static uint32_t drawLine(uint16_t* fb, int x0, int x1, int y,
                         uint16_t color) {
    uint32_t n = 0U;
    int x;
    if (y < VIEW_TOP || y > VIEW_BOTTOM) return 0U;
    if (x0 < 0) x0 = 0;
    if (x1 >= FRAME_WIDTH) x1 = FRAME_WIDTH - 1;
    for (x = x0; x <= x1; ++x) {
        fb[y * FRAME_WIDTH + x] = color;
        ++n;
    }
    return n;
}

/* Reproduce SDL_RenderDrawFillCircle from src/SDL_Video.c (four horizontal
 * spans per midpoint iteration). A Euclidean radius-2 disc has only 13
 * pixels, whereas the desktop algorithm draws 21; never approximate it. */
static uint32_t paintOriginalCircle(uint16_t* fb, int x, int y, int r,
                                    uint16_t color) {
    int dx = r, dy = 0, accum = dx - 1;
    uint32_t pixels = 0U;
    while (dy <= dx) {
        pixels += drawLine(fb, x - dx, x + dx, y + dy, color);
        pixels += drawLine(fb, x - dy, x + dy, y + dx, color);
        pixels += drawLine(fb, x - dx, x + dx, y - dy, color);
        pixels += drawLine(fb, x - dy, x + dy, y - dx, color);
        ++dy;
        accum -= (dy << 1) - 1;
        if (accum < 0) {
            --dx;
            accum += dx << 1;
        }
    }
    return pixels;
}

uint32_t EspNativeDogBlood_paint(uint16_t* framebuffer, uint32_t width,
                                 uint32_t height, uint32_t seed,
                                 uint32_t ageMs, uint8_t berserker) {
    uint32_t pixels = 0U;
    const uint16_t color = berserker != 0U
        ? ESP_NATIVE_DOG_BLOOD_BERSERK_COLOR565
        : ESP_NATIVE_DOG_BLOOD_COLOR565;
    uint8_t i;
    if (framebuffer == NULL || width != FRAME_WIDTH ||
        height != FRAME_HEIGHT ||
        ageMs >= ESP_NATIVE_DOG_BLOOD_WINDOW_MS) return 0U;
    for (i = 0U; i < ESP_NATIVE_DOG_BLOOD_COUNT; ++i) {
        EspNativeDogBloodParticle particle;
        int16_t x, y;
        uint8_t radius;
        if (!EspNativeDogBlood_sample(seed, i, &particle)) return 0U;
        EspNativeDogBlood_project(&particle, ageMs, &x, &y, &radius);
        /* Fill geometry is the exact source SDL midpoint algorithm.
         * The original screenspace bounds are the 20..99 gameplay viewport. */
        pixels += paintOriginalCircle(framebuffer, x, y, radius, color);
    }
    return pixels;
}
