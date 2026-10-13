#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_NOTE_PREFIX_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_NOTE_PREFIX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read-only view of the lazily owned current-map NOTE corpus. Returns an
 * empty string when no notes have been committed. Borrowed pointer: never
 * mutate, retain across map transitions, or free it. No PAK I/O/allocation. */
const char* EspNativeGameplayNotePrefix_text(uint16_t* outLength);
/* Discard the lazy map-local owner on session transition or checkpoint LOAD. */
void EspNativeGameplayNotePrefix_reset(void);

#ifdef __cplusplus
}
#endif

#endif
