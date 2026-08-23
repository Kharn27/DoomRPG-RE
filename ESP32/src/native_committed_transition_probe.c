#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_map_change_map_state.h"
#include "esp_map_committed_transition.h"
#include "esp_map_events.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_transition_preflight.h"
#include "esp_stats_menu_intent.h"
#include "native_committed_transition_probe.h"
#include "native_resident_handoff_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define SOURCE_MAP_ID ESP_MAP_ID_INTRO
#define TARGET_MAP_ID ESP_MAP_ID_JUNCTION
#define SOURCE_RESOURCE "/intro.bsp"
#define TARGET_RESOURCE "/junction.bsp"

#define CHANGE_EVENT_INDEX 1U
#define CHANGE_COMMAND_OFFSET 1U
#define EXPECTED_CHANGE_RAW 0x80000000UL
#define EXPECTED_CHANGE_GLOBAL 2U
#define EXPECTED_CHANGE_STRING_INDEX 0U

#define EXPECTED_STATE_BYTES 24U
#define EXPECTED_WAIT_STATE_FNV 0x66fe636aU
#define EXPECTED_READY_STATE_FNV 0x0ef58ea8U
#define EXPECTED_COMMITTED_STATE_FNV 0x2c595a62U
#define EXPECTED_ROLLED_BACK_STATE_FNV 0x2dec1442U

#define EXPECTED_SOURCE_SNAPSHOT_FNV 0xb3811f3dU
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_PREFLIGHT_FNV 0x108e5c7bU
#define EXPECTED_STATS_INTENT_FNV 0x96afe901U
#define EXPECTED_SOURCE_HEAP_COST 18008U
#define EXPECTED_TARGET_HEAP_COST 10540U
#define EXPECTED_HEAP_GAIN (EXPECTED_SOURCE_HEAP_COST - EXPECTED_TARGET_HEAP_COST)
#define EXPECTED_TARGET_PAYLOAD 10410U

#define EXPECTED_TARGET_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_TARGET_MAP_FNV 0xc5cdfc04U
#define EXPECTED_TARGET_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_TARGET_LINE_FNV 0x3658710dU
#define EXPECTED_TARGET_TEXTURE_FNV 0x537319adU
#define EXPECTED_TARGET_AUTOMAP_FNV 0x0b2ae445U
#define EXPECTED_TARGET_TOPOLOGY_FNV 0xd6e8df7dU

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

static uint32_t hashBytes(const void* data, uint32_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (p == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();

    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }
    return hashBytes(framebuffer, (uint32_t)bytes);
}

static uint32_t transitionWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[9];

    if (doomRpg == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL) return 0U;
    values[0] = (uint32_t)doomRpg->game->changeMapParam;
    values[1] = (uint32_t)doomRpg->game->spawnParam;
    values[2] = (uint32_t)(uint16_t)doomRpg->menu->mapNameId;
    values[3] = (uint32_t)doomRpg->menuSystem->menu;
    values[4] = (uint32_t)doomRpg->doomCanvas->state;
    values[5] = (uint32_t)doomRpg->doomCanvas->storyPage;
    values[6] = (uint32_t)(uint16_t)doomRpg->doomCanvas->loadMapID;
    values[7] = (uint32_t)doomRpg->doomCanvas->loadType;
    values[8] = (uint32_t)doomRpg->doomCanvas->saveType;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t values[8];

    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->totalTime;
    values[1] = (uint32_t)player->totalMoves;
    values[2] = (uint32_t)player->completedLevels;
    values[3] = (uint32_t)player->killedMonstersLevels;
    values[4] = (uint32_t)player->foundSecretsLevels;
    values[5] = (uint32_t)player->berserkerTics;
    values[6] = (uint32_t)(uintptr_t)player->dogFamiliar;
    values[7] = (uint32_t)player->moves;
    return hashBytes(values, sizeof(values));
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int snapshotIsZero(const EspMapResidentSnapshot* snapshot) {
    EspMapResidentSnapshot zero;
    if (snapshot == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(snapshot, &zero, sizeof(zero)) == 0;
}

static int buildRealChange(EspMapChangeMapState* state,
                           EspMapChangeMapResult* result) {
    EspMapEventRef ref;
    EspMapEventDescriptor descriptor;
    uint32_t value;

    if (state == NULL || result == NULL ||
        !EspMapRuntime_getEvent(CHANGE_EVENT_INDEX, &value)) return 0;
    memset(&ref, 0, sizeof(ref));
    ref.index = CHANGE_EVENT_INDEX;
    ref.value = value;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    if (!EspMapEvents_describe(&ref, &descriptor)) return 0;

    EspMapChangeMap_reset(state);
    memset(result, 0, sizeof(*result));
    if (EspMapChangeMap_apply(state, &descriptor, CHANGE_COMMAND_OFFSET,
                              result) != ESP_MAP_CHANGE_MAP_OK) return 0;
    return state->active == 1U && result->rawParam == EXPECTED_CHANGE_RAW &&
           result->spawnParam == 0U && result->showStats == 1U &&
           result->pending == 1U && result->legacyReturnValue == 1U &&
           result->sourceEventIndex == CHANGE_EVENT_INDEX &&
           result->globalCommandIndex == EXPECTED_CHANGE_GLOBAL &&
           result->mapStringIndex == EXPECTED_CHANGE_STRING_INDEX &&
           result->effectFlags ==
               (ESP_MAP_CHANGE_MAP_EFFECT_ADD_LEVEL_STATS |
                ESP_MAP_CHANGE_MAP_EFFECT_SHOW_STATS_MENU);
}

static int sourceSnapshotCanonical(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL && sizeof(*snapshot) == 96U &&
           hashBytes(snapshot, sizeof(*snapshot)) == EXPECTED_SOURCE_SNAPSHOT_FNV;
}

static int targetSnapshotCanonical(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL && snapshot->totalPayloadBytes == EXPECTED_TARGET_PAYLOAD &&
           snapshot->runtimeArenaBytes == 8867U &&
           snapshot->mapStateBytes == 1024U && snapshot->scriptStateBytes == 73U &&
           snapshot->lineStateBytes == 52U && snapshot->textureStateBytes == 26U &&
           snapshot->automapStateBytes == 32U && snapshot->topologyBytes == 336U &&
           snapshot->runtimeFNV1a == EXPECTED_TARGET_RUNTIME_FNV &&
           snapshot->mapStateFNV1a == EXPECTED_TARGET_MAP_FNV &&
           snapshot->scriptStateFNV1a == EXPECTED_TARGET_SCRIPT_FNV &&
           snapshot->lineStateFNV1a == EXPECTED_TARGET_LINE_FNV &&
           snapshot->textureStateFNV1a == EXPECTED_TARGET_TEXTURE_FNV &&
           snapshot->automapStateFNV1a == EXPECTED_TARGET_AUTOMAP_FNV &&
           snapshot->topologyFNV1a == EXPECTED_TARGET_TOPOLOGY_FNV &&
           snapshot->nodeCount == 77U && snapshot->lineCount == 207U &&
           snapshot->spriteCount == 48U && snapshot->eventCount == 66U &&
           snapshot->byteCodeCount == 319U && snapshot->stringCount == 126U &&
           snapshot->entityCount == 30U && snapshot->enemyCount == 0U &&
           snapshot->destructibleCount == 3U &&
           hashBytes(snapshot, sizeof(*snapshot)) == EXPECTED_TARGET_SNAPSHOT_FNV;
}

void Esp32CommittedTransitionProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32CommittedTransitionProbe_isDone(void) {
    return probeState.done;
}

void Esp32CommittedTransitionProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot source;
    EspMapResidentSnapshot sourceAfterGate;
    EspMapResidentSnapshot recovered;
    EspMapResidentSnapshot target;
    EspMapResidentSnapshot secondTarget;
    EspMapResidentSnapshot gateOutput;
    EspMapChangeMapState pending;
    EspMapChangeMapState pendingBefore;
    EspMapChangeMapState pendingForBadBegin;
    EspMapChangeMapResult change;
    EspStatsMenuIntent statsIntent;
    EspMapTransitionPreflightResult preflight;
    EspMapTransitionPreflightResult badPreflight;
    EspBspInventory sourceInventory;
    EspBspInventory targetInventory;
    EspBspInventory badInventory;
    EspBspInventory failInventory;
    EspMapCommittedTransitionState transition;
    EspMapCommittedTransitionState badBeginState;
    EspMapCommittedTransitionState rollbackState;
    EspMapCommittedTransitionState stateBeforeGate;
    EspMapCommittedTransitionStatus status;
    uint32_t sourceHeap;
    uint32_t sourceLargest;
    uint32_t targetHeap;
    uint32_t targetLargest;
    uint32_t rollbackHeap;
    uint32_t rollbackLargest;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t transitionBefore;
    uint32_t transitionAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t preflightFNV;
    uint32_t waitStateFNV;
    uint32_t readyStateFNV;
    uint32_t rollbackStateFNV;
    uint32_t committedStateFNV;
    int invalidBegin;
    int preAckGate;
    int badInventoryGate;
    int forcedRollback;
    int repeatAck;
    int repeatCommitRefused;
    int sourcePreserved;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32ResidentHandoffProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[COMMITTRANSITIONPROBE] ARMED reversible handoff proven; committed Junction transition starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native committed Junction transition ===\n");
    printf("[COMMITTRANSITIONPROBE] CONTRACT consume the real Entrance CHANGEMAP pending state, wait for explicit stats acknowledgement, validate source/target inventories before teardown, prove forced post-teardown rollback, then commit Junction and leave its full native resident owner set active; no DoomCanvas_loadMap, no legacy Game/Menu/Render/Player mutation, no spawn/loadType ownership and no ST_PLAYING\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->render == NULL ||
        doomRpg->game == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->player == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspMapCommittedTransitionState) != EXPECTED_STATE_BYTES ||
        !EspMapResidentLifecycle_capture(&source) ||
        !sourceSnapshotCanonical(&source)) {
        printf("[COMMITTRANSITIONPROBE] FAILED unsafe Entrance boundary\n");
        probeState.done = 1;
        return;
    }

    sourceHeap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    sourceLargest =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    transitionBefore = transitionWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);

    if (!buildRealChange(&pending, &change) ||
        EspStatsMenuIntent_prepare(TARGET_MAP_ID, change.showStats, &statsIntent) !=
            ESP_STATS_MENU_INTENT_OK ||
        hashBytes(&statsIntent, sizeof(statsIntent)) != EXPECTED_STATS_INTENT_FNV ||
        EspMapTransitionPreflight_run(TARGET_MAP_ID, &preflight) !=
            ESP_MAP_TRANSITION_PREFLIGHT_OK ||
        preflight.targetMapId != TARGET_MAP_ID || preflight.gameplayLoadMapId != 2U ||
        preflight.ready != 1U ||
        (preflightFNV = hashBytes(&preflight, sizeof(preflight))) !=
            EXPECTED_PREFLIGHT_FNV ||
        !EspBspReader_inventoryPackEntry(SOURCE_RESOURCE, &sourceInventory) ||
        !EspBspReader_inventoryPackEntry(TARGET_RESOURCE, &targetInventory) ||
        EspAssetPack_isOpen()) {
        printf("[COMMITTRANSITIONPROBE] FAILED source intent/preflight inventories\n");
        probeState.done = 1;
        return;
    }

    memset(&transition, 0, sizeof(transition));
    memset(&badBeginState, 0, sizeof(badBeginState));
    badPreflight = preflight;
    badPreflight.targetMapId = ESP_MAP_ID_END_GAME;
    pendingForBadBegin = pending;
    pendingBefore = pendingForBadBegin;
    invalidBegin = EspMapCommittedTransition_begin(
                       &badBeginState, SOURCE_MAP_ID, &pendingForBadBegin, &change,
                       &statsIntent, &badPreflight) ==
                       ESP_MAP_COMMITTED_TRANSITION_INVALID &&
                   memcmp(&pendingForBadBegin, &pendingBefore,
                          sizeof(pendingBefore)) == 0 &&
                   hashBytes(&badBeginState, sizeof(badBeginState)) ==
                       hashBytes(&(EspMapCommittedTransitionState){0},
                                 sizeof(badBeginState));
    if (!invalidBegin) {
        printf("[COMMITTRANSITIONPROBE] FAILED invalid begin atomicity\n");
        probeState.done = 1;
        return;
    }

    status = EspMapCommittedTransition_begin(
        &transition, SOURCE_MAP_ID, &pending, &change, &statsIntent, &preflight);
    waitStateFNV = hashBytes(&transition, sizeof(transition));
    if (status != ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS ||
        EspMapChangeMap_isActive(&pending) ||
        waitStateFNV != EXPECTED_WAIT_STATE_FNV || transition.pendingConsumed != 1U ||
        transition.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS ||
        transition.statsAcknowledged != 0U || transition.committed != 0U ||
        transition.sourceMapId != SOURCE_MAP_ID ||
        transition.targetMapId != TARGET_MAP_ID ||
        transition.targetGameplayLoadMapId != 2U ||
        transition.spawnParam != 0U || transition.menuKind != ESP_STATS_MENU_KIND_LEVEL) {
        printf("[COMMITTRANSITIONPROBE] FAILED begin/wait state status=%u stateFNV=%08x\n",
               (unsigned int)status, (unsigned int)waitStateFNV);
        probeState.done = 1;
        return;
    }

    stateBeforeGate = transition;
    memset(&gateOutput, 0xa5, sizeof(gateOutput));
    status = EspMapCommittedTransition_commit(
        &transition, &sourceInventory, &targetInventory, &gateOutput);
    preAckGate = status == ESP_MAP_COMMITTED_TRANSITION_INVALID &&
                 memcmp(&transition, &stateBeforeGate, sizeof(transition)) == 0 &&
                 snapshotIsZero(&gateOutput) &&
                 EspMapResidentLifecycle_capture(&sourceAfterGate) &&
                 memcmp(&sourceAfterGate, &source, sizeof(source)) == 0;
    if (!preAckGate) {
        printf("[COMMITTRANSITIONPROBE] FAILED pre-ack commit gate status=%u\n",
               (unsigned int)status);
        probeState.done = 1;
        return;
    }

    status = EspMapCommittedTransition_ackStats(&transition);
    readyStateFNV = hashBytes(&transition, sizeof(transition));
    if (status != ESP_MAP_COMMITTED_TRANSITION_READY ||
        readyStateFNV != EXPECTED_READY_STATE_FNV ||
        transition.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_READY ||
        transition.statsAcknowledged != 1U) {
        printf("[COMMITTRANSITIONPROBE] FAILED stats ack status=%u stateFNV=%08x\n",
               (unsigned int)status, (unsigned int)readyStateFNV);
        probeState.done = 1;
        return;
    }
    stateBeforeGate = transition;
    repeatAck = EspMapCommittedTransition_ackStats(&transition) ==
                    ESP_MAP_COMMITTED_TRANSITION_READY &&
                memcmp(&transition, &stateBeforeGate, sizeof(transition)) == 0;

    badInventory = targetInventory;
    badInventory.crc32 ^= 1U;
    memset(&gateOutput, 0xa5, sizeof(gateOutput));
    stateBeforeGate = transition;
    status = EspMapCommittedTransition_commit(
        &transition, &sourceInventory, &badInventory, &gateOutput);
    badInventoryGate = status == ESP_MAP_COMMITTED_TRANSITION_INVENTORY_MISMATCH &&
                       memcmp(&transition, &stateBeforeGate,
                              sizeof(transition)) == 0 &&
                       snapshotIsZero(&gateOutput) &&
                       EspMapResidentLifecycle_capture(&sourceAfterGate) &&
                       memcmp(&sourceAfterGate, &source, sizeof(source)) == 0;
    if (!repeatAck || !badInventoryGate) {
        printf("[COMMITTRANSITIONPROBE] FAILED ready fail-closed gates repeatAck=%d badInventory=%d status=%u\n",
               repeatAck, badInventoryGate, (unsigned int)status);
        probeState.done = 1;
        return;
    }

    rollbackState = transition;
    failInventory = targetInventory;
    ++failInventory.plan.resourceSetsBytes;
    memset(&gateOutput, 0xa5, sizeof(gateOutput));
    status = EspMapCommittedTransition_commit(
        &rollbackState, &sourceInventory, &failInventory, &gateOutput);
    rollbackHeap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    rollbackLargest =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    rollbackStateFNV = hashBytes(&rollbackState, sizeof(rollbackState));
    forcedRollback = status == ESP_MAP_COMMITTED_TRANSITION_ROLLED_BACK &&
                     rollbackState.phase ==
                         ESP_MAP_COMMITTED_TRANSITION_PHASE_ROLLED_BACK &&
                     rollbackState.committed == 0U &&
                     rollbackStateFNV == EXPECTED_ROLLED_BACK_STATE_FNV &&
                     snapshotIsZero(&gateOutput) &&
                     EspMapResidentLifecycle_capture(&recovered) &&
                     sourceSnapshotCanonical(&recovered) &&
                     memcmp(&recovered, &source, sizeof(source)) == 0 &&
                     rollbackHeap == sourceHeap &&
                     rollbackLargest == sourceLargest && !EspAssetPack_isOpen();
    if (!forcedRollback) {
        printf("[COMMITTRANSITIONPROBE] FAILED forced rollback status=%u phase=%u stateFNV=%08x heap=%u/%u largest=%u/%u\n",
               (unsigned int)status, (unsigned int)rollbackState.phase,
               (unsigned int)rollbackStateFNV, (unsigned int)rollbackHeap,
               (unsigned int)sourceHeap, (unsigned int)rollbackLargest,
               (unsigned int)sourceLargest);
        probeState.done = 1;
        return;
    }

    status = EspMapCommittedTransition_commit(
        &transition, &sourceInventory, &targetInventory, &target);
    targetHeap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    targetLargest =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    committedStateFNV = hashBytes(&transition, sizeof(transition));
    if (status != ESP_MAP_COMMITTED_TRANSITION_OK ||
        !EspMapCommittedTransition_isCommitted(&transition) ||
        committedStateFNV != EXPECTED_COMMITTED_STATE_FNV ||
        !targetSnapshotCanonical(&target) ||
        !EspMapResidentLifecycle_capture(&secondTarget) ||
        memcmp(&secondTarget, &target, sizeof(target)) != 0 ||
        targetHeap != sourceHeap + EXPECTED_HEAP_GAIN ||
        targetLargest != sourceLargest || EspAssetPack_isOpen()) {
        printf("[COMMITTRANSITIONPROBE] FAILED final commit status=%u stateFNV=%08x targetFNV=%08x heap=%u expected=%u largest=%u/%u\n",
               (unsigned int)status, (unsigned int)committedStateFNV,
               (unsigned int)hashBytes(&target, sizeof(target)),
               (unsigned int)targetHeap,
               (unsigned int)(sourceHeap + EXPECTED_HEAP_GAIN),
               (unsigned int)targetLargest, (unsigned int)sourceLargest);
        probeState.done = 1;
        return;
    }

    stateBeforeGate = transition;
    memset(&gateOutput, 0xa5, sizeof(gateOutput));
    repeatCommitRefused =
        EspMapCommittedTransition_commit(&transition, &sourceInventory,
                                         &targetInventory, &gateOutput) ==
            ESP_MAP_COMMITTED_TRANSITION_INVALID &&
        memcmp(&transition, &stateBeforeGate, sizeof(transition)) == 0 &&
        snapshotIsZero(&gateOutput) &&
        EspMapResidentLifecycle_capture(&secondTarget) &&
        memcmp(&secondTarget, &target, sizeof(target)) == 0;

    frameAfter = framebufferHash();
    transitionAfter = transitionWitness(doomRpg);
    playerAfter = playerWitness(doomRpg->player);
    sourcePreserved = frameAfter == frameBefore &&
                      transitionAfter == transitionBefore &&
                      playerAfter == playerBefore &&
                      legacyRuntimeIsClear(doomRpg->render) &&
                      doomRpg->game->numEntities == 0 &&
                      doomRpg->game->numMonsters == 0 &&
                      doomRpg->doomCanvas->state == ST_INTRO &&
                      doomRpg->doomCanvas->storyPage == 3 &&
                      repeatCommitRefused && !EspAssetPack_isOpen();
    if (!sourcePreserved) {
        printf("[COMMITTRANSITIONPROBE] FAILED final legacy/frame boundary repeatCommit=%d\n",
               repeatCommitRefused);
        probeState.done = 1;
        return;
    }

    printf("[COMMITTRANSITION] BEGIN stateBytes=%u sourceMap=%u targetMap=%u gameplayLoadMapId=%u spawnParam=%u menuKind=%u pendingConsumed=1 phase=WAIT_STATS waitStateFNV=%08x preflightFNV=%08x statsIntentFNV=%08x\n",
           (unsigned int)sizeof(transition), (unsigned int)transition.sourceMapId,
           (unsigned int)transition.targetMapId,
           (unsigned int)transition.targetGameplayLoadMapId,
           (unsigned int)transition.spawnParam, (unsigned int)transition.menuKind,
           (unsigned int)waitStateFNV, (unsigned int)preflightFNV,
           (unsigned int)hashBytes(&statsIntent, sizeof(statsIntent)));
    printf("[COMMITTRANSITION] ACK statsAcknowledged=1 phase=READY readyStateFNV=%08x repeatAck=%d\n",
           (unsigned int)readyStateFNV, repeatAck);
    printf("[COMMITTRANSITION] GATES invalidBegin=%d preAckCommit=%d badInventory=%d repeatCommit=%d stateAtomic=yes sourcePreservedBeforeCommit=yes\n",
           invalidBegin, preAckGate, badInventoryGate, repeatCommitRefused);
    printf("[COMMITTRANSITION] ROLLBACK forced=1 phase=ROLLED_BACK stateFNV=%08x sourceRestored=yes snapshotFNV=%08x heap8=%u->%u largest8=%u->%u packClosed=yes\n",
           (unsigned int)rollbackStateFNV,
           (unsigned int)hashBytes(&recovered, sizeof(recovered)),
           (unsigned int)sourceHeap, (unsigned int)rollbackHeap,
           (unsigned int)sourceLargest, (unsigned int)rollbackLargest);
    printf("[COMMITTRANSITION] COMMIT status=%u phase=COMMITTED committed=1 committedStateFNV=%08x targetSnapshotFNV=%08x payload=%u targetHeapGain=%u sourceHeap=%u targetHeap=%u largest=%u->%u packClosed=yes\n",
           (unsigned int)status, (unsigned int)committedStateFNV,
           (unsigned int)hashBytes(&target, sizeof(target)),
           (unsigned int)target.totalPayloadBytes, (unsigned int)EXPECTED_HEAP_GAIN,
           (unsigned int)sourceHeap, (unsigned int)targetHeap,
           (unsigned int)sourceLargest, (unsigned int)targetLargest);
    printf("[COMMITTRANSITION] TARGETFNV arena=%08x map=%08x script=%08x line=%08x texture=%08x automap=%08x topology=%08x entities=%u enemies=%u destructibles=%u\n",
           (unsigned int)target.runtimeFNV1a,
           (unsigned int)target.mapStateFNV1a,
           (unsigned int)target.scriptStateFNV1a,
           (unsigned int)target.lineStateFNV1a,
           (unsigned int)target.textureStateFNV1a,
           (unsigned int)target.automapStateFNV1a,
           (unsigned int)target.topologyFNV1a,
           (unsigned int)target.entityCount, (unsigned int)target.enemyCount,
           (unsigned int)target.destructibleCount);
    printf("[COMMITTRANSITION] LEGACY playerFNV=%08x->%08x transitionFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes DoomCanvas_loadMapCalled=no menuMutation=no legacyPlayerMutation=no spawnApplied=no loadTypeMutation=no\n",
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter);
    printf("[COMMITTRANSITION] PARK state=%d page=%d committedTransition=yes mapSwapCommitted=yes sourceMap=1 targetMap=9 junctionResident=yes sourceRestored=no targetLeftResident=yes nativePayload=%u persistentHeapProven=%u pendingConsumed=yes statsAck=yes spawnPending=yes spawnApplied=no ST_PLAYING=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)target.totalPayloadBytes,
           (unsigned int)EXPECTED_TARGET_HEAP_COST,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);

    probeState.done = 1;
}
