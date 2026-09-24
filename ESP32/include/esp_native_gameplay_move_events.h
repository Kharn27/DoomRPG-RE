#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H

#include <stdint.h>

#include "esp_map_ui_intent.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_status_message.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX 4U

typedef enum EspNativeGameplayMoveEventStatus_e {
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT = 2,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_ELIGIBLE = 3,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED = 4,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX = 5,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_LOCKED = 6,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_ALREADY_TARGET = 7,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK = 8,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK = 9,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY = 10,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK = 11,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK = 12
} EspNativeGameplayMoveEventStatus;

typedef struct EspNativeGameplayMoveEventResult_s {
    uint32_t runFlags;
    uint16_t tile;
    uint16_t eventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint16_t targetEventIndex;
    uint8_t commandOffset;
    uint8_t codeId;
    uint8_t eligibleCount;
    uint8_t unsupportedCodeId;
    uint8_t openBefore;
    uint8_t openAfter;
    uint8_t locked;
    uint8_t mutated;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t removeIfHandled;
    uint8_t rollbackAvailable;
    uint8_t stateBefore;
    uint8_t stateAfter;
    uint8_t showBatchCount;
    uint8_t reservedResult;
    union {
        EspNativeGameplayStatusMessageResult statusMessage;
        EspMapShowResult show;
    };
} EspNativeGameplayMoveEventResult;

typedef struct EspNativeGameplayMoveDialogIntent_s {
    uint32_t runFlags;
    uint16_t eventIndex;
    uint8_t commandOffset;
    uint8_t codeId;
} EspNativeGameplayMoveDialogIntent;

/*
 * Execute the bounded movement tile-event families recovered from
 * Game_executeTile()/Game_runEvent(): exactly one eligible regular door
 * OPENLINE/CLOSELINE, FORCE_MESSAGE, dialog command, compact script-state
 * mutation (CHANGESTATE/NEXTSTATE/PREVSTATE), or one homogeneous EV_SHOW batch
 * of at most ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX eligible commands.
 *
 * EV_SHOW batches are fully replay-probed against the compact topology and
 * rolled back exactly before MOVE commit. Their at-most-four rollback records
 * live in one private static MOVE owner rather than in this public result, so
 * the loopTask stack does not grow with the batch journal. The real commit
 * applies SHOWs in legacy order and retains every per-step rollback record plus
 * removed command bit until render/view commit. A second simultaneous SHOW
 * batch, mixed eligible opcode sequences, and larger batches remain fail-closed.
 *
 * Dialog presentation is not performed inside the commit wrapper: an ENTER
 * dialog becomes a tiny pending intent consumed by resident gameplay after the
 * destination world frame has rendered. Script-state mutations use the same
 * permanent EspMapOpcodeExecutor already validated by the native event chain;
 * the MOVE transaction retains only enough before/after state for exact render
 * rollback. CHECK_KEY remains SELECT-owned; MOVE filtering still passes no key
 * selector bits.
 */
EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executePhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult);

int EspNativeGameplayMoveEvents_rollbackPhase(
    const EspNativeGameplayMoveEventResult* result);

const char* EspNativeGameplayMoveEvents_statusName(
    EspNativeGameplayMoveEventStatus status);

/* Integration hooks used by the scoped gameplay MOVE/render transaction. */
void EspNativeGameplayMoveEvents_onFrameResult(int renderOk);

/* A pending dialog is readable only after the destination world frame rendered
 * successfully. finishPendingDialog() closes the rollback lease only after the
 * dialog presenter itself has opened successfully. On presenter failure the
 * caller must rollback the MOVE instead. */
int EspNativeGameplayMoveEvents_pendingDialog(
    uint32_t sequence,
    EspNativeGameplayMoveDialogIntent* outIntent);
int EspNativeGameplayMoveEvents_finishPendingDialog(uint32_t sequence);

/* Read-only census for EXIT SHOW -> ENTER DIALOG candidates. */
int EspNativeGameplayMoveEvents_logShowDialogCandidates(void);

#ifdef __cplusplus
}
#endif

#endif
