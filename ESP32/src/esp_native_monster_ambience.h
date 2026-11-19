#ifndef DOOMRPG_ESP32_NATIVE_MONSTER_AMBIENCE_H
#define DOOMRPG_ESP32_NATIVE_MONSTER_AMBIENCE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Audio-only, map-session, no heap, no render, no gameplay RNG mutation.
 * Sight: one random variant for the highest-subtype newly activated monster,
 * one event per newly observed frame group (legacy Game_activate/soundMonster).
 * Idle: one active living monster at a legacy-like sparse random interval.
 * Restored activation prefix does not replay old sight cries. */
/* Presentation-only 350ms guard for EVERY newly activated member of the\n * one-sight-cry render cohort (not only the highest-subtype cry winner).\n * A restored activation prefix and SOUND OFF never arm a new guard.\n * No enemy-turn, renderer, save ABI, heap or gameplay RNG mutation. */
int EspNativeMonsterAmbience_sightSettling(uint16_t spriteIndex,
                                          uint32_t nowMs);
void EspNativeMonsterAmbience_service(uint32_t nowMs,
                                     int gameplayActive,
                                     int idleAllowed);
#ifdef __cplusplus
}
#endif
#endif
