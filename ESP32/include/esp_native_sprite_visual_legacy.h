#ifndef DOOMRPG_ESP32_SPRITE_VISUAL_LEGACY_H
#define DOOMRPG_ESP32_SPRITE_VISUAL_LEGACY_H

#include <stdint.h>
#include <limits.h>

/* Visual-only projection parity with desktop Render_loadMap's one-unit
 * adjustment for wall-attached sprites. The compact BSP arena and mutable
 * monster/save owners retain unmodified map coordinates. All four directions
 * and priority are deliberately identical to src/Render.c. */
#define ESP_SPRITE_VISUAL_WALL_ATTACHED 0x00800000UL

static inline void EspNativeSpriteVisual_coordinates(
    uint16_t rawX, uint16_t rawY, uint32_t info,
    int* outX, int* outY) {
    int x = (int)rawX, y = (int)rawY;
    if ((info & ESP_SPRITE_VISUAL_WALL_ATTACHED) != 0U) {
        if ((info & 0x00100000UL) != 0U) ++y;
        else if ((info & 0x00080000UL) != 0U) --y;
        else if ((info & 0x00200000UL) != 0U) ++x;
        else if ((info & 0x00400000UL) != 0U) --x;
    }
    *outX = x;
    *outY = y;
}

/* Legacy Render_walkNode sprite sort parity, including wall-attached front
 * priority and the special sprite-ID interval [184,190]. The caller passes
 * visual (nudged) coordinates, not the unmodified immutable map position. */
static inline int32_t EspNativeSpriteVisual_sortDepth(
    int x, int y, uint32_t info, uint8_t type,
    int viewCos_, int viewSin_, int viewTransX) {
    int32_t z;
    const uint32_t id = info & 511U;
    if ((info & ESP_SPRITE_VISUAL_WALL_ATTACHED) != 0U) {
        return INT32_MAX;
    }
    z = (int32_t)(x * viewCos_ + y * viewSin_ + viewTransX);
    if ((info & 0x01000000UL) != 0U) ++z;
    else if (type == 1U) --z;
    else if (id >= 184U && id <= 190U) z = 0;
    else if (id >= 180U && id <= 191U) z -= 2;
    return z;
}

#endif
