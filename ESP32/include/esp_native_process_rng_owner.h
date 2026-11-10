#ifndef DOOMRPG_ESP32_NATIVE_PROCESS_RNG_OWNER_H
#define DOOMRPG_ESP32_NATIVE_PROCESS_RNG_OWNER_H

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Permanent process-lifetime native owner of the exact Random_t table.
 * This pointer must remain stable across map transitions, rollback probes,
 * and replay-guard lease reservations. Never reset on session exit or LOAD.
 * The legacy byte/word generators and hidden seed state are untouched. */
Random_t* EspNativeProcessRng_live(void);

#ifdef __cplusplus
}
#endif

#endif
