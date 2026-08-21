#ifndef DOOMRPG_ESP32_MAP_SCRIPT_STATE_H
#define DOOMRPG_ESP32_MAP_SCRIPT_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
