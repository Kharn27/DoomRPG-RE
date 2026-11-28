#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_EXPLOSION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_EXPLOSION_H

#include <stdint.h>

/* Shared type-1 GSprite radius geometry (Game_radiusHurtEntities, z=0).
 * Pure: no RNG ownership, mutation, storage, rendering, or allocation.
 * The caller consumes one word at the end of the explosion and owns the
 * surrounding world transaction. Ordinals MUST remain W/E/N/S/SW/SE/NE/NW.
 * Use current world coordinates for a moved monster; never its spawn tile.
 */
#define ESP_NATIVE_EXPLOSION_NEIGHBORS 8U

static inline int EspNativeExplosion_neighborTile(
    int32_t worldX, int32_t worldY, uint8_t ordinal, uint16_t* outTile) {
    static const int8_t dx[ESP_NATIVE_EXPLOSION_NEIGHBORS] =
        {-1, 1, 0, 0, -1, 1, 1, -1};
    static const int8_t dy[ESP_NATIVE_EXPLOSION_NEIGHBORS] =
        {0, 0, -1, 1, 1, 1, -1, -1};
    int32_t x, y;
    if (ordinal >= ESP_NATIVE_EXPLOSION_NEIGHBORS || outTile == 0) return 0;
    /* Same tile-origin conversion as the existing native barrel radius. */
    x = (worldX >> 6) + dx[ordinal];
    y = (worldY >> 6) + dy[ordinal];
    if (x < 0 || x >= 32 || y < 0 || y >= 32) return 0;
    *outTile = (uint16_t)(y * 32 + x);
    return 1;
}

static inline uint8_t EspNativeExplosion_damageFromWord(uint32_t word) {
    return (uint8_t)(((word & 0xffU) / 11U) + 5U);
}

static inline uint8_t EspNativeExplosion_component(uint8_t damage,
                                                  uint8_t ordinal) {
    if (ordinal >= ESP_NATIVE_EXPLOSION_NEIGHBORS) return 0U;
    return ordinal < 4U ? damage : (uint8_t)(damage >> 1U);
}
#endif
