#ifndef DOOMRPG_ESP32_MAP_SCRIPT_STATE_H
#define DOOMRPG_ESP32_MAP_SCRIPT_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES 512U

typedef struct EspMapScriptStateView_s {
    const uint8_t* storage;
    uint32_t storageBytes;

    const uint8_t* eventStatesPacked;
    uint32_t eventCount;
    uint32_t eventStateBytes;

    const uint8_t* removedCommandBits;
    uint32_t byteCodeCount;
    uint32_t removedCommandBytes;
} EspMapScriptStateView;

/*
 * Pointer-free persistent checkpoint payload for mutable native script state.
 * Event states and removed-command bits are copied as the compact semantic
 * storage already owned by EspMapScriptState. Runtime identity and exact
 * counts/lengths are carried explicitly; no owner pointer is serialized.
 * The fixed 512-byte payload keeps save stack/IO bounded and fails closed on
 * maps whose compact script owner does not fit this versioned section.
 */
typedef struct EspMapScriptStateSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t eventCount;
    uint32_t byteCodeCount;
    uint16_t eventStateBytes;
    uint16_t removedCommandBytes;
    uint16_t storageBytes;
    uint16_t reserved0;
    uint8_t storage[ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES];
} EspMapScriptStateSnapshot;

/*
 * Mutable script state kept strictly outside the immutable map arena.
 * Event state consumes four bits per source event. Commands removed by the
 * recovered MCODE_FLAG_REMOVE/0x200 behavior consume one bit per bytecode.
 */
void EspMapScriptState_reset(void);
int EspMapScriptState_buildFromRuntime(void);
int EspMapScriptState_isReady(void);
const EspMapScriptStateView* EspMapScriptState_view(void);

int EspMapScriptState_getEventState(uint32_t eventIndex, uint8_t* outState);
int EspMapScriptState_setEventState(uint32_t eventIndex, uint8_t state);

int EspMapScriptState_isCommandRemoved(uint32_t commandIndex,
                                       uint8_t* outRemoved);
int EspMapScriptState_setCommandRemoved(uint32_t commandIndex,
                                        uint8_t removed);

/*
 * Export/import only semantic mutable bytes. Snapshot requires the normal
 * resident script owner to exist. Restore validates the current immutable
 * runtime FNV, event/bytecode counts and exact packed sizes before copying into
 * that existing owner; it never allocates and never mutates unrelated world
 * owners. Unused payload bytes must be zero for deterministic save CRCs.
 */
uint32_t EspMapScriptState_fingerprint(void);
int EspMapScriptState_snapshot(EspMapScriptStateSnapshot* outSnapshot);
int EspMapScriptState_restore(const EspMapScriptStateSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
