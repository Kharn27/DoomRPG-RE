#ifndef DOOMRPG_ESP32_MAP_COMMITTED_TRANSITION_H
#define DOOMRPG_ESP32_MAP_COMMITTED_TRANSITION_H

#include <stdint.h>

#include "esp_bsp_reader.h"
#include "esp_map_change_map_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_transition_preflight.h"
#include "esp_stats_menu_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_COMMITTED_TRANSITION_PHASE_EMPTY 0U
#define ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS 1U
#define ESP_MAP_COMMITTED_TRANSITION_PHASE_READY 2U
#define ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED 3U
#define ESP_MAP_COMMITTED_TRANSITION_PHASE_ROLLED_BACK 4U
#define ESP_MAP_COMMITTED_TRANSITION_PHASE_FAILED 5U

typedef enum EspMapCommittedTransitionStatus_e {
    ESP_MAP_COMMITTED_TRANSITION_INVALID = 0,
    ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS = 1,
    ESP_MAP_COMMITTED_TRANSITION_READY = 2,
    ESP_MAP_COMMITTED_TRANSITION_INVENTORY_MISMATCH = 3,
    ESP_MAP_COMMITTED_TRANSITION_SOURCE_MISMATCH = 4,
    ESP_MAP_COMMITTED_TRANSITION_TARGET_BUILD_FAILED = 5,
    ESP_MAP_COMMITTED_TRANSITION_ROLLED_BACK = 6,
    ESP_MAP_COMMITTED_TRANSITION_RECOVERY_FAILED = 7,
    ESP_MAP_COMMITTED_TRANSITION_OK = 8
} EspMapCommittedTransitionStatus;

/*
 * Pointer-free durable transition owner spanning the stats pause and the
 * destructive resident-map handoff.
 *
 * targetSource* binds the future destructive commit to the exact BSP that was
 * preflighted while the source map was still resident. spawnParam is copied
 * from the recovered CHANGEMAP semantics but is intentionally not consumed by
 * this milestone; player placement remains a later owner.
 */
typedef struct EspMapCommittedTransitionState_s {
    uint32_t targetSourceBytes;
    uint32_t targetSourceCrc32;
    uint32_t targetSourceFNV1a;
    uint32_t spawnParam;

    uint8_t sourceMapId;
    uint8_t targetMapId;
    uint8_t targetGameplayLoadMapId;
    uint8_t menuKind;
    uint8_t phase;
    uint8_t pendingConsumed;
    uint8_t statsAcknowledged;
    uint8_t committed;
} EspMapCommittedTransitionState;

void EspMapCommittedTransition_reset(EspMapCommittedTransitionState* state);
int EspMapCommittedTransition_isCommitted(
    const EspMapCommittedTransitionState* state);

/*
 * Bind the already-proven CHANGEMAP pending state to a preflighted target.
 *
 * On success this consumes pendingChange exactly once by resetting it, matching
 * legacy Game_changeMap() clearing changeMapParam after scheduling either the
 * stats menu or the direct map load. For showStats transitions the resulting
 * phase is WAIT_STATS; otherwise it is READY.
 *
 * Invalid inputs are atomic: neither state nor pendingChange is modified.
 */
EspMapCommittedTransitionStatus EspMapCommittedTransition_begin(
    EspMapCommittedTransitionState* state,
    uint8_t sourceMapId,
    EspMapChangeMapState* pendingChange,
    const EspMapChangeMapResult* changeResult,
    const EspStatsMenuIntent* statsIntent,
    const EspMapTransitionPreflightResult* targetPreflight);

/*
 * Model the user accepting MENU_MAP_STATS / MENU_MAP_STATS_OVERALL.
 * WAIT_STATS -> READY. Repeating the acknowledgement while already READY is
 * harmless and returns READY; all other phases fail closed without mutation.
 */
EspMapCommittedTransitionStatus EspMapCommittedTransition_ackStats(
    EspMapCommittedTransitionState* state);

/*
 * Commit READY -> COMMITTED using the proven resident lifecycle.
 *
 * Both inventories must already exist before this call. They are validated
 * against the live source runtime and the preflight-bound target before any
 * resetAll(). Once validation succeeds the function explicitly releases the
 * source and builds the target. A successful build leaves the target resident.
 *
 * If target construction fails after source release, the function attempts to
 * rebuild the source from sourceInventory. Successful recovery leaves phase
 * ROLLED_BACK; failed recovery leaves phase FAILED. No legacy Game/Menu/Render
 * object, DoomCanvas state or player position is touched here.
 */
EspMapCommittedTransitionStatus EspMapCommittedTransition_commit(
    EspMapCommittedTransitionState* state,
    const EspBspInventory* sourceInventory,
    const EspBspInventory* targetInventory,
    EspMapResidentSnapshot* outTargetSnapshot);

#ifdef __cplusplus
}
#endif

#endif
