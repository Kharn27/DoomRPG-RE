#ifndef ESP_NATIVE_SPECIAL_PLANE_GEOMETRY_H
#define ESP_NATIVE_SPECIAL_PLANE_GEOMETRY_H
#include <stdint.h>
#include "esp_map_runtime.h"
/* Original Render map-load offset before Game_trace reads static sprites.
 * Native map access returns raw BSP coordinates. Apply once at the plane
 * trace consumer, without modifying the arena or the linked tile.
 */
static inline void EspNativeSpecialPlane_legacyPosition(
    const EspMapSprite* sprite, int32_t* x, int32_t* y) {
    *x = (int32_t)sprite->x;
    *y = (int32_t)sprite->y;
    if ((sprite->info & 0x00800000UL) != 0U) {
        if ((sprite->info & 0x00100000UL) != 0U) ++*y;
        else if ((sprite->info & 0x00080000UL) != 0U) --*y;
        else if ((sprite->info & 0x00200000UL) != 0U) ++*x;
        else if ((sprite->info & 0x00400000UL) != 0U) --*x;
    }
}
#endif
