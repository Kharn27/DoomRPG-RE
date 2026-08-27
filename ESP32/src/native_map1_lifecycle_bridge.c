#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_native_first_frame.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_resident_gameplay.h"
#include "esp_player_view_state.h"
#include "esp_probe_log.h"
#include "native_committed_transition_probe.h"
#include "native_entrance_spawn_chain_probe.h"
#include "native_entrance_startup_route_probe.h"
#include "native_intro_dispose.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_change_map_probe.h"
#include "native_map1_dialog_owner_probe.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
#include "native_map1_events_probe.h"
#include "native_map1_givemap_probe.h"
#include "native_map1_key_gate_probe.h"
#include "native_map1_level_exit_stats_probe.h"
#include "native_map1_line_door_probe.h"
#include "native_map1_notebook_probe.h"
#include "native_map1_opcode_exec_probe.h"
#include "native_map1_password_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_save_route_probe.h"
#include "native_map1_show_hide_final_probe.h"
#include "native_map1_state_probe.h"
#include "native_map1_status_message_probe.h"
#include "native_map1_string_reader_probe.h"
#include "native_map1_ui_intent_probe.h"
#include "native_map1_unlock_probe.h"
#include "native_player_exit_state_probe.h"
#include "native_resident_handoff_probe.h"
#include "native_stats_menu_intent_probe.h"
#include "native_transition_preflight_final_probe.h"
#include "platform_video_c_bridge.h"

#define VALIDATED_FAST_FORWARD_MAX_PASSES 64U

static unsigned int fastForwardTotalPasses;
static int fastForwardReadyLogged;
static int fastForwardWaitLogged;
static int fastForwardBlockedLogged;
static int entranceCatalogLogged;
static int entranceFirstFrameAttempted;
static int entranceHudAttempted;
static int entranceHudReady;
static int entranceCompositeAttempted;
static int entranceCompositeReady;

void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

/*
 * Production startup boundary after PR #100.
 *
 * The historical validation ladder originally continued past the read-only
 * target preflight into ResidentHandoff -> CommittedTransition -> Junction
 * spawn/render/gameplay. That was useful while proving each migration layer,
 * but it accidentally turned the real Entrance level-exit script into a boot
 * sequence and skipped the first playable map.
 *
 * Keep all source-map semantic regression probes through transition preflight;
 * stop BEFORE any resident teardown. /junction.bsp may be streamed read-only by
 * the preflight, but /intro.bsp (Entrance) remains the resident map. Initial
 * startup then proceeds through the dedicated Entrance spawn chain and directly
 * presents one native Entrance world frame. A later real gameplay event must
 * own the destructive transition away from Entrance.
 */
static void resetValidatedEntranceChain(void) {
    Esp32Map1BspPass1_reset();
    Esp32Map1RuntimeLoad_reset();
    Esp32Map1AccessProbe_reset();
    Esp32Map1StateProbe_reset();
    Esp32Map1EventsProbe_reset();
    Esp32Map1EventDescriptorProbe_reset();
    Esp32Map1EventFilterProbe_reset();
    Esp32Map1OpcodeExecProbe_reset();
    Esp32Map1UiIntentProbe_reset();
    Esp32Map1StringReaderProbe_reset();
    Esp32Map1StatusMessageProbe_reset();
    Esp32Map1DialogOwnerProbe_reset();
    Esp32Map1NotebookProbe_reset();
    Esp32Map1KeyGateProbe_reset();
    Esp32Map1PasswordProbe_reset();
    Esp32Map1LineDoorProbe_reset();
    Esp32Map1UnlockProbe_reset();
    Esp32Map1GiveMapProbe_reset();
    Esp32Map1SaveRouteProbe_reset();
    Esp32Map1ChangeMapProbe_reset();
    Esp32Map1ShowHideFinalProbe_reset();
    Esp32Map1LevelExitStatsProbe_reset();
    Esp32PlayerExitStateProbe_reset();
    Esp32StatsMenuIntentProbe_reset();
    Esp32TransitionPreflightFinalProbe_reset();

    /* Explicitly clear the two destructive historical stages while never
     * servicing them during startup. This keeps a logical restart deterministic
     * without relying on a fresh MCU/BSS reset. */
    Esp32ResidentHandoffProbe_reset();
    Esp32CommittedTransitionProbe_reset();

    Esp32EntranceStartupRouteProbe_reset();
    Esp32EntranceSpawnChainProbe_reset();
    EspNativeGraphicsCatalog_reset();
    EspNativeFirstFrame_reset();
    EspNativeGameplayHud_reset();
    EspNativeResidentGameplay_reset();
}

static void serviceValidatedEntrancePredecessors(struct DoomRPG_s* doomRpg) {
    Esp32Map1RuntimeLoad_service(doomRpg);
    Esp32Map1AccessProbe_service(doomRpg);
    Esp32Map1StateProbe_service(doomRpg);
    Esp32Map1EventsProbe_service(doomRpg);
    Esp32Map1EventDescriptorProbe_service(doomRpg);
    Esp32Map1EventFilterProbe_service(doomRpg);
    Esp32Map1OpcodeExecProbe_service(doomRpg);
    Esp32Map1UiIntentProbe_service(doomRpg);
    Esp32Map1StringReaderProbe_service(doomRpg);
    Esp32Map1StatusMessageProbe_service(doomRpg);
    Esp32Map1DialogOwnerProbe_service(doomRpg);
    Esp32Map1NotebookProbe_service(doomRpg);
    Esp32Map1KeyGateProbe_service(doomRpg);
    Esp32Map1PasswordProbe_service(doomRpg);
    Esp32Map1LineDoorProbe_service(doomRpg);
    Esp32Map1UnlockProbe_service(doomRpg);
    Esp32Map1GiveMapProbe_service(doomRpg);
    Esp32Map1SaveRouteProbe_service(doomRpg);
    Esp32Map1ChangeMapProbe_service(doomRpg);
    Esp32Map1ShowHideFinalProbe_service(doomRpg);
    Esp32Map1LevelExitStatsProbe_service(doomRpg);
    Esp32PlayerExitStateProbe_service(doomRpg);
    Esp32StatsMenuIntentProbe_service(doomRpg);
    Esp32TransitionPreflightFinalProbe_service(doomRpg);
}

static void serviceEntranceFirstVisibleFrame(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    const EspNativeGraphicsCatalogView* catalog;
    const EspNativeFirstFrameState* frame;
    EspNativeGraphicsCatalogStatus catalogStatus;
    EspNativeFirstFrameStatus frameStatus;

    if (entranceFirstFrameAttempted || EspNativeFirstFrame_isReady()) return;
    if (doomRpg == NULL || doomRpg->render == NULL) {
        entranceFirstFrameAttempted = 1;
        printf("[ENTRANCEFRAME] FAILED missing DoomRPG/render owner\n");
        return;
    }

    if (!EspNativeGraphicsCatalog_isReady()) {
        catalogStatus = EspNativeGraphicsCatalog_buildFromRuntime();
        if (catalogStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
            catalogStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
            entranceFirstFrameAttempted = 1;
            printf("[ENTRANCEFRAME] FAILED graphics catalog status=%d\n",
                   (int)catalogStatus);
            return;
        }
    }

    catalog = EspNativeGraphicsCatalog_view();
    if (catalog == NULL) {
        entranceFirstFrameAttempted = 1;
        printf("[ENTRANCEFRAME] FAILED graphics catalog missing after build\n");
        return;
    }
    if (!entranceCatalogLogged) {
        printf("[ENTRANCEFRAME] CATALOG textures=%u sprites=%u storage=%u fnv=%08x packClosed=yes\n",
               (unsigned int)catalog->textureCount,
               (unsigned int)catalog->spriteCount,
               (unsigned int)catalog->storageBytes,
               (unsigned int)catalog->stateFNV1a);
        entranceCatalogLogged = 1;
    }

    view = EspPlayerView_view();
    entranceFirstFrameAttempted = 1;
    printf("\n=== Doom RPG ESP32-native Entrance first visible frame ===\n");
    frameStatus = EspNativeFirstFrame_route(doomRpg->render, view);
    if (frameStatus != ESP_NATIVE_FIRST_FRAME_OK) {
        printf("[ENTRANCEFRAME] FAILED route status=%d view=%p\n",
               (int)frameStatus,
               (const void*)view);
        return;
    }

    frame = EspNativeFirstFrame_view();
    if (frame == NULL) {
        printf("[ENTRANCEFRAME] FAILED route returned OK without published frame\n");
        return;
    }

    printf("[ENTRANCEFRAME] READY targetMap=%u frame=%08x->%08x walls=%u pixels=%u leaves=%u candidates=%u cache=%uH/%uM presented=%u next=initial-HUD\n",
           (unsigned int)frame->targetMapId,
           (unsigned int)frame->frameBeforeFNV,
           (unsigned int)frame->frameAfterFNV,
           (unsigned int)frame->wallDraws,
           (unsigned int)frame->pixelsDrawn,
           (unsigned int)frame->leafNodes,
           (unsigned int)frame->lineCandidates,
           (unsigned int)frame->cacheHits,
           (unsigned int)frame->cacheMisses,
           (unsigned int)frame->presented);
}

static void serviceEntranceInitialHud(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    EspNativeGameplayHudModel model;
    EspNativeGameplayHudStats stats;
    EspNativeGameplayHudStatus status;

    if (entranceHudAttempted || !EspNativeFirstFrame_isReady()) return;
    entranceHudAttempted = 1;

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[ENTRANCEHUD] FAILED missing DoomRPG/render owner\n");
        return;
    }
    view = EspPlayerView_view();
    if (view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0) {
        printf("[ENTRANCEHUD] FAILED unsettled player view=%p\n",
               (const void*)view);
        return;
    }

    /* Recovered new-game player model. Map identity/orientation are always
     * taken from the permanent native view; only the fresh-game combat values
     * remain this bounded startup contract until native player stats own them. */
    memset(&model, 0, sizeof(model));
    model.targetMapId = view->targetMapId;
    model.gameplayLoadMapId = view->gameplayLoadMapId;
    model.loadType = view->loadType;
    model.health = 30U;
    model.maxHealth = 30U;
    model.armor = 0U;
    model.maxArmor = 20U;
    model.ammo = 8U;
    model.weapon = 2U;
    model.ammoType = 1U;
    model.weaponsPresent = 1U;
    model.destAngle = (uint8_t)view->destAngle;

    memset(&stats, 0, sizeof(stats));
    printf("\n=== Doom RPG ESP32-native resident initial HUD ===\n");
    status = EspNativeGameplayHud_routeInitial(&model, &stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK &&
        status != ESP_NATIVE_GAMEPLAY_HUD_ALREADY_ACTIVE) {
        printf("[ENTRANCEHUD] FAILED status=%d map=%u loadMap=%u angle=%u\n",
               (int)status,
               (unsigned int)model.targetMapId,
               (unsigned int)model.gameplayLoadMapId,
               (unsigned int)model.destAngle);
        return;
    }
    if (!EspNativeGameplayHud_isReady()) {
        printf("[ENTRANCEHUD] FAILED route completed without HUD owner\n");
        return;
    }
    if (!Esp32PlatformVideo_present()) {
        printf("[ENTRANCEHUD] FAILED present\n");
        return;
    }

    entranceHudReady = 1;
    printf("[ENTRANCEHUD] READY map=%u hp=%u/%u armor=%u/%u weapon=%u ammo=%u angle=%u resources=%u pixels=%u reads=%u presented=1 next=world+sprites composite\n",
           (unsigned int)model.targetMapId,
           (unsigned int)model.health,
           (unsigned int)model.maxHealth,
           (unsigned int)model.armor,
           (unsigned int)model.maxArmor,
           (unsigned int)model.weapon,
           (unsigned int)model.ammo,
           (unsigned int)model.destAngle,
           (unsigned int)stats.resourcesValidated,
           (unsigned int)stats.pixelsWritten,
           (unsigned int)stats.packReads);
}

static void serviceEntranceGameplayComposite(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    const EspNativeGraphicsCatalogView* catalog;
    EspNativeGraphicsCatalogStatus dependencyStatus;
    EspNativeGameplayFrameStats stats;

    if (entranceCompositeAttempted || !entranceHudReady) return;
    entranceCompositeAttempted = 1;

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[ENTRANCECOMPOSITE] FAILED missing DoomRPG/render owner\n");
        return;
    }
    view = EspPlayerView_view();
    if (view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0) {
        printf("[ENTRANCECOMPOSITE] FAILED unsettled player view=%p\n",
               (const void*)view);
        return;
    }

    dependencyStatus = EspNativeGraphicsCatalog_expandSpriteDependencies();
    if (dependencyStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
        dependencyStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
        printf("[ENTRANCECOMPOSITE] FAILED sprite dependency status=%d\n",
               (int)dependencyStatus);
        return;
    }
    catalog = EspNativeGraphicsCatalog_view();
    if (catalog == NULL) {
        printf("[ENTRANCECOMPOSITE] FAILED catalog missing after dependency closure\n");
        return;
    }

    printf("\n=== Doom RPG ESP32-native resident gameplay composite ===\n");
    printf("[ENTRANCECOMPOSITE] CONTRACT generic resident world + full initial HUD + BSP-visible max-64 candidate sprite workset; free and N/S/E/W oriented bitshape sprites share one renderer; TILE/CROSS/special remain fail-closed with exact diagnostics\n");
    printf("[ENTRANCECOMPOSITE] CATALOG textures=%u sprites=%u storage=%u fnv=%08x dependencyStatus=%d\n",
           (unsigned int)catalog->textureCount,
           (unsigned int)catalog->spriteCount,
           (unsigned int)catalog->storageBytes,
           (unsigned int)catalog->stateFNV1a,
           (int)dependencyStatus);

    if (!EspNativeGameplayFrame_renderTurn(
            doomRpg->render, (uint8_t)view->viewAngle, &stats)) {
        printf("[ENTRANCECOMPOSITE] FAILED render angle=%d\n",
               (int)view->viewAngle);
        return;
    }

    entranceCompositeReady = 1;
    printf("[ENTRANCECOMPOSITE] READY map=%u angle=%u frame=%08x sprites=%u/%u glow=%u/%u walls=%u/%u planes=%u hudPixels=%u packReads=%u+%u totalUs=%u presented=%u next=touch+TURN+MOVE\n",
           (unsigned int)view->targetMapId,
           (unsigned int)stats.angle,
           (unsigned int)stats.frameAfterFNV,
           (unsigned int)stats.spriteDraws,
           (unsigned int)stats.spritePixels,
           (unsigned int)stats.glowDraws,
           (unsigned int)stats.glowPixels,
           (unsigned int)stats.wallDraws,
           (unsigned int)stats.wallPixels,
           (unsigned int)stats.planePixels,
           (unsigned int)stats.hudPixels,
           (unsigned int)stats.spritePackReads,
           (unsigned int)stats.hudPackReads,
           (unsigned int)stats.totalMicros,
           (unsigned int)stats.finalPresented);
}

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    EspProbeLog_setQuiet(0);
    EspProbeLog_clearBlockingFailure();
    fastForwardTotalPasses = 0U;
    fastForwardReadyLogged = 0;
    fastForwardWaitLogged = 0;
    fastForwardBlockedLogged = 0;
    entranceCatalogLogged = 0;
    entranceFirstFrameAttempted = 0;
    entranceHudAttempted = 0;
    entranceHudReady = 0;
    entranceCompositeAttempted = 0;
    entranceCompositeReady = 0;
    resetValidatedEntranceChain();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    unsigned int pass;

    __real_Esp32IntroDispose_service(doomRpg);

    /* Keep the already hardware-proven Entrance source semantics as silent
     * executable regression checks. The loop ends at the read-only transition
     * preflight and can never service resident handoff or committed transition.
     */
    EspProbeLog_setQuiet(1);
    Esp32Map1BspPass1_service(doomRpg);

    if (!EspProbeLog_hasBlockingFailure() &&
        Esp32Map1BspPass1_isDone() &&
        !Esp32TransitionPreflightFinalProbe_isDone()) {
        for (pass = 0U;
             pass < VALIDATED_FAST_FORWARD_MAX_PASSES &&
             !Esp32TransitionPreflightFinalProbe_isDone() &&
             !EspProbeLog_hasBlockingFailure();
             ++pass) {
            serviceValidatedEntrancePredecessors(doomRpg);
            ++fastForwardTotalPasses;
            if (!EspProbeLog_hasBlockingFailure()) vTaskDelay(1);
        }
    }
    EspProbeLog_setQuiet(0);

    if (EspProbeLog_hasBlockingFailure()) {
        if (!fastForwardBlockedLogged) {
            printf("[NATIVEBOOT] BLOCKED Entrance predecessor probe failure after %u silent passes; resident handoff/committed transition forbidden\n",
                   fastForwardTotalPasses);
            fastForwardBlockedLogged = 1;
        }
        return;
    }

    if (!Esp32Map1BspPass1_isDone()) return;

    if (!Esp32TransitionPreflightFinalProbe_isDone()) {
        if (!fastForwardWaitLogged) {
            printf("[NATIVEBOOT] WAIT Entrance source validation incomplete after %u silent passes; no map swap attempted\n",
                   fastForwardTotalPasses);
            fastForwardWaitLogged = 1;
        }
        return;
    }

    if (!fastForwardReadyLogged) {
        printf("[NATIVEBOOT] ENTRANCE source validation complete silent passes=%u; production startup stops before ResidentHandoff/CommittedTransition and continues into Entrance spawn\n",
               fastForwardTotalPasses);
        fastForwardReadyLogged = 1;
    }

    Esp32EntranceStartupRouteProbe_service(doomRpg);
    if (Esp32EntranceStartupRouteProbe_isDone()) {
        Esp32EntranceSpawnChainProbe_service(doomRpg);
    }
    if (Esp32EntranceSpawnChainProbe_isReady()) {
        serviceEntranceFirstVisibleFrame(doomRpg);
    }
    if (EspNativeFirstFrame_isReady()) {
        serviceEntranceInitialHud(doomRpg);
    }
    if (entranceHudReady) {
        serviceEntranceGameplayComposite(doomRpg);
    }
    if (entranceCompositeReady) {
        EspNativeResidentGameplay_service(doomRpg);
    }
}
