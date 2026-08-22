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

/*
 * Keep the hardware-validated intro clock/dispose, native BSP pass-1 and native
 * resident-runtime implementations untouched. These wrappers are temporary
 * lifecycle scaffolding only; reusable native map components remain
 * legacy-engine free.
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
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * Final Continue -> validated intro teardown -> native BSP pass 1 -> native
     * compact resident arena -> allocation-free indexed accessor validation ->
     * compact mutable tile state -> allocation-free tile/event lookup ->
     * read-only event descriptor/bytecode linkage -> compact mutable script
     * state + side-effect-free execution filtering -> opcode inventory + first
     * reversible native state-opcode execution -> allocation-free string spans
     * + UI/string intent translation -> bounded pack-backed one-string reader
     * -> compact EV_FORCEMESSAGE status-message owner -> compact DIALOG/NOBACK
     * pause owner -> bounded EV_NOTE notebook owner -> pure EV_CHECK_KEY control
     * gate -> bounded EV_PASSWORD pause/submit owner -> compact line open/lock
     * world state + reversible EV_OPENLINE/EV_CLOSELINE execution -> compact
     * line texture 9/10 state + reversible EV_UNLOCK execution -> compact
     * line/sprite automap reveal state + reversible EV_GIVEMAP execution ->
     * caller-owned EV_SAVEGAME future-save route state -> caller-owned
     * EV_CHANGEMAP pending transition intent -> compact map-sprite entity
     * topology + corrected isolated/contextual EV_SHOW/EV_HIDE proof -> pure
     * native level-exit stats snapshot for the CHANGEMAP show-stats consumer.
     * Each stage arms first and executes on a later Arduino loop service.
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
}
