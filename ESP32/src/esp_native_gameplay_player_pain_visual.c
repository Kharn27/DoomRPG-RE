#include <stdint.h>
#include <string.h>

#include "esp_native_gameplay_player_pain_visual.h"

enum { PAIN_IDLE = 0, PAIN_STAGED = 1, PAIN_COMMITTED = 2 };

typedef struct PainLease_s {
    uint32_t probe;
    uint32_t begunMs;
    uint32_t generation;
    uint8_t side;
    uint8_t phase;
    uint8_t expired;
    uint8_t reserved;
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

int EspNativePlayerPainVisual_isActive(uint32_t nowMs) {
    return lease.phase != PAIN_IDLE && lease.expired == 0U &&
           (uint32_t)(nowMs - lease.begunMs) < ESP_NATIVE_PLAYER_PAIN_MS;
}

uint32_t EspNativePlayerPainVisual_tag(uint32_t nowMs) {
    return (lease.generation << 1U) |
           (uint32_t)EspNativePlayerPainVisual_isActive(nowMs);
}

int EspNativePlayerPainVisual_expiryDue(uint32_t nowMs) {
    return lease.phase == PAIN_COMMITTED && lease.expired == 0U &&
           (uint32_t)(nowMs - lease.begunMs) >= ESP_NATIVE_PLAYER_PAIN_MS;
}

int EspNativePlayerPainVisual_ackExpiry(uint32_t nowMs) {
    if (!EspNativePlayerPainVisual_expiryDue(nowMs)) return 0;
    lease.expired = 1U;
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
