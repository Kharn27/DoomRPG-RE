#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_BARREL_RADIUS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_BARREL_RADIUS_H

#include <stdint.h>

/*
 * Game_hurtEntityAt() only mutates entities with info&0x40000.
 * Entity_initspawn() never grants it to types 0, 2, 9 or 14; fire
 * (10) and destructible subtypes 3/4 are explicitly skipped by the
 * radius handler. This is a type rule, independent of map or source.
 * A selected inert entity still occludes the remaining tile list.
 * Type 7 needs its definition parm; other effects require native owners.
 */
static inline int EspNativeGameplayBarrelRadius_entityNoop(
    uint8_t type, uint8_t subtype) {
    return type == 0U || type == 2U || type == 9U ||
           type == 10U || type == 14U ||
           (type == 12U && (subtype == 3U || subtype == 4U));
}

static inline int EspNativeGameplayBarrelRadius_lineNoop(
    uint8_t type, uint8_t subtype) {
    return EspNativeGameplayBarrelRadius_entityNoop(type, subtype);
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

/* A player hit can interleave a death byte just like monster consequences.
 * Keep the full preview for multi-blast chains with neither victim family. */
static inline uint8_t EspNativeGameplayBarrelRadius_previewWordsForChain(
    uint8_t barrelCount, uint8_t monsterCount, uint8_t playerCount) {
    return EspNativeGameplayBarrelRadius_previewWords(
        barrelCount, (uint8_t)(monsterCount != 0U || playerCount != 0U));
}

/* Any planned blast may reserve the first death byte. */
static inline int EspNativeGameplayBarrelRadius_terminalDeathOwned(
    uint8_t barrelCount) {
    return barrelCount != 0U;
}

/* blastCursor is zero-based and preflight supplies the expected count.
 * Invalid cursors must not wrap the unsigned remaining count. */
static inline int EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(
    uint8_t barrelCount, uint8_t blastCursor) {
    return blastCursor < barrelCount &&
           EspNativeGameplayBarrelRadius_terminalDeathOwned(
               (uint8_t)(barrelCount - blastCursor));
}

#endif
