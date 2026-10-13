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

/* Stage a synchronous EV_NOTE in the existing candidate/scratch lease.
 * The notebook changes only at commit, after the caller publishes its frame.
 * Removed-command bits remain the enclosing event transaction's responsibility. */
int EspNativeGameplayNotePrefix_prepare(uint16_t eventIndex, uint8_t offset);
void EspNativeGameplayNotePrefix_commit(void);
void EspNativeGameplayNotePrefix_cancel(void);

#ifdef __cplusplus
}
#endif

#endif
