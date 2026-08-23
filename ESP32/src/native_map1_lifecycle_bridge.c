#include "native_intro_dispose.h"
#include "native_committed_transition_probe.h"
#include "native_junction_facing_probe.h"
#include "native_junction_finish_rotation_tile_probe.h"
#include "native_junction_hud_refresh_probe.h"
#include "native_junction_initial_tile_probe.h"
#include "native_junction_orientation_probe.h"
#include "native_junction_player_setup_probe.h"
#include "native_junction_player_view_probe.h"
#include "native_junction_post_load_event_particle_cleanup_probe.h"
#include "native_junction_post_load_flag_cleanup_probe.h"
#include "native_junction_post_load_givemap_probe.h"
#include "native_junction_post_load_hud_clear_probe.h"
#include "native_junction_post_load_initial_save_intent_probe.h"
#include "native_junction_post_load_weapon_select_probe.h"
#include "native_junction_spawn_probe.h"
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

/*
 * Keep the hardware-validated intro clock/dispose, native BSP pass-1 and native
 * resident-runtime implementations untouched. These wrappers are temporary
 * lifecycle scaffolding only; reusable native map/player/UI/transition
 * components remain legacy-engine free.
 */
void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
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
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * Final Continue -> validated intro teardown -> compact native map/resident
     * owners -> bounded opcode/UI/world semantic owners -> committed Junction
     * residency -> fresh-map spawn/player/HUD/setup -> first tile dispatch ->
     * finishRotation orientation -> second tile dispatch -> durable native
     * facing -> post-load HUD message clear -> direct Junction Game_givemap ->
     * current-weapon self-selection -> initial-save caller intent -> scalar
     * isLoaded/isSaved/activeLoadType cleanup -> empty event/particle cleanup.
     * Durable save persistence, isUpdateView and ST_PLAYING remain outside this
     * chain. Each stage arms first and executes on a later loop.
     */
    Esp32Map1BspPass1_service(doomRpg);
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
}
