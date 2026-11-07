#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_native_gameplay_player_pain_visual.h"

enum { PAIN_IDLE = 0, PAIN_STAGED = 1, PAIN_COMMITTED = 2,
       PAIN_VISIBLE = 3 };

typedef struct PainLease_s {
    uint32_t probe;
    uint32_t begunMs;
    uint32_t generation;
    uint8_t side;
    uint8_t phase;
    uint8_t expired;
    uint8_t dogBlood;
    uint8_t lastShakeStep;
    uint8_t padding[3];
} PainLease;

/* The second tiny record is the complete pre-stage rollback, including an
 * older still-visible hit. No per-impact malloc, timer, second framebuffer or
 * random state. A generation is only a presentation cache key. */
static PainLease lease;
static PainLease beforeStage;
static uint32_t nextGeneration;

void EspNativePlayerPainVisual_reset(void) {
    memset(&lease, 0, sizeof(lease));
    memset(&beforeStage, 0, sizeof(beforeStage));
    nextGeneration = 0U;
}

int EspNativePlayerPainVisual_stage(uint32_t probe,
                                   EspNativePlayerPainSide side,
                                   uint32_t nowMs) {
    if (probe == 0U || side > ESP_NATIVE_PLAYER_PAIN_LEFT ||
        lease.phase == PAIN_STAGED) return 0;
    beforeStage = lease;
    lease.probe = probe;
    lease.begunMs = nowMs;
    lease.generation = ++nextGeneration;
    if (lease.generation == 0U) lease.generation = ++nextGeneration;
    lease.side = (uint8_t)side;
    lease.phase = PAIN_STAGED;
    lease.expired = 0U;
    lease.dogBlood = 0U;
    lease.lastShakeStep = UINT8_MAX;
    memset(lease.padding, 0, sizeof(lease.padding));
    return 1;
}

int EspNativePlayerPainVisual_stageDog(uint32_t probe,
                                      EspNativePlayerPainSide side,
                                      uint32_t nowMs) {
    if (!EspNativePlayerPainVisual_stage(probe, side, nowMs)) return 0;
    lease.dogBlood = 1U;
    return 1;
}

int EspNativePlayerPainVisual_dogInfo(uint32_t nowMs,
                                     uint32_t* outSeed,
                                     uint32_t* outAgeMs) {
    if (outSeed == NULL || outAgeMs == NULL || lease.dogBlood == 0U ||
        !EspNativePlayerPainVisual_isActive(nowMs)) return 0;
    *outSeed = 0x7bd1a5e3U ^ lease.probe * 0x9e3779b9U;
    *outAgeMs = lease.phase == PAIN_VISIBLE
        ? nowMs - lease.begunMs : 0U;
    return 1;
}

int EspNativePlayerPainVisual_cancel(uint32_t probe) {
    if (lease.phase != PAIN_STAGED || lease.probe != probe) return 0;
    lease = beforeStage;
    memset(&beforeStage, 0, sizeof(beforeStage));
    return 1;
}

int EspNativePlayerPainVisual_commit(uint32_t probe) {
    if (lease.phase != PAIN_STAGED || lease.probe != probe) return 0;
    lease.phase = PAIN_COMMITTED;
    memset(&beforeStage, 0, sizeof(beforeStage));
    return 1;
}

/* Successful gameplay COMMIT is not a visibility witness. With the
 * old clock, the ordered monster movement tail could cost >500ms before
 * its final physical present, so real pain faces, weapon tint and shake
 * would expire without ever being seen (while old damage flash still ran).
 * PAIN_STAGED and PAIN_COMMITTED are intentionally timeless; PAIN_VISIBLE
 * begins at the first actual LCD publish, and only then may expire. */
int EspNativePlayerPainVisual_firstPresented(uint32_t nowMs) {
    if (lease.phase != PAIN_COMMITTED || lease.expired != 0U) return 0;
    lease.phase = PAIN_VISIBLE;
    lease.begunMs = nowMs;
    lease.lastShakeStep = 0U; /* initial displayed viewport = step zero */
    return 1;
}

int EspNativePlayerPainVisual_isActive(uint32_t nowMs) {
    return lease.phase != PAIN_IDLE && lease.expired == 0U &&
           (lease.phase != PAIN_VISIBLE ||
            (uint32_t)(nowMs - lease.begunMs) <
                ESP_NATIVE_PLAYER_PAIN_MS);
}

uint32_t EspNativePlayerPainVisual_tag(uint32_t nowMs) {
    return (lease.generation << 1U) |
           (uint32_t)EspNativePlayerPainVisual_isActive(nowMs);
}

int EspNativePlayerPainVisual_expiryDue(uint32_t nowMs) {
    return lease.phase == PAIN_VISIBLE && lease.expired == 0U &&
           (uint32_t)(nowMs - lease.begunMs) >= ESP_NATIVE_PLAYER_PAIN_MS;
}

int EspNativePlayerPainVisual_ackExpiry(uint32_t nowMs) {
    if (!EspNativePlayerPainVisual_expiryDue(nowMs)) return 0;
    lease.expired = 1U;
    return 1;
}

/* The source DoomCanvas_drawRGB uses randNextByte()%(shakeVal*2)
 * minus shakeVal: shakeVal=2 means [-2,-1,0,+1]. We sample a local
 * visual generator: never perturb gameplay Random_t, and make each
 * 75ms sample stable across concurrent presentation owners. */
static uint32_t shakeVisualNext(uint32_t* visual) {
    uint32_t x = *visual;
    if (x == 0U) x = 0x6d2b79f5U;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *visual = x;
    return x;
}

int EspNativePlayerPainVisual_shakeOffsets(uint32_t nowMs,
                                          int8_t* outX, int8_t* outY,
                                          uint8_t* outStep) {
    uint32_t visual;
    uint8_t step;
    if (outX == NULL || outY == NULL || outStep == NULL ||
        !EspNativePlayerPainVisual_isActive(nowMs)) return 0;
    step = lease.phase == PAIN_VISIBLE
        ? (uint8_t)((nowMs - lease.begunMs) /
                    ESP_NATIVE_PLAYER_PAIN_SHAKE_STEP_MS)
        : 0U;
    visual = 0x8e52a79bU ^ (lease.probe * 0x9e3779b9U) ^
             (lease.generation * 0x85ebca6bU) ^
             ((uint32_t)step * 0xc2b2ae35U);
    *outX = (int8_t)((int)(shakeVisualNext(&visual) & 3U) - 2);
    *outY = (int8_t)((int)(shakeVisualNext(&visual) & 3U) - 2);
    *outStep = step;
    return 1;
}

int EspNativePlayerPainVisual_shakeRefreshDue(uint32_t nowMs) {
    int8_t x, y;
    uint8_t step;
    if (lease.phase != PAIN_VISIBLE ||
        !EspNativePlayerPainVisual_shakeOffsets(nowMs, &x, &y, &step))
        return 0;
    return lease.lastShakeStep == UINT8_MAX ||
           step > lease.lastShakeStep;
}

int EspNativePlayerPainVisual_shakeComposed(uint8_t step) {
    if (lease.phase == PAIN_IDLE || lease.expired != 0U ||
        step > (ESP_NATIVE_PLAYER_PAIN_MS - 1U) /
               ESP_NATIVE_PLAYER_PAIN_SHAKE_STEP_MS) return 0;
    lease.lastShakeStep = step;
    return 1;
}

/* Only rows [20,100) may move. Vertical copy order prevents overwriting
 * source rows; horizontal memmove accounts for two-pixel overlap.
 * Uncovered pixels are black instead of wrapping the opposite edge. */
int EspNativePlayerPainVisual_shiftViewport(uint16_t* framebuffer,
                                             uint32_t width, uint32_t height,
                                             int8_t dx, int8_t dy) {
    const int top = 20, bottom = 100, w = 160;
    int y;
    if (framebuffer == NULL || width != 160U || height != 120U ||
        dx < -2 || dx > 1 || dy < -2 || dy > 1) return 0;
    if (dy > 0) {
        for (y = bottom - 1; y >= top; --y) {
            uint16_t* row = framebuffer + (size_t)y * w;
            if (y - dy >= top)
                memmove(row, framebuffer + (size_t)(y - dy)*w,
                        (size_t)w * sizeof(uint16_t));
            else memset(row, 0, (size_t)w * sizeof(uint16_t));
        }
    } else if (dy < 0) {
        for (y = top; y < bottom; ++y) {
            uint16_t* row = framebuffer + (size_t)y * w;
            if (y - dy < bottom)
                memmove(row, framebuffer + (size_t)(y - dy)*w,
                        (size_t)w * sizeof(uint16_t));
            else memset(row, 0, (size_t)w * sizeof(uint16_t));
        }
    }
    for (y = top; y < bottom; ++y) {
        uint16_t* row = framebuffer + (size_t)y * w;
        if (dx > 0) {
            memmove(row + dx, row, (size_t)(w - dx)*sizeof(uint16_t));
            memset(row, 0, (size_t)dx * sizeof(uint16_t));
        } else if (dx < 0) {
            const int d = -dx;
            memmove(row, row + d, (size_t)(w - d)*sizeof(uint16_t));
            memset(row + w - d, 0, (size_t)d*sizeof(uint16_t));
        }
    }
    return 1;
}

int EspNativePlayerPainVisual_paintBorder(uint16_t* framebuffer,
                                         uint32_t width, uint32_t height,
                                         uint32_t nowMs) {
    int x, y;
    const uint16_t red = 0xb800U; /* RGB888 #BB0000 -> RGB565 */
    if (framebuffer == NULL || width != 160U || height != 120U ||
        !EspNativePlayerPainVisual_isActive(nowMs)) return 0;
    /* Preserve the source rectangle edges literally:
     * left/right 2x79 at x=0,158 and y=20..98;
     * top/bottom 159x2 at y=20,98 and x=0..158.
     * Pixel (159,99) is intentionally untouched in the original. */
    for (y=20;y<99;++y) {
        framebuffer[y*160+0] = red;
        framebuffer[y*160+1] = red;
        framebuffer[y*160+158] = red;
        framebuffer[y*160+159] = red;
    }
    for (x=0;x<159;++x) {
        framebuffer[20*160+x] = red;
        framebuffer[21*160+x] = red;
        framebuffer[98*160+x] = red;
        framebuffer[99*160+x] = red;
    }
    return 1;
}

uint8_t EspNativePlayerPainVisual_face(uint8_t health, uint8_t maxHealth,
                                      uint8_t gotFace, uint32_t nowMs) {
    /* Hud_drawBottomBar: gotFace always wins if health > 0. The 500ms pain
     * direction only owns an ALIVE face, not the dead/health-tier sprites. */
    if (health > 0U && gotFace != 0U) return 8U;
    if (health == 0U || !EspNativePlayerPainVisual_isActive(nowMs)) {
        if (health <= (uint8_t)(maxHealth / 4U)) return 3U;
        if (health <= (uint8_t)(maxHealth / 3U)) return 2U;
        if (health <= (uint8_t)(maxHealth / 2U)) return 1U;
        return 0U;
    }
    switch ((EspNativePlayerPainSide)lease.side) {
    case ESP_NATIVE_PLAYER_PAIN_RIGHT: return 6U;
    case ESP_NATIVE_PLAYER_PAIN_BACK: return 7U;
    case ESP_NATIVE_PLAYER_PAIN_LEFT: return 5U;
    default: return 4U; /* FRONT / unknown */
    }
}

uint16_t EspNativePlayerPainVisual_weaponColor(uint16_t rgb565) {
    /* Render_getSpanMode(): +8 red/blue (5-bit), +16 green (6-bit),
     * saturating. Source renderer uses BGR565; native palette is RGB565. */
    uint16_t r = (rgb565 >> 11U) & 31U;
    uint16_t g = (rgb565 >> 5U) & 63U;
    uint16_t b = rgb565 & 31U;
    r = r + 8U > 31U ? 31U : r + 8U;
    g = g + 16U > 63U ? 63U : g + 16U;
    b = b + 8U > 31U ? 31U : b + 8U;
    return (uint16_t)((r << 11U) | (g << 5U) | b);
}

int EspNativePlayerPainVisual_arrow(uint32_t nowMs,
                                    int16_t* outX, int16_t* outY,
                                    uint16_t* outSourceY) {
    int16_t x, y;
    uint16_t sourceY;
    if (outX == NULL || outY == NULL || outSourceY == NULL ||
        !EspNativePlayerPainVisual_isActive(nowMs)) return 0;
    switch ((EspNativePlayerPainSide)lease.side) {
    case ESP_NATIVE_PLAYER_PAIN_RIGHT:
        x = 160 - 20 - 9; y = 120 / 2 - 9; sourceY = 36U;
        break;
    case ESP_NATIVE_PLAYER_PAIN_LEFT:
        x = 20 - 9; y = 120 / 2 - 9; sourceY = 18U;
        break;
    case ESP_NATIVE_PLAYER_PAIN_BACK:
        x = 160 / 2 - 9; y = (120 - 20) - 20 - 9; sourceY = 0U;
        break;
    default: return 0;
    }
    *outX = x;
    *outY = y;
    *outSourceY = sourceY;
    return 1;
}
