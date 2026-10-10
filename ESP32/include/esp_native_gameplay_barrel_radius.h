#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_BARREL_RADIUS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_BARREL_RADIUS_H

#include <stdint.h>

/*
 * Game_hurtEntityAt() acts only on entities with info&0x40000.
 * Entity_initspawn() never marks an eType=0 line as hurtable, regardless of
 * eSubType. Game_loadMapEntities() still links these line entities, so the
 * radius lookup can encounter one first and must treat it as an inert no-op.
 *
 * Legacy also explicitly skips eType=12 subtypes 3 (jammed door) and
 * 4 (power coupling). Other line types remain fail-closed until their
 * gameplay consequences have a native owner.
 */
static inline int EspNativeGameplayBarrelRadius_lineNoop(
    uint8_t type, uint8_t subtype) {
    return type == 0U ||
           (type == 12U && (subtype == 3U || subtype == 4U));
}

#endif
