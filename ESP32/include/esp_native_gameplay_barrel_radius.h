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

/* Consequence RNG draws are interleaved between barrel blast words as soon
 * as a chain has multiple barrels and any monster in a radius cell.
 * Preview exactly the first explosion word, then consume each later word
 * LIVE after the preceding monster's pain/death/drop RNG side effects. */
static inline uint8_t EspNativeGameplayBarrelRadius_previewWords(
    uint8_t barrelCount, uint8_t monsterPreflightCount) {
    return barrelCount > 1U && monsterPreflightCount != 0U
               ? 1U : barrelCount;
}

/* Terminal death may be committed after the entire explosion transaction
 * only if exactly one barrel's blast word participates. Adjacent monster
 * splash is supported because the death RNG byte is pre-reserved before its
 * consequence rolls; multi-barrel death remains fail-closed until subsequent
 * blast/death ordering has a dedicated replay milestone. */
static inline int EspNativeGameplayBarrelRadius_terminalDeathOwned(
    uint8_t barrelCount) {
    return barrelCount == 1U;
}

#endif
