#ifndef ESP_NATIVE_GAMEPLAY_MONSTER_SUBTYPE10_POLICY_H
#define ESP_NATIVE_GAMEPLAY_MONSTER_SUBTYPE10_POLICY_H

#include <stdint.h>

/* Exact Entity_aiThink() subtype 10 gate: the byte is drawn once on each
 * stationary AI turn, before testing the circular inactive-monster list.
 * Never draw a second byte per corpse. */
#define ESP_NATIVE_SUBTYPE10_RAISE_THRESHOLD 84U
#define ESP_NATIVE_SUBTYPE10_RAISE_RADIUS_SQUARED 0x4000U
#define ESP_NATIVE_SUBTYPE10_RAISE_OCCUPANCY_MASK 16263U

static inline int EspNativeSubtype10_wantsRaise(uint8_t randomByte) {
    return randomByte < ESP_NATIVE_SUBTYPE10_RAISE_THRESHOLD;
}

/* Pure model of Entity_aiThink's per-inactive-monster filter. dx,dy are
 * signed world units (not tiles). Caller owns the ORIGINAL inactive-list
 * order, corpse/no-raise flags and Game_findMapEntityXYFlag(mask=16263).
 * The native P1 caller uses this only to identify an UNSUPPORTED raise
 * candidate and fail closed, never to apply an approximate resurrection. */
static inline int EspNativeSubtype10_candidateEligible(
    int32_t dx, int32_t dy, uint8_t corpseFlag,
    uint8_t alreadyTargeted, uint8_t occupied) {
    const int64_t squared = (int64_t)dx * dx + (int64_t)dy * dy;
    return squared != 0 && squared <=
               (int64_t)ESP_NATIVE_SUBTYPE10_RAISE_RADIUS_SQUARED &&
           corpseFlag != 0U && alreadyTargeted == 0U && occupied == 0U;
}

#endif
