#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_native_plane_renderer.h"
#include "esp_probe_log.h"
#include "native_intro_dispose.h"
#include "native_committed_transition_probe.h"
#include "native_junction_facing_probe.h"
#include "native_junction_finish_rotation_tile_probe.h"
#include "native_junction_first_frame_corrected_probe.h"
#include "native_junction_gameplay_hud_probe.h"
#include "native_junction_gameplay_input_probe.h"
#include "native_junction_graphics_catalog_probe.h"
#include "native_junction_hud_refresh_probe.h"
#include "native_junction_initial_tile_probe.h"
#include "native_junction_move_collision_probe.h"
#include "native_junction_orientation_probe.h"
#include "native_junction_player_setup_probe.h"
#include "native_junction_player_view_probe.h"
#include "native_junction_playing_service_probe.h"
#include "native_junction_post_load_event_particle_cleanup_probe.h"
#include "native_junction_post_load_flag_cleanup_probe.h"
#include "native_junction_post_load_givemap_probe.h"
#include "native_junction_post_load_hud_clear_probe.h"
#include "native_junction_post_load_idle_time_probe.h"
#include "native_junction_post_load_initial_save_intent_probe.h"
#include "native_junction_post_load_playing_transition_probe.h"
#include "native_junction_post_load_view_invalidation_probe.h"
#include "native_junction_post_load_weapon_select_probe.h"
#include "native_junction_spawn_probe.h"
#include "native_junction_sprite_census_probe.h"
#include "native_junction_sprite_fidelity_probe.h"
#include "native_junction_sprite_overlay_probe.h"
#include "native_junction_turn_dispatch_probe.h"
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

#define VALIDATED_FAST_FORWARD_MAX_PASSES 64U

static unsigned int fastForwardTotalPasses;
static int fastForwardReadyLogged;
static int fastForwardWaitLogged;
static int fastForwardBlockedLogged;

void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

/* Temporary frame-fidelity diagnostic owned by
 * native_first_frame_color_probe_wrappers.c.  The BMP write is deliberately
 * outside the strict renderer integrity contract because first-use stdio/SD
 * may retain a small VFS/libc allocation.
 */
void Esp32FirstFrameDiagnostic_reset(void);
int Esp32FirstFrameDiagnostic_exportBmp(void);

static void resetValidatedChain(void) {
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
    Esp32ResidentHandoffProbe_reset();
    Esp32CommittedTransitionProbe_reset();
    Esp32JunctionSpawnProbe_reset();
    Esp32JunctionPlayerViewProbe_reset();
    Esp32JunctionHudRefreshProbe_reset();
    Esp32JunctionPlayerSetupProbe_reset();
    Esp32JunctionInitialTileProbe_reset();
    Esp32JunctionOrientationProbe_reset();
    Esp32JunctionFinishRotationTileProbe_reset();
    Esp32JunctionFacingProbe_reset();
    Esp32JunctionPostLoadHudClearProbe_reset();
    Esp32JunctionPostLoadGiveMapProbe_reset();
    Esp32JunctionPostLoadWeaponSelectProbe_reset();
    Esp32JunctionPostLoadInitialSaveIntentProbe_reset();
    Esp32JunctionPostLoadFlagCleanupProbe_reset();
    Esp32JunctionPostLoadEventParticleCleanupProbe_reset();
    Esp32JunctionPostLoadViewInvalidationProbe_reset();
    Esp32JunctionPostLoadPlayingTransitionProbe_reset();
    Esp32JunctionPostLoadIdleTimeProbe_reset();
    Esp32JunctionPlayingServiceProbe_reset();
    Esp32JunctionGraphicsCatalogProbe_reset();
    Esp32JunctionFirstFrameCorrectedProbe_reset();
    Esp32JunctionSpriteCensusProbe_reset();
    Esp32JunctionSpriteFidelityProbe_reset();
    Esp32JunctionSpriteOverlayProbe_reset();
    Esp32JunctionGameplayHudProbe_reset();
    Esp32JunctionGameplayInputProbe_reset();
    Esp32JunctionTurnDispatchProbe_reset();
    Esp32JunctionMoveCollisionProbe_reset();
    EspNativePlaneRenderer_reset();
    Esp32FirstFrameDiagnostic_reset();
}

static void serviceValidatedPredecessors(struct DoomRPG_s* doomRpg) {
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
    Esp32ResidentHandoffProbe_service(doomRpg);
    Esp32CommittedTransitionProbe_service(doomRpg);
    Esp32JunctionSpawnProbe_service(doomRpg);
    Esp32JunctionPlayerViewProbe_service(doomRpg);
    Esp32JunctionHudRefreshProbe_service(doomRpg);
    Esp32JunctionPlayerSetupProbe_service(doomRpg);
    Esp32JunctionInitialTileProbe_service(doomRpg);
    Esp32JunctionOrientationProbe_service(doomRpg);
    Esp32JunctionFinishRotationTileProbe_service(doomRpg);
    Esp32JunctionFacingProbe_service(doomRpg);
    Esp32JunctionPostLoadHudClearProbe_service(doomRpg);
    Esp32JunctionPostLoadGiveMapProbe_service(doomRpg);
    Esp32JunctionPostLoadWeaponSelectProbe_service(doomRpg);
    Esp32JunctionPostLoadInitialSaveIntentProbe_service(doomRpg);
    Esp32JunctionPostLoadFlagCleanupProbe_service(doomRpg);
    Esp32JunctionPostLoadEventParticleCleanupProbe_service(doomRpg);
    Esp32JunctionPostLoadViewInvalidationProbe_service(doomRpg);
    Esp32JunctionPostLoadPlayingTransitionProbe_service(doomRpg);
    Esp32JunctionPostLoadIdleTimeProbe_service(doomRpg);
    Esp32JunctionPlayingServiceProbe_service(doomRpg);
    Esp32JunctionGraphicsCatalogProbe_service(doomRpg);
}

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    EspProbeLog_setQuiet(0);
    EspProbeLog_clearBlockingFailure();
    fastForwardTotalPasses = 0U;
    fastForwardReadyLogged = 0;
    fastForwardWaitLogged = 0;
    fastForwardBlockedLogged = 0;
    resetValidatedChain();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    unsigned int pass;

    __real_Esp32IntroDispose_service(doomRpg);

    /* Historical probes remain executable source-of-truth checks, but their
     * successful chatter is no longer useful on every firmware flash. Pipeline
     * the already hardware-proven owners once BSP pass-1 is complete. A probe-
     * level FAILED/ERROR latches the fast-forward and forbids current-frame
     * execution even if that historical probe marks itself done after recovery.
     */
    EspProbeLog_setQuiet(1);
    Esp32Map1BspPass1_service(doomRpg);

    if (!EspProbeLog_hasBlockingFailure() &&
        Esp32Map1BspPass1_isDone() &&
        !Esp32JunctionGraphicsCatalogProbe_isDone()) {
        for (pass = 0U;
             pass < VALIDATED_FAST_FORWARD_MAX_PASSES &&
             !Esp32JunctionGraphicsCatalogProbe_isDone() &&
             !EspProbeLog_hasBlockingFailure();
             ++pass) {
            serviceValidatedPredecessors(doomRpg);
            ++fastForwardTotalPasses;
            if (!EspProbeLog_hasBlockingFailure()) vTaskDelay(1);
        }
    }
    EspProbeLog_setQuiet(0);

    if (EspProbeLog_hasBlockingFailure()) {
        /* A current gameplay-probe failure must still allow the already-drawn
         * transient touch overlay to expire and restore its saved pixels. This
         * service performs no gameplay dispatch when the input probe is merely
         * active; it only completes its bounded feedback timer/restore path. */
        if (Esp32JunctionGameplayInputProbe_isActive()) {
            Esp32JunctionGameplayInputProbe_service(doomRpg);
        }
        if (!fastForwardBlockedLogged) {
            printf("[NATIVEBOOT] BLOCKED predecessor probe failure after %u silent passes; current first-frame probe NOT started\n",
                   fastForwardTotalPasses);
            fastForwardBlockedLogged = 1;
        }
        return;
    }

    if (!Esp32Map1BspPass1_isDone()) return;

    if (!Esp32JunctionGraphicsCatalogProbe_isDone()) {
        if (!fastForwardWaitLogged) {
            printf("[NATIVEBOOT] WAIT validated predecessor chain incomplete after %u silent passes\n",
                   fastForwardTotalPasses);
            fastForwardWaitLogged = 1;
        }
        return;
    }

    if (!fastForwardReadyLogged) {
        printf("[NATIVEBOOT] READY validated predecessors silent passes=%u catalog=969d5a77; current first-frame fidelity probe starts now\n",
               fastForwardTotalPasses);
        fastForwardReadyLogged = 1;
    }

    Esp32JunctionFirstFrameCorrectedProbe_service(doomRpg);

    /* Only a strict PARK may trigger diagnostics. The first-frame probe has
     * already captured heap/largest/PAK integrity before these calls. */
    if (Esp32JunctionFirstFrameCorrectedProbe_isDone()) {
        (void)Esp32FirstFrameDiagnostic_exportBmp();
        Esp32JunctionSpriteCensusProbe_service();
        Esp32JunctionSpriteFidelityProbe_preOverlayService(doomRpg);
        if (Esp32JunctionSpriteFidelityProbe_preOverlayDone()) {
            Esp32JunctionSpriteOverlayProbe_service(doomRpg);
            Esp32JunctionSpriteFidelityProbe_postOverlayService(doomRpg);
            if (Esp32JunctionSpriteFidelityProbe_postOverlayDone()) {
                Esp32JunctionGameplayHudProbe_service(doomRpg);
                if (Esp32JunctionGameplayHudProbe_isDone()) {
                    Esp32JunctionGameplayInputProbe_service(doomRpg);
                    if (Esp32JunctionGameplayInputProbe_isActive()) {
                        Esp32JunctionTurnDispatchProbe_service(doomRpg);
                        if (Esp32JunctionTurnDispatchProbe_isActive()) {
                            Esp32JunctionMoveCollisionProbe_service(doomRpg);
                        }
                    }
                }
            }
        }
    }
}
