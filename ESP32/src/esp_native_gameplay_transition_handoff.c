#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_catalog.h"
#include "esp_map_committed_transition.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_gameplay_status_message.h"
#include "esp_native_gameplay_facing_label.h"
#include "esp_native_gameplay_transition.h"
#include "esp_native_gameplay_transition_handoff.h"
#include "esp_native_transition_presentation.h"
#include "esp_player_facing_state.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"
#include "platform_touch_events.h"

#define HANDOFF_LOAD_TYPE ESP_PLAYER_SPAWN_LOAD_FRESH_MAP
#define HANDOFF_GAME_IS_LOADED 0U
#define HANDOFF_FAMILIAR_AMMO_TYPE 5U

/*
 * This is bounded transition scratch, not retained map content. In particular,
 * the two inventories are reconstructed from the PAK only for the destructive
 * compare/commit boundary and are reused for every map change. Keeping them
 * static avoids paying their size on loopTask while a touch callback is active.
 */
typedef struct EspNativeGameplayTransitionHandoffScratch_s {
    EspBspInventory sourceInventory;
    EspBspInventory targetInventory;
    EspMapResidentSnapshot targetSnapshot;
    EspMapCommittedTransitionState committed;
    EspPlayerSpawnState spawn;
    EspNativeGameplaySessionConfig sessionConfig;
    uint32_t playerKeys;
    uint16_t disabledWeapons;
    uint8_t armed;
    uint8_t busy;
    uint8_t failed;
    uint8_t reserved;
} EspNativeGameplayTransitionHandoffScratch;

static EspNativeGameplayTransitionHandoffScratch handoff;

static uint32_t nowMs(void) {
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static void resetSpawnOwners(void) {
    EspPlayerView_reset();
    EspHudRefresh_reset();
    EspPlayerFreshMap_reset();
    EspPlayerInitialTile_reset();
    EspPlayerOrientation_reset();
    EspPlayerFinishRotationTile_reset();
    EspPlayerFacing_reset();
    EspHudPostLoadClear_reset();
    EspNativeGameplayDispatch_reset();
}

static void failHandoff(const char* stage, unsigned int status) {
    EspAssetPack_mapFlashSetProgressCallback(NULL);
    EspNativeTransitionPresentation_endLoading();
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    handoff.failed = 1U;
    handoff.armed = 0U;
    handoff.busy = 0U;
    printf("[NATIVECHANGEMAP] HANDOFF-FAILED stage=%s status=%u resident=%u packOpen=%u failClosed=yes\n",
           stage != NULL ? stage : "unknown",
           status,
           (unsigned int)EspMapResidentLifecycle_isReady(),
           (unsigned int)EspAssetPack_isOpen());
}

static int inventoryForMap(uint8_t mapId, EspBspInventory* outInventory) {
    const char* name;
    if (outInventory == NULL || !EspMapCatalog_isValidId(mapId)) return 0;
    name = EspMapCatalog_nameForId(mapId);
    if (name == NULL || name[0] == '\0') return 0;
    memset(outInventory, 0, sizeof(*outInventory));
    if (!EspAssetPack_sourceProbeBegin(name)) return 0;
    const int inventoryOk =
        EspBspReader_inventoryPackEntry(name, outInventory);
    EspAssetPack_sourceProbeEnd();
    return inventoryOk &&
           !EspAssetPack_isOpen() &&
           !EspAssetPack_isSourceProbeActive() &&
           outInventory->sourceBytes != 0U &&
           outInventory->consumedBytes == outInventory->sourceBytes &&
           outInventory->trailingBytes == 0U &&
           outInventory->crc32 != 0U &&
           outInventory->crc32 == outInventory->expectedCrc32 &&
           outInventory->plan.persistentBytes != 0U;
}

static int captureSessionConfig(void) {
    const EspNativeGameplayPlayerState* player =
        EspNativeGameplayPlayerState_view();
    const EspNativeGameplayWeaponSpec* weaponSpec = NULL;
    uint8_t ammoType;

    if (player == NULL || player->active != 1U ||
        player->weapon >= ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT) {
        return 0;
    }

    if (player->weapon < ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS) {
        weaponSpec = EspNativeGameplayCombatMath_weapon(player->weapon);
        if (weaponSpec == NULL ||
            weaponSpec->ammoType >= ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES) {
            return 0;
        }
        ammoType = weaponSpec->ammoType;
    }
    else {
        ammoType = HANDOFF_FAMILIAR_AMMO_TYPE;
    }

    memset(&handoff.sessionConfig, 0, sizeof(handoff.sessionConfig));
    handoff.sessionConfig.health = EspNativeGameplayPlayerState_health();
    handoff.sessionConfig.maxHealth = EspNativeGameplayPlayerState_maxHealth();
    handoff.sessionConfig.armor = EspNativeGameplayPlayerState_armor();
    handoff.sessionConfig.maxArmor = EspNativeGameplayPlayerState_maxArmor();
    handoff.sessionConfig.ammo = EspNativeGameplayPlayerState_ammo(ammoType);
    handoff.sessionConfig.weapon = player->weapon;
    handoff.sessionConfig.ammoType = ammoType;
    handoff.sessionConfig.weaponsPresent = player->weapons != 0U ? 1U : 0U;
    handoff.playerKeys = player->keys;
    handoff.disabledWeapons = player->disabledWeapons;

    return handoff.sessionConfig.maxHealth != 0U &&
           handoff.sessionConfig.health <= handoff.sessionConfig.maxHealth &&
           handoff.sessionConfig.armor <= handoff.sessionConfig.maxArmor;
}

static int routeCommittedSpawn(void) {
    EspPlayerSpawnStatus spawnStatus;
    EspPlayerViewApplyStatus viewStatus;
    EspHudRefreshStatus hudStatus;
    EspPlayerFreshMapStatus freshStatus;
    EspPlayerInitialTileStatus firstTileStatus;
    EspPlayerOrientationStatus orientationStatus;
    EspPlayerFinishRotationTileStatus secondTileStatus;
    EspPlayerFacingStatus facingStatus;
    EspHudPostLoadClearStatus clearStatus;
    const EspPlayerViewState* view;
    uint8_t deferredCode = 0U;
    uint8_t deferredOffset = 0U;

    memset(&handoff.spawn, 0, sizeof(handoff.spawn));
    spawnStatus = EspPlayerSpawn_prepareCommitted(
        &handoff.committed,
        &handoff.targetInventory,
        HANDOFF_LOAD_TYPE,
        HANDOFF_GAME_IS_LOADED,
        &handoff.spawn);
    if (spawnStatus != ESP_PLAYER_SPAWN_OK) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=PREPARE status=%u\n",
               (unsigned int)spawnStatus);
        return 0;
    }

    viewStatus = EspPlayerView_applySpawn(&handoff.spawn);
    hudStatus = viewStatus == ESP_PLAYER_VIEW_APPLY_OK
                    ? EspHudRefresh_routePostSpawn()
                    : ESP_HUD_REFRESH_VIEW_INVALID;
    freshStatus = hudStatus == ESP_HUD_REFRESH_OK
                      ? EspPlayerFreshMap_route(nowMs(), handoff.disabledWeapons)
                      : ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    if (viewStatus != ESP_PLAYER_VIEW_APPLY_OK ||
        hudStatus != ESP_HUD_REFRESH_OK ||
        freshStatus != ESP_PLAYER_FRESH_MAP_OK) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=POST_SPAWN view=%u hud=%u fresh=%u disabledWeapons=%u\n",
               (unsigned int)viewStatus,
               (unsigned int)hudStatus,
               (unsigned int)freshStatus,
               (unsigned int)handoff.disabledWeapons);
        return 0;
    }

    firstTileStatus = EspPlayerInitialTile_route(
        handoff.playerKeys, 0U, &deferredCode, &deferredOffset);
    if (firstTileStatus != ESP_PLAYER_INITIAL_TILE_OK) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=FIRST_TILE status=%u opcode=%u commandOffset=%u keys=%08x\n",
               (unsigned int)firstTileStatus,
               (unsigned int)deferredCode,
               (unsigned int)deferredOffset,
               (unsigned int)handoff.playerKeys);
        return 0;
    }

    orientationStatus = EspPlayerOrientation_route();
    if (orientationStatus != ESP_PLAYER_ORIENTATION_OK) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=ORIENTATION status=%u\n",
               (unsigned int)orientationStatus);
        return 0;
    }

    deferredCode = 0U;
    deferredOffset = 0U;
    secondTileStatus = EspPlayerFinishRotationTile_route(
        handoff.playerKeys, 0U, &deferredCode, &deferredOffset);
    if (secondTileStatus != ESP_PLAYER_FINISH_ROTATION_TILE_OK) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=SECOND_TILE status=%u opcode=%u commandOffset=%u keys=%08x\n",
               (unsigned int)secondTileStatus,
               (unsigned int)deferredCode,
               (unsigned int)deferredOffset,
               (unsigned int)handoff.playerKeys);
        return 0;
    }

    facingStatus = EspPlayerFacing_route();
    clearStatus = facingStatus == ESP_PLAYER_FACING_OK
                      ? EspHudPostLoadClear_route()
                      : ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;
    if (facingStatus != ESP_PLAYER_FACING_OK ||
        clearStatus != ESP_HUD_POST_LOAD_CLEAR_OK ||
        !EspNativeGameplayDispatch_adoptView()) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=FINAL facing=%u clear=%u dispatchReady=%u\n",
               (unsigned int)facingStatus,
               (unsigned int)clearStatus,
               (unsigned int)EspNativeGameplayDispatch_isReady());
        return 0;
    }

    view = EspPlayerView_view();
    if (view == NULL || view->active != 1U ||
        view->targetMapId != handoff.committed.targetMapId ||
        view->gameplayLoadMapId != handoff.committed.targetGameplayLoadMapId ||
        view->hudRefreshPending != 0U ||
        view->facingRefreshPending != 0U ||
        view->playerSetupPending != 0U ||
        view->tileEnterPending != 0U ||
        view->viewAngle != view->destAngle ||
        EspAssetPack_isOpen()) {
        printf("[NATIVECHANGEMAP] SPAWN-BLOCKED stage=SETTLE target=%u view=%u packOpen=%u\n",
               (unsigned int)handoff.committed.targetMapId,
               view != NULL ? (unsigned int)view->targetMapId : 0U,
               (unsigned int)EspAssetPack_isOpen());
        return 0;
    }

    printf("[NATIVECHANGEMAP] SPAWN targetMap=%u gameplayLoadMapId=%u tile=%u pos=%u,%u angle=%u source=%u override=%u settled=yes\n",
           (unsigned int)view->targetMapId,
           (unsigned int)view->gameplayLoadMapId,
           (unsigned int)handoff.spawn.tileIndex,
           (unsigned int)handoff.spawn.worldX,
           (unsigned int)handoff.spawn.worldY,
           (unsigned int)handoff.spawn.angle,
           (unsigned int)handoff.spawn.spawnSource,
           (unsigned int)handoff.spawn.overrideUsed);
    return 1;
}

static void onStatsAcknowledged(int16_t screenX,
                                int16_t screenY,
                                uint16_t pressure,
                                uint16_t rawX,
                                uint16_t rawY) {
    const EspNativeGameplayTransitionState* transition;
    EspMapCommittedTransitionStatus committedStatus;
    uint8_t sourceMapId;
    uint8_t targetMapId;

    (void)screenX;
    (void)screenY;
    (void)pressure;
    (void)rawX;
    (void)rawY;

    if (!handoff.armed || handoff.busy || handoff.failed) return;
    handoff.busy = 1U;
    PlatformInput_setTapCallback(NULL);

    transition = EspNativeGameplayTransition_view();
    if (transition == NULL || transition->active != 1U ||
        transition->waitingStats != 1U ||
        transition->committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS ||
        transition->committed.pendingConsumed != 1U ||
        transition->committed.statsAcknowledged != 0U ||
        transition->committed.committed != 0U ||
        !EspMapResidentLifecycle_isReady() || EspAssetPack_isOpen()) {
        failHandoff("WAIT_STATS_OWNER", 0U);
        return;
    }

    handoff.committed = transition->committed;
    sourceMapId = handoff.committed.sourceMapId;
    targetMapId = handoff.committed.targetMapId;

    if (!inventoryForMap(sourceMapId, &handoff.sourceInventory)) {
        failHandoff("SOURCE_INVENTORY", sourceMapId);
        return;
    }
    if (!inventoryForMap(targetMapId, &handoff.targetInventory)) {
        failHandoff("TARGET_INVENTORY", targetMapId);
        return;
    }
    if (!captureSessionConfig()) {
        failHandoff("PLAYER_SESSION", 0U);
        return;
    }

    committedStatus = EspMapCommittedTransition_ackStats(&handoff.committed);
    if (committedStatus != ESP_MAP_COMMITTED_TRANSITION_READY ||
        handoff.committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_READY ||
        handoff.committed.statsAcknowledged != 1U) {
        failHandoff("STATS_ACK", (unsigned int)committedStatus);
        return;
    }

    printf("[NATIVECHANGEMAP] STATS-ACK sourceMap=%u targetMap=%u phase=%u presentation=native-stats input=one-tap\n",
           (unsigned int)sourceMapId,
           (unsigned int)targetMapId,
           (unsigned int)handoff.committed.phase);

    /*
     * Cross the visual ownership boundary before the destructive backing
     * switch. The starfield descriptor is captured while source backing is
     * still valid; map-flash staging then pumps it from the authoritative SD
     * PAK between complete erase/copy/verify chunks.
     */
    if (!EspNativeTransitionPresentation_beginLoading(targetMapId)) {
        failHandoff("LOADING_PRESENTATION", targetMapId);
        return;
    }
    EspAssetPack_mapFlashSetProgressCallback(
        EspNativeTransitionPresentation_progress);

    memset(&handoff.targetSnapshot, 0, sizeof(handoff.targetSnapshot));
    committedStatus = EspMapCommittedTransition_commit(
        &handoff.committed,
        &handoff.sourceInventory,
        &handoff.targetInventory,
        &handoff.targetSnapshot);
    EspAssetPack_mapFlashSetProgressCallback(NULL);
    if (committedStatus != ESP_MAP_COMMITTED_TRANSITION_OK ||
        handoff.committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED ||
        handoff.committed.committed != 1U ||
        !EspMapResidentLifecycle_isReady() || EspAssetPack_isOpen()) {
        failHandoff("RESIDENT_COMMIT", (unsigned int)committedStatus);
        return;
    }
    EspNativeTransitionPresentation_endLoading();

    printf("[NATIVECHANGEMAP] COMMIT sourceMap=%u targetMap=%u gameplayLoadMapId=%u phase=%u arena=%u payload=%u nodes=%u lines=%u sprites=%u events=%u rollback=no\n",
           (unsigned int)sourceMapId,
           (unsigned int)targetMapId,
           (unsigned int)handoff.committed.targetGameplayLoadMapId,
           (unsigned int)handoff.committed.phase,
           (unsigned int)handoff.targetSnapshot.runtimeArenaBytes,
           (unsigned int)handoff.targetSnapshot.totalPayloadBytes,
           (unsigned int)handoff.targetSnapshot.nodeCount,
           (unsigned int)handoff.targetSnapshot.lineCount,
           (unsigned int)handoff.targetSnapshot.spriteCount,
           (unsigned int)handoff.targetSnapshot.eventCount);

    /* Tear down only transient source-map gameplay/session owners. The shared
     * compact player root survives this reset and supplied sessionConfig above.
     * Input reset also clears the old transition owner; the committed copy in
     * this static scratch remains the authoritative handoff witness below. */
    EspNativeGameplaySession_reset();
    resetSpawnOwners();

    if (!routeCommittedSpawn()) {
        failHandoff("TARGET_SPAWN", 0U);
        return;
    }
    if (!EspNativeGameplaySession_configure(&handoff.sessionConfig)) {
        failHandoff("TARGET_SESSION", 0U);
        return;
    }

    handoff.armed = 0U;
    handoff.busy = 0U;
    handoff.failed = 0U;
    printf("[NATIVECHANGEMAP] SESSION targetMap=%u configured=yes weapon=%u ammoType=%u ammo=%u hp=%u/%u armor=%u/%u next=generic-session-service\n",
           (unsigned int)targetMapId,
           (unsigned int)handoff.sessionConfig.weapon,
           (unsigned int)handoff.sessionConfig.ammoType,
           (unsigned int)handoff.sessionConfig.ammo,
           (unsigned int)handoff.sessionConfig.health,
           (unsigned int)handoff.sessionConfig.maxHealth,
           (unsigned int)handoff.sessionConfig.armor,
           (unsigned int)handoff.sessionConfig.maxArmor);
}

int EspNativeGameplayTransitionHandoff_tryArmNullCallback(void) {
    const EspNativeGameplayTransitionState* transition;

    /* During the destructive callback, session reset legitimately requests a
     * NULL callback. Never reinterpret that teardown as another stats arm. */
    if (handoff.busy) return 0;

    transition = EspNativeGameplayTransition_view();
    if (transition == NULL || transition->active != 1U ||
        transition->waitingStats != 1U ||
        transition->committed.phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS ||
        transition->committed.pendingConsumed != 1U ||
        transition->committed.statsAcknowledged != 0U ||
        transition->committed.committed != 0U) {
        handoff.armed = 0U;
        handoff.failed = 0U;
        EspAssetPack_mapFlashSetProgressCallback(NULL);
        /*
         * A MENU_MAIN checkpoint restore legitimately resets the resident
         * gameplay/input session after beginLoading(). That reset requests a
         * NULL tap callback too, but it is not a WAIT_STATS ownership change.
         * Preserve an already-active checkpoint loading frame; otherwise this
         * generic no-transition cleanup silently drops its presentation owner
         * before cache/session priming begins.
         *
         * Stats presentation never sets loadingActive, so the historical
         * stale-WAIT_STATS cleanup still resets that path exactly as before.
         */
        if (EspNativeTransitionPresentation_isLoadingActive()) {
            printf("[NATIVECHANGEMAP] NULL-CALLBACK no-wait-stats checkpointLoading=preserved\n");
        }
        else {
            EspNativeTransitionPresentation_reset();
        }
        return 0;
    }

    /* A failed exact WAIT_STATS handoff stays inert until ownership leaves the
     * state. This prevents the platform NULL callback path from silently
     * re-arming the same failed transition. */
    if (handoff.failed) return 0;

    /* WAIT_STATS owns the full framebuffer. Clear source-map top-bar fallback
     * owners before the opaque stats frame so neither a stale FORCE_MESSAGE nor
     * a facing label can repaint over the transition UI. */
    EspNativeGameplayStatusMessage_reset();
    EspNativeGameplayFacingLabel_reset();

    if (!EspNativeTransitionPresentation_showStats(transition)) {
        printf("[NATIVECHANGEMAP] STATS-PRESENTATION status=FAILED sourceMap=%u targetMap=%u failClosed=yes\n",
               (unsigned int)transition->committed.sourceMapId,
               (unsigned int)transition->committed.targetMapId);
        handoff.failed = 1U;
        return 1;
    }

    handoff.armed = 1U;
    PlatformInput_setTapCallback(onStatsAcknowledged);
    printf("[NATIVECHANGEMAP] STATS-PRESENTATION phase=WAIT_STATS callback=one-tap sourceMap=%u targetMap=%u sourceResident=%u statsPresentation=native-level-complete\n",
           (unsigned int)transition->committed.sourceMapId,
           (unsigned int)transition->committed.targetMapId,
           (unsigned int)EspMapResidentLifecycle_isReady());
    return 1;
}
