#ifndef DOOMRPG_ESP32_MAP_LINE_STATE_H
#define DOOMRPG_ESP32_MAP_LINE_STATE_H

#include <stdint.h>

#include "esp_map_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_OPENLINE 15U
#define ESP_MAP_OPCODE_CLOSELINE 16U

#define ESP_MAP_LINE_FLAG_OPEN 0x00000040UL
#define ESP_MAP_LINE_FLAG_LOCKED 0x00000400UL
#define ESP_MAP_COMMAND_FLAG_REMOVE 0x00000200UL

#define ESP_MAP_LINE_SOUND_OPEN 5063U
#define ESP_MAP_LINE_SOUND_CLOSE 5064U

#define ESP_MAP_LINE_EFFECT_DOOR_ANIMATION 0x01U
#define ESP_MAP_LINE_EFFECT_ENTITY_RELINK  0x02U
#define ESP_MAP_LINE_EFFECT_PLAY_SOUND     0x04U
#define ESP_MAP_LINE_EFFECT_ALL            0x07U

typedef struct EspMapLineStateView_s {
    const uint8_t* openBits;
    const uint8_t* lockedBits;
    uint32_t lineCount;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t openCount;
    uint32_t lockedCount;
} EspMapLineStateView;

typedef enum EspMapLineDoorStatus_e {
    ESP_MAP_LINE_DOOR_INVALID = 0,
    ESP_MAP_LINE_DOOR_UNSUPPORTED = 1,
    ESP_MAP_LINE_DOOR_NOT_READY = 2,
    ESP_MAP_LINE_DOOR_LINE_OUT_OF_RANGE = 3,
    ESP_MAP_LINE_DOOR_LOCKED = 4,
    ESP_MAP_LINE_DOOR_ALREADY_TARGET = 5,
    ESP_MAP_LINE_DOOR_OK = 6
} EspMapLineDoorStatus;

typedef struct EspMapLineDoorResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint8_t sourceCommandOffset;
    uint8_t codeId;
    uint8_t openBefore;
    uint8_t openAfter;
    uint8_t locked;
    uint8_t mutated;
    uint8_t effectFlags;
    uint8_t removeCommandIfHandled;
} EspMapLineDoorResult;

/*
 * Own the compact mutable line-door overlay for the current immutable map.
 * One packed bit per line tracks openness and one tracks the lock predicate.
 * The immutable line geometry, texture and all other flags remain in
 * EspMapRuntime. No legacy Render/Entity objects are allocated or referenced.
 */
void EspMapLineState_reset(void);
int EspMapLineState_buildFromRuntime(void);
int EspMapLineState_isReady(void);
const EspMapLineStateView* EspMapLineState_view(void);
int EspMapLineState_getOpen(uint32_t lineIndex, uint8_t* outOpen);
int EspMapLineState_getLocked(uint32_t lineIndex, uint8_t* outLocked);

/*
 * Small permanent world-state primitives used by door/script/gameplay owners.
 * They update only the packed overlay and its count/FNV witnesses.
 */
int EspMapLineState_setOpen(uint32_t lineIndex, uint8_t open);
int EspMapLineState_setLocked(uint32_t lineIndex, uint8_t locked);

/*
 * Execute only real 15/EV_OPENLINE and 16/EV_CLOSELINE semantics against the
 * native line overlay. A successful transition owns only the canonical open
 * bit mutation; animation, legacy collision-entity relinking and sound are
 * returned as effect flags for later native consumers and are not performed.
 *
 * LOCKED/already-target are semantic no-ops matching Game_performDoorEvent()
 * returning false. OK corresponds to its true return. removeCommandIfHandled
 * mirrors the outer Game_runEvent() 0x200 removal condition without mutating
 * EspMapScriptState yet.
 */
EspMapLineDoorStatus EspMapLineState_applyDoorCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineDoorResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
