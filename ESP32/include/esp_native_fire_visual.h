#ifndef DOOMRPG_ESP32_NATIVE_FIRE_VISUAL_H
#define DOOMRPG_ESP32_NATIVE_FIRE_VISUAL_H

#include <stdint.h>

/* Entity_initspawn() (src/Entity.c): eType==10 is the Fire family and sets
 * sprite->renderMode=7. Keep the recovered ID-specific defaults supplied by
 * the caller; only a real BSP fire entity overrides its base draw mode.
 *
 * CROSS billboards are the separately recovered four-copy render path.
 * Glow companions and non-BSP transients are not passed through this helper.
 * No render state, asset data, or retained allocation is introduced. */
static inline uint8_t EspNativeFireVisual_mapMode(
    uint8_t originalMode, uint8_t entityType, uint32_t spriteInfo) {
    if (entityType == 10U && (spriteInfo & 0x04000000UL) == 0U) {
        return 7U;
    }
    return originalMode;
}

#endif
