#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_probe_log.h"
#include "native_committed_transition_probe.h"
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

#define VALIDATED_FAST_FORWARD_MAX_PASSES 64U

static unsigned int fastForwardTotalPasses;
static int fastForwardReadyLogged;
static int fastForwardWaitLogged;
static int fastForwardBlockedLogged;

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
 * the preflight, but /intro.bsp (Entrance) remains the resident map. A later
 * real gameplay event must own the destructive transition.
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

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    EspProbeLog_setQuiet(0);
    EspProbeLog_clearBlockingFailure();
    fastForwardTotalPasses = 0U;
    fastForwardReadyLogged = 0;
    fastForwardWaitLogged = 0;
    fastForwardBlockedLogged = 0;
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
        printf("[NATIVEBOOT] ENTRANCE source validation complete silent passes=%u; production startup stops before ResidentHandoff/CommittedTransition\n",
               fastForwardTotalPasses);
        fastForwardReadyLogged = 1;
    }

    Esp32EntranceStartupRouteProbe_service(doomRpg);
}
