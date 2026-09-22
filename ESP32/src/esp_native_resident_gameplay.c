#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_ui_intent.h"
#include "esp_native_first_frame.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_automap.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_move_events.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_pass_turn.h"
#include "esp_native_gameplay_password.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_select.h"
#include "esp_native_gameplay_weapon_control.h"
#include "esp_native_resident_gameplay.h"
#include "esp_player_view_state.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

typedef struct EspNativeResidentGameplayState_s {
    uint32_t taps;
    uint32_t actions;
    uint32_t turns;
    uint32_t moves;
    uint32_t selects;
    uint32_t dialogs;
    uint32_t dialogResumes;
    uint32_t dialogCancels;
    uint32_t blocked;
    uint32_t deferred;
    uint32_t selectRefused;
    uint8_t active;
    uint8_t failed;
    uint8_t checkpointResumeArmed;
    uint8_t modeFlags;
} EspNativeResidentGameplayState;

static EspNativeResidentGameplayState gameplayState;

#define RESIDENT_MODE_AUTOMAP 0x01U

static int automapActive(void) {
    return (gameplayState.modeFlags & RESIDENT_MODE_AUTOMAP) != 0U;
}

static void disableGameplay(const char* reason) {
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(1, NULL);
    }
    EspNativeGameplayHub_reset();
    EspNativeGameplayDialog_reset();
    EspNativeGameplayPassword_reset();
    gameplayState.failed = 1U;
    gameplayState.active = 0U;
    PlatformInput_setTapCallback(NULL);
    printf("[RESIDENTGAMEPLAY] FAILED reason=%s\n",
           reason != NULL ? reason : "unknown");
}

static int ensureCollisionCatalog(void) {
    EspAssetPackEntry entityDefs;
    int ok = 0;

    if (EspEntityDefTypeCatalog_isReady()) return 1;
    if (EspAssetPack_isOpen()) return 0;
    memset(&entityDefs, 0, sizeof(entityDefs));

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    if (EspAssetPack_findEntry("/entities.db", &entityDefs) &&
        (entityDefs.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) == 0U &&
        EspEntityDefTypeCatalog_buildFromPackEntry(&entityDefs)) {
        ok = 1;
    }
    EspAssetPack_close();
    return ok && EspEntityDefTypeCatalog_isReady() && !EspAssetPack_isOpen();
}

static void onGameplayTap(int16_t screenX,
                          int16_t screenY,
                          uint16_t pressure,
                          uint16_t rawX,
                          uint16_t rawY) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputStatus status;
    EspNativeGameplayControlsStats feedbackStats;
    int logicalX;
    int logicalY;

    (void)pressure;
    (void)rawX;
    (void)rawY;

    if (!gameplayState.active || gameplayState.failed) return;
    if (screenX < 0 || screenY < 0) return;

    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }

    /*
     * EV_PASSWORD owns its explicit 3x4 keypad. Keep raw logical taps out of
     * the normal 12-zone gameplay map, and keep the submitted modal blocked
     * until the resident service has consumed its continuation result.
     */
    if (EspNativeGameplayPassword_isActive()) {
        EspNativeGameplayPasswordTapStatus passwordStatus;

        ++gameplayState.taps;
        passwordStatus =
            EspNativeGameplayPassword_handleTap(logicalX, logicalY);
        if (passwordStatus == ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_INVALID) {
            disableGameplay("password-touch");
            return;
        }
        if (passwordStatus != ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_IGNORED) {
            printf("[RESIDENTGAMEPLAY] PASSWORD-TAP tap=%u logical=%d,%d status=%d modal=%s feedback=keypad-only\n",
                   (unsigned int)gameplayState.taps,
                   logicalX,
                   logicalY,
                   (int)passwordStatus,
                   passwordStatus == ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_SUBMITTED
                       ? "submitted"
                       : "active");
        }
        return;
    }
    if (EspNativeGameplayPassword_hasPendingCompletion()) {
        return;
    }

    /*
     * Dialog owns the whole touch surface. A tap anywhere means the same
     * semantic SELECT that the legacy HIT key used: first finish the current
     * typewriter page, then advance/page/resume on later taps. Keep this route
     * separate from the world/HUB hit map so no movement, weapon or menu action
     * can leak through while dialog is active. Deliberately skip the normal
     * zone-flash overlay here: a full-screen feedback rectangle would be noisy
     * and would consume the bounded edit owner for no gameplay value.
     */
    if (EspNativeGameplayDialog_isActive()) {
        memset(&hit, 0, sizeof(hit));
        hit.action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
        hit.zone = ESP_NATIVE_GAMEPLAY_ZONE_SELECT;
        hit.left = 0U;
        hit.top = 0U;
        hit.right = (uint8_t)(DOOMRPG_LOGICAL_WIDTH - 1);
        hit.bottom = (uint8_t)(DOOMRPG_LOGICAL_HEIGHT - 1);

        ++gameplayState.taps;
        status = EspNativeGameplayInput_route(&hit, logicalX, logicalY);
        if (status == ESP_NATIVE_GAMEPLAY_INPUT_OK) {
            printf("[RESIDENTGAMEPLAY] QUEUE tap=%u action=SELECT zone=%u logical=%d,%d context=DIALOG tapDomain=full-screen feedback=none\n",
                   (unsigned int)gameplayState.taps,
                   (unsigned int)hit.zone,
                   logicalX,
                   logicalY);
        }
        else if (status == ESP_NATIVE_GAMEPLAY_INPUT_BUSY) {
            printf("[RESIDENTGAMEPLAY] BUSY tap=%u action=SELECT context=DIALOG pending=1\n",
                   (unsigned int)gameplayState.taps);
        }
        return;
    }

    status = EspNativeGameplayInput_classify(logicalX, logicalY, &hit);
    if (status != ESP_NATIVE_GAMEPLAY_INPUT_OK) return;

    ++gameplayState.taps;
    status = EspNativeGameplayInput_route(&hit, logicalX, logicalY);
    if (status == ESP_NATIVE_GAMEPLAY_INPUT_OK) {
        memset(&feedbackStats, 0, sizeof(feedbackStats));
        if (!EspNativeGameplayControls_begin(&hit, &feedbackStats) ||
            !Esp32PlatformVideo_present()) {
            (void)EspNativeGameplayControls_restore(0, NULL);
            disableGameplay("touch-feedback-draw");
            return;
        }
        printf("[RESIDENTGAMEPLAY] QUEUE tap=%u action=%s zone=%u logical=%d,%d context=%s\n",
               (unsigned int)gameplayState.taps,
               EspNativeGameplayInput_actionName(hit.action),
               (unsigned int)hit.zone,
               logicalX,
               logicalY,
               EspNativeGameplayHub_isActive()
                   ? "HUB"
                   : (EspNativeGameplayDialog_isActive()
                          ? "DIALOG"
                          : (automapActive() ? "AUTOMAP" : "WORLD")));
        printf("[TOUCHFEEDBACK] FLASH zone=%u action=%s edits=%u hold=%ums frame=%08x->%08x style=semantic-neon-double-ring+vector-glyph\n",
               (unsigned int)feedbackStats.zone,
               EspNativeGameplayInput_actionName(feedbackStats.action),
               (unsigned int)feedbackStats.edits,
               (unsigned int)ESP_NATIVE_GAMEPLAY_FEEDBACK_MS,
               (unsigned int)feedbackStats.baselineFNV,
               (unsigned int)feedbackStats.overlayFNV);
    }
    else if (status == ESP_NATIVE_GAMEPLAY_INPUT_BUSY) {
        printf("[RESIDENTGAMEPLAY] BUSY tap=%u action=%s pending=1\n",
               (unsigned int)gameplayState.taps,
               EspNativeGameplayInput_actionName(hit.action));
    }
}

static int renderCurrent(Render_t* render,
                         uint8_t angle,
                         const char* reason) {
    EspNativeGameplayFrameStats frame;

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(render, angle, &frame)) {
        printf("[RESIDENTGAMEPLAY] RENDER-FAILED reason=%s angle=%u\n",
               reason != NULL ? reason : "action",
               (unsigned int)angle);
        return 0;
    }

    printf("[RESIDENTGAMEPLAY] FRAME reason=%s angle=%u frame=%08x sprites=%u/%u walls=%u pixels=%u totalUs=%u presented=%u controls=idle-invisible\n",
           reason != NULL ? reason : "action",
           (unsigned int)frame.angle,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.spriteDraws,
           (unsigned int)frame.spritePixels,
           (unsigned int)frame.wallDraws,
           (unsigned int)frame.wallPixels,
           (unsigned int)frame.totalMicros,
           (unsigned int)frame.finalPresented);
    return 1;
}

static int renderAutomapCurrent(Render_t* render, const char* reason);
static int closeAutomap(Render_t* render, const char* reason);

static int renderActionCurrent(Render_t* render,
                               uint8_t angle,
                               const char* reason) {
    if (automapActive()) {
        return renderAutomapCurrent(render, reason);
    }
    return renderCurrent(render, angle, reason);
}

static void serviceTurn(Render_t* render,
                        const EspNativeGameplayInputState* intent) {
    EspPlayerViewState beforeView;
    EspPlayerViewState afterView;
    EspNativeGameplayTurnState beforeTurn;
    EspNativeGameplayTurnState afterTurn;
    EspNativeGameplayDispatchResult result;
    EspNativeGameplayDispatchStatus status;

    memset(&beforeView, 0, sizeof(beforeView));
    memset(&afterView, 0, sizeof(afterView));
    memset(&beforeTurn, 0, sizeof(beforeTurn));
    memset(&afterTurn, 0, sizeof(afterTurn));
    memset(&result, 0, sizeof(result));

    status = EspNativeGameplayDispatch_prepareTurn(
        intent, &beforeView, &afterView, &beforeTurn, &afterTurn, &result);
    if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED) {
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] TURN-DEFER action=%s status=%d\n",
               EspNativeGameplayInput_actionName(intent->action), (int)status);
        return;
    }

    status = EspNativeGameplayDispatch_commitTurn(
        &beforeView, &afterView, &beforeTurn, &afterTurn, &result);
    if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        disableGameplay("turn-commit");
        return;
    }

    if (!renderActionCurrent(render, (uint8_t)afterView.viewAngle, "TURN")) {
        status = EspNativeGameplayDispatch_rollbackTurn(
            &afterView, &beforeView, &afterTurn, &beforeTurn, &result);
        if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            !renderActionCurrent(render, (uint8_t)beforeView.viewAngle,
                                 "TURN-ROLLBACK")) {
            disableGameplay("turn-render-rollback");
            return;
        }
        printf("[RESIDENTGAMEPLAY] TURN ROLLBACK action=%s angle=%d\n",
               EspNativeGameplayInput_actionName(intent->action),
               (int)beforeView.viewAngle);
        return;
    }

    ++gameplayState.turns;
    printf("[RESIDENTGAMEPLAY] TURN n=%u seq=%u action=%s angle=%u->%u committed=yes\n",
           (unsigned int)gameplayState.turns,
           (unsigned int)result.sequence,
           EspNativeGameplayInput_actionName(intent->action),
           (unsigned int)result.angleBefore,
           (unsigned int)result.angleAfter);
}

static void serviceMove(Render_t* render,
                        const EspNativeGameplayInputState* intent) {
    EspPlayerViewState beforeView;
    EspPlayerViewState afterView;
    EspNativeGameplayMoveResult result;
    EspNativeGameplayMoveDialogIntent moveDialog;
    EspNativeGameplayDispatchStatus status;
    const uint8_t startedInAutomap = automapActive() ? 1U : 0U;

    memset(&beforeView, 0, sizeof(beforeView));
    memset(&afterView, 0, sizeof(afterView));
    memset(&result, 0, sizeof(result));
    memset(&moveDialog, 0, sizeof(moveDialog));

    status = EspNativeGameplayDispatch_prepareMove(
        intent, &beforeView, &afterView, &result);
    if (status == ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_BLOCKED) {
        uint8_t legacyAdvance = 0U;
        if (startedInAutomap != 0U) {
            legacyAdvance =
                (uint8_t)(EspNativeGameplayMonsterTurn_requestBlockedAutomapMove(
                              intent->sequence)
                              ? 1U
                              : 0U);
        }
        ++gameplayState.blocked;
        printf("[RESIDENTGAMEPLAY] MOVE-BLOCKED n=%u seq=%u action=%s tile=%u->%u blocker=%u type=%u context=%s legacyAdvance=%s\n",
               (unsigned int)gameplayState.blocked,
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.sourceTile,
               (unsigned int)result.destTile,
               (unsigned int)result.blockerSpriteIndex,
               (unsigned int)result.blockerType,
               startedInAutomap != 0U ? "AUTOMAP" : "WORLD",
               startedInAutomap != 0U
                   ? (legacyAdvance != 0U ? "yes" : "DEFER")
                   : "no");
        return;
    }
    if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED) {
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] MOVE-DEFER action=%s status=%d collision=%u\n",
               EspNativeGameplayInput_actionName(intent->action),
               (int)status,
               (unsigned int)result.collisionStatus);
        return;
    }

    status = EspNativeGameplayDispatch_commitMove(&beforeView, &afterView, &result);
    if (status == ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED) {
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] MOVE-EVENT-DEFER n=%u seq=%u action=%s tile=%u->%u worldStable=yes gameplayActive=yes\n",
               (unsigned int)gameplayState.deferred,
               (unsigned int)result.sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.sourceTile,
               (unsigned int)result.destTile);
        return;
    }
    if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        disableGameplay("move-commit");
        return;
    }

    if (!renderActionCurrent(render, (uint8_t)afterView.viewAngle, "MOVE")) {
        status = EspNativeGameplayDispatch_rollbackMove(
            &afterView, &beforeView, &result);
        if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            !renderActionCurrent(render, (uint8_t)beforeView.viewAngle,
                                 "MOVE-ROLLBACK")) {
            disableGameplay("move-render-rollback");
            return;
        }
        printf("[RESIDENTGAMEPLAY] MOVE ROLLBACK action=%s pos=%d,%d\n",
               EspNativeGameplayInput_actionName(intent->action),
               (int)beforeView.viewX,
               (int)beforeView.viewY);
        return;
    }

    if (EspNativeGameplayMoveEvents_pendingDialog(result.sequence, &moveDialog)) {
        EspNativeGameplayDialogBeginStatus dialogStatus;

        if (startedInAutomap != 0U &&
            !closeAutomap(render, "AUTOMAP-MOVE-DIALOG")) {
            status = EspNativeGameplayDispatch_rollbackMove(
                &afterView, &beforeView, &result);
            if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
                !renderAutomapCurrent(render, "MOVE-DIALOG-CLOSE-ROLLBACK")) {
                disableGameplay("move-dialog-close-rollback");
                return;
            }
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] MOVE-DIALOG-DEFER n=%u seq=%u event=%u cmd=%u opcode=%u status=automap-close-failed moveRolledBack=yes automapRestored=yes\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)result.sequence,
                   (unsigned int)moveDialog.eventIndex,
                   (unsigned int)moveDialog.commandOffset,
                   (unsigned int)moveDialog.codeId);
            return;
        }

        dialogStatus = EspNativeGameplayDialog_begin(
                moveDialog.eventIndex,
                moveDialog.commandOffset,
                moveDialog.runFlags);
        if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            int rollbackPresented = 0;
            status = EspNativeGameplayDispatch_rollbackMove(
                &afterView, &beforeView, &result);
            if (status == ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK) {
                if (startedInAutomap != 0U) {
                    gameplayState.modeFlags =
                        (uint8_t)(gameplayState.modeFlags |
                                  RESIDENT_MODE_AUTOMAP);
                    rollbackPresented =
                        renderAutomapCurrent(render, "MOVE-DIALOG-ROLLBACK");
                }
                else {
                    rollbackPresented =
                        renderCurrent(render, (uint8_t)beforeView.viewAngle,
                                      "MOVE-DIALOG-ROLLBACK");
                }
            }
            if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
                !rollbackPresented) {
                disableGameplay("move-dialog-open-rollback");
                return;
            }
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] MOVE-DIALOG-DEFER n=%u seq=%u event=%u cmd=%u opcode=%u status=%s moveRolledBack=yes gameplayActive=yes\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)result.sequence,
                   (unsigned int)moveDialog.eventIndex,
                   (unsigned int)moveDialog.commandOffset,
                   (unsigned int)moveDialog.codeId,
                   EspNativeGameplayDialog_beginStatusName(dialogStatus));
            return;
        }
        if (!EspNativeGameplayMoveEvents_finishPendingDialog(result.sequence)) {
            disableGameplay("move-dialog-finish-lease");
            return;
        }
        ++gameplayState.moves;
        ++gameplayState.dialogs;
        if (startedInAutomap == 0U) {
            uint16_t uncovered = 0U;
            if (!EspNativeGameplayAutomap_uncoverAt(
                    afterView.destX, afterView.destY, &uncovered)) {
                disableGameplay("automap-uncover-move-dialog");
                return;
            }
            printf("[AUTOMAP] UNCOVER reason=MOVE-DIALOG tile=%u mutated=%u state=ready\n",
                   (unsigned int)result.destTile,
                   (unsigned int)uncovered);
        }
        printf("[RESIDENTGAMEPLAY] MOVE-DIALOG n=%u seq=%u action=%s tile=%u->%u event=%u cmd=%u opcode=%u active=yes back=%s pauseScript=yes skipTurn=yes continuation=preflighted committed=yes\n",
               (unsigned int)gameplayState.dialogs,
               (unsigned int)result.sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.sourceTile,
               (unsigned int)result.destTile,
               (unsigned int)moveDialog.eventIndex,
               (unsigned int)moveDialog.commandOffset,
               (unsigned int)moveDialog.codeId,
               moveDialog.codeId == ESP_MAP_OPCODE_DIALOG ? "yes" : "no");
        return;
    }

    if (startedInAutomap == 0U) {
        uint16_t uncovered = 0U;
        if (!EspNativeGameplayAutomap_uncoverAt(
                afterView.destX, afterView.destY, &uncovered)) {
            disableGameplay("automap-uncover-move");
            return;
        }
        printf("[AUTOMAP] UNCOVER reason=MOVE tile=%u mutated=%u state=ready\n",
               (unsigned int)result.destTile,
               (unsigned int)uncovered);
    }
    ++gameplayState.moves;
    printf("[RESIDENTGAMEPLAY] MOVE n=%u seq=%u action=%s tile=%u->%u delta=%d,%d pos=%d,%d moveEvents=door15/16+force24+enter-dialog8/26-live-other-deferred committed=yes\n",
           (unsigned int)gameplayState.moves,
           (unsigned int)result.sequence,
           EspNativeGameplayInput_actionName(intent->action),
           (unsigned int)result.sourceTile,
           (unsigned int)result.destTile,
           (int)result.deltaX,
           (int)result.deltaY,
           (int)afterView.viewX,
           (int)afterView.viewY);
}

static void serviceWeaponControl(Render_t* render,
                                 const EspNativeGameplayInputState* intent) {
    EspNativeGameplayWeaponControlResult result;
    EspNativeGameplayWeaponControlStatus status;
    EspNativeGameplayPlayerState before;
    EspNativeGameplayPlayerState after;
    const EspPlayerViewState* view;
    uint32_t fnvAfter;

    memset(&result, 0, sizeof(result));
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    status = EspNativeGameplayWeaponControl_prepare(intent, &result);

    if (status == ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_UNCHANGED) {
        printf("[WEAPONCONTROL] UNCHANGED seq=%u action=%s weapon=%u weapons=%04x inspected=%u reason=no-other-owned-usable mutation=no turn=no\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.weaponBefore,
               (unsigned int)result.weapons,
               (unsigned int)result.inspected);
        return;
    }
    if (status != ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_PREPARED ||
        !EspNativeGameplayPlayerState_snapshot(&before)) {
        ++gameplayState.deferred;
        printf("[WEAPONCONTROL] DEFER seq=%u action=%s status=%s mutation=no turn=no\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               EspNativeGameplayWeaponControl_statusName(status));
        return;
    }

    after = before;
    if (after.weapon != result.weaponBefore ||
        result.weaponAfter >= ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT) {
        ++gameplayState.deferred;
        printf("[WEAPONCONTROL] DEFER seq=%u action=%s reason=stale-player-state expected=%u actual=%u mutation=no turn=no\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.weaponBefore,
               (unsigned int)after.weapon);
        return;
    }
    after.weapon = result.weaponAfter;
    if (!EspNativeGameplayPlayerState_restore(&after)) {
        disableGameplay("weapon-control-commit");
        return;
    }

    view = EspPlayerView_view();
    if (view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0 ||
        !renderCurrent(render, (uint8_t)view->viewAngle, "WEAPON-CYCLE")) {
        if (!EspNativeGameplayPlayerState_restore(&before)) {
            disableGameplay("weapon-control-rollback-state");
            return;
        }
        view = EspPlayerView_view();
        if (view == NULL || view->active != 1U ||
            !renderCurrent(render, (uint8_t)view->viewAngle,
                           "WEAPON-CYCLE-ROLLBACK")) {
            disableGameplay("weapon-control-render-rollback");
            return;
        }
        printf("[WEAPONCONTROL] ROLLBACK seq=%u action=%s weapon=%u->%u playerFNV=%08x restored=yes turn=no\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.weaponBefore,
               (unsigned int)result.weaponAfter,
               (unsigned int)result.playerFNVBefore);
        return;
    }

    fnvAfter = EspNativeGameplayPlayerState_fingerprint();
    printf("[WEAPONCONTROL] COMMIT seq=%u action=%s weapon=%u->%u weapons=%04x ammoType=%u ammo=%u inspected=%u playerFNV=%08x->%08x redraw=yes turn=no rollback=closed\n",
           (unsigned int)intent->sequence,
           EspNativeGameplayInput_actionName(intent->action),
           (unsigned int)result.weaponBefore,
           (unsigned int)result.weaponAfter,
           (unsigned int)result.weapons,
           (unsigned int)result.ammoTypeAfter,
           (unsigned int)result.ammoAfter,
           (unsigned int)result.inspected,
           (unsigned int)result.playerFNVBefore,
           (unsigned int)fnvAfter);
}

static void logDeferredSelectEvent(uint16_t eventIndex) {
    uint32_t value;
    EspMapEventRef ref;
    EspMapEventDescriptor descriptor;
    uint32_t offset;

    if (!EspMapRuntime_getEvent(eventIndex, &value)) {
        printf("[ACTIONTRACE] event=%u unavailable\n",
               (unsigned int)eventIndex);
        return;
    }
    ref.index = eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    memset(&descriptor, 0, sizeof(descriptor));
    if (!EspMapEvents_describe(&ref, &descriptor)) {
        printf("[ACTIONTRACE] event=%u tile=%u describe=failed\n",
               (unsigned int)eventIndex,
               (unsigned int)ref.tileIndex);
        return;
    }

    printf("[ACTIONTRACE] event=%u tile=%u commands=%u firstGlobal=%u raw-sequence",
           (unsigned int)eventIndex,
           (unsigned int)descriptor.tileIndex,
           (unsigned int)descriptor.commandCount,
           (unsigned int)descriptor.firstCommandIndex);
    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        EspMapByteCode command;
        if (!EspMapEvents_getCommand(&descriptor, offset, &command)) {
            printf(" off%u=READFAIL", (unsigned int)offset);
            continue;
        }
        printf(" off%u=id%u/a1=%08x/a2=%08x",
               (unsigned int)offset,
               (unsigned int)command.id,
               (unsigned int)command.arg1,
               (unsigned int)command.arg2);
    }
    printf("\n");
}

#define LEGACY_REGULAR_DOOR_FLAG 0x00000004UL
#define LEGACY_SECRET_XP 5U
#define LEGACY_SECRET_SOUND 5133U

static int classifySecretDoorBatch(
    const EspNativeGameplayActionResult* result,
    uint8_t* outFoundSecret) {
    uint8_t i;

    if (outFoundSecret != NULL) *outFoundSecret = 0U;
    if (result == NULL || outFoundSecret == NULL ||
        result->doorCount == 0U ||
        result->doorCount > ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS) {
        return 0;
    }

    for (i = 0U; i < result->doorCount; ++i) {
        EspMapLine line;
        if (!EspMapRuntime_getLine(result->doors[i].lineIndex, &line)) {
            return 0;
        }
        if ((line.flags & LEGACY_REGULAR_DOOR_FLAG) == 0U) {
            *outFoundSecret = 1U;
        }
    }
    return 1;
}

static void serviceSelect(DoomRPG_t* doomRpg,
                          Render_t* render,
                          const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayActionResult result;
    EspNativeGameplayActionStatus status;
    const uint8_t startedInAutomap = automapActive() ? 1U : 0U;

    memset(&result, 0, sizeof(result));
    if (doomRpg == NULL || view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0) {
        disableGameplay("select-unsettled-view");
        return;
    }

    status = EspNativeGameplayAction_executeSelect(intent, &result);
    printf("[ACTION] SELECT seq=%u status=%s tile=%u event=%u eligible=%u unsupported=%u\n",
           (unsigned int)intent->sequence,
           EspNativeGameplayAction_statusName(status),
           (unsigned int)result.frontTile,
           (unsigned int)result.eventIndex,
           (unsigned int)result.eligibleCount,
           (unsigned int)result.unsupportedCodeId);

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_CHAIN_READY) {
        EspNativeGameplayDialogResumeResult chain;
        EspNativeGameplayDialogResumeStatus chainStatus;
        memset(&chain, 0, sizeof(chain));
        chainStatus = EspNativeGameplayEventChain_execute(
            result.eventIndex,
            result.commandOffset,
            ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS,
            &chain);
        if (chainStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
            chainStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) {
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] SELECT-CHAIN-DEFER n=%u seq=%u event=%u cmd=%u opcode=%u status=%s mutation=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent->sequence,
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.commandOffset,
                   (unsigned int)result.codeId,
                   EspNativeGameplayDialog_resumeStatusName(chainStatus));
            return;
        }
        if (!renderActionCurrent(render, (uint8_t)view->viewAngle,
                                 "SELECT-CHAIN")) {
            if (chainStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
                chain.rollbackAvailable != 0U &&
                EspNativeGameplayDialog_rollbackResume(&chain) &&
                renderActionCurrent(render, (uint8_t)view->viewAngle,
                                    "SELECT-CHAIN-ROLLBACK")) {
                printf("[RESIDENTGAMEPLAY] SELECT-CHAIN ROLLBACK seq=%u event=%u restored=yes\n",
                       (unsigned int)intent->sequence,
                       (unsigned int)result.eventIndex);
                return;
            }
            disableGameplay("select-chain-render");
            return;
        }
        ++gameplayState.selects;
        printf("[RESIDENTGAMEPLAY] SELECT-CHAIN n=%u seq=%u event=%u startCmd=%u entryOpcode=%u finalOpcode=%u mutation=%u redraw=yes rollback=closed turnAdvance=deferred\n",
               (unsigned int)gameplayState.selects,
               (unsigned int)intent->sequence,
               (unsigned int)result.eventIndex,
               (unsigned int)result.commandOffset,
               (unsigned int)result.codeId,
               (unsigned int)(chainStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                                  ? chain.codeId : 0U),
               (unsigned int)(chainStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                                  ? chain.mutated : 0U));
        return;
    }

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_PASSWORD_READY) {
        EspNativeGameplayPasswordBeginStatus passwordStatus;

        if (startedInAutomap != 0U &&
            !closeAutomap(render, "AUTOMAP-SELECT-PASSWORD")) {
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] SELECT-PASSWORD-DEFER n=%u seq=%u event=%u cmd=%u status=automap-close-failed mutation=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent->sequence,
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.commandOffset);
            return;
        }

        passwordStatus = EspNativeGameplayPassword_begin(
                result.eventIndex,
                result.commandOffset,
                ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS);
        if (passwordStatus != ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_OK) {
            if (startedInAutomap != 0U) {
                gameplayState.modeFlags =
                    (uint8_t)(gameplayState.modeFlags | RESIDENT_MODE_AUTOMAP);
                if (!renderAutomapCurrent(render, "SELECT-PASSWORD-ROLLBACK")) {
                    disableGameplay("select-password-automap-rollback");
                    return;
                }
            }
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] SELECT-PASSWORD-DEFER n=%u seq=%u event=%u cmd=%u status=%s mutation=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent->sequence,
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.commandOffset,
                   EspNativeGameplayPassword_beginStatusName(passwordStatus));
            return;
        }
        ++gameplayState.selects;
        printf("[RESIDENTGAMEPLAY] SELECT-PASSWORD select=%u seq=%u event=%u cmd=%u active=yes keypad=0-9+DEL+VALID pauseScript=yes skipTurn=yes continuation=preflighted\n",
               (unsigned int)gameplayState.selects,
               (unsigned int)intent->sequence,
               (unsigned int)result.eventIndex,
               (unsigned int)result.commandOffset);
        return;
    }

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_DIALOG_READY) {
        EspNativeGameplayDialogBeginStatus dialogStatus;

        if (startedInAutomap != 0U &&
            !closeAutomap(render, "AUTOMAP-SELECT-DIALOG")) {
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] SELECT-DIALOG-DEFER n=%u seq=%u event=%u cmd=%u status=automap-close-failed mutation=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent->sequence,
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.commandOffset);
            return;
        }

        dialogStatus = EspNativeGameplayDialog_begin(
                result.eventIndex,
                result.commandOffset,
                ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS);
        if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            if (startedInAutomap != 0U) {
                gameplayState.modeFlags =
                    (uint8_t)(gameplayState.modeFlags | RESIDENT_MODE_AUTOMAP);
                if (!renderAutomapCurrent(render, "SELECT-DIALOG-ROLLBACK")) {
                    disableGameplay("select-dialog-automap-rollback");
                    return;
                }
            }
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] SELECT-DIALOG-DEFER n=%u seq=%u event=%u cmd=%u status=%s mutation=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent->sequence,
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.commandOffset,
                   EspNativeGameplayDialog_beginStatusName(dialogStatus));
            return;
        }
        ++gameplayState.selects;
        ++gameplayState.dialogs;
        printf("[RESIDENTGAMEPLAY] SELECT-DIALOG n=%u seq=%u event=%u cmd=%u active=yes pauseScript=yes skipTurn=yes continuation=preflighted worldMutation=no\n",
               (unsigned int)gameplayState.dialogs,
               (unsigned int)intent->sequence,
               (unsigned int)result.eventIndex,
               (unsigned int)result.commandOffset);
        return;
    }

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK) {
        EspNativeGameplayPlayerState playerBefore;
        EspNativeGameplayPlayerXpResult secretXp;
        Random_t randomBefore;
        uint8_t foundSecret = 0U;
        uint8_t playerCaptured = 0U;
        uint8_t secretFeedbackQueued = 0U;

        memset(&playerBefore, 0, sizeof(playerBefore));
        memset(&secretXp, 0, sizeof(secretXp));

        if (!classifySecretDoorBatch(&result, &foundSecret)) {
            if (!EspNativeGameplayAction_rollbackSelect(&result)) {
                disableGameplay("select-door-secret-classify-rollback");
                return;
            }
            ++gameplayState.deferred;
            printf("[SECRET] DEFER event=%u doors=%u reason=line-classification mutation=rolled-back\n",
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.doorCount);
            return;
        }

        if (foundSecret != 0U) {
            if (!EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
                if (!EspNativeGameplayAction_rollbackSelect(&result)) {
                    disableGameplay("select-door-secret-player-snapshot-rollback");
                    return;
                }
                ++gameplayState.deferred;
                printf("[SECRET] DEFER event=%u doors=%u reason=player-snapshot mutation=rolled-back\n",
                       (unsigned int)result.eventIndex,
                       (unsigned int)result.doorCount);
                return;
            }
            playerCaptured = 1U;
            randomBefore = doomRpg->random;

            if (!EspNativeGameplayPlayerState_applyXp(
                    doomRpg, LEGACY_SECRET_XP, &secretXp) ||
                !EspNativeGameplayActionEngine_queueTextFeedback(
                    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT,
                    "Found Secret!", 0U)) {
                doomRpg->random = randomBefore;
                if (!EspNativeGameplayPlayerState_restore(&playerBefore) ||
                    !EspNativeGameplayAction_rollbackSelect(&result)) {
                    disableGameplay("select-door-secret-reward-rollback");
                    return;
                }
                ++gameplayState.deferred;
                printf("[SECRET] DEFER event=%u doors=%u reason=reward-owner mutation=rolled-back\n",
                       (unsigned int)result.eventIndex,
                       (unsigned int)result.doorCount);
                return;
            }
            secretFeedbackQueued = 1U;
        }

        if (!renderActionCurrent(render, (uint8_t)view->viewAngle,
                                 "SELECT-DOOR")) {
            int feedbackRestored = 1;
            int playerRestored = 1;
            if (secretFeedbackQueued != 0U) {
                feedbackRestored =
                    EspNativeGameplayActionEngine_cancelQueuedFeedback(
                        ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT);
            }
            if (playerCaptured != 0U) {
                doomRpg->random = randomBefore;
                playerRestored =
                    EspNativeGameplayPlayerState_restore(&playerBefore);
            }
            if (!feedbackRestored || !playerRestored ||
                !EspNativeGameplayAction_rollbackSelect(&result) ||
                !renderActionCurrent(render, (uint8_t)view->viewAngle,
                                     "SELECT-DOOR-ROLLBACK")) {
                disableGameplay("select-door-render-rollback");
                return;
            }
            printf("[RESIDENTGAMEPLAY] SELECT ROLLBACK seq=%u doors=%u firstLine=%u secret=%u xpRestored=%s rngRestored=%s restored=yes\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)result.doorCount,
                   (unsigned int)result.lineIndex,
                   (unsigned int)foundSecret,
                   foundSecret ? "yes" : "n/a",
                   foundSecret ? "yes" : "n/a");
            return;
        }

        ++gameplayState.selects;
        {
            uint8_t doorIndex;
            printf("[ACTION] DOOR-BATCH event=%u count=%u status=OK",
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.doorCount);
            for (doorIndex = 0U; doorIndex < result.doorCount; ++doorIndex) {
                const EspNativeGameplayActionDoorStep* step =
                    &result.doors[doorIndex];
                printf(" [%u]line=%u/op=%u/open=%u->%u/removed=%u->%u",
                       (unsigned int)doorIndex,
                       (unsigned int)step->lineIndex,
                       (unsigned int)step->codeId,
                       (unsigned int)step->openBefore,
                       (unsigned int)step->openAfter,
                       (unsigned int)step->removedBefore,
                       (unsigned int)step->removedAfter);
            }
            printf("\n");
        }
        if (foundSecret != 0U) {
            printf("[SECRET] FOUND event=%u doors=%u xp=%u level=%u->%u levelUps=%u rngCalls=%u playerFNV=%08x->%08x message=\"Found Secret!\" sound=%u-deferred commit=yes\n",
                   (unsigned int)result.eventIndex,
                   (unsigned int)result.doorCount,
                   (unsigned int)secretXp.xpApplied,
                   (unsigned int)secretXp.levelBefore,
                   (unsigned int)secretXp.levelAfter,
                   (unsigned int)secretXp.levelUps,
                   (unsigned int)secretXp.rngCalls,
                   (unsigned int)secretXp.stateFNVBefore,
                   (unsigned int)secretXp.stateFNVAfter,
                   (unsigned int)LEGACY_SECRET_SOUND);
        }
        printf("[RESIDENTGAMEPLAY] SELECT n=%u seq=%u doors=%u firstDoor=%u committed=yes redraw=yes collision=live animation=bounded-batch secret=%s sound=%s entityRelink=deferred turnAdvance=deferred\n",
               (unsigned int)gameplayState.selects,
               (unsigned int)intent->sequence,
               (unsigned int)result.doorCount,
               (unsigned int)result.lineIndex,
               foundSecret ? "found+5xp" : "no",
               foundSecret ? "5133-deferred" : "door-deferred");
        return;
    }

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED ||
        status == ESP_NATIVE_GAMEPLAY_ACTION_DOOR_ALREADY_TARGET) {
        ++gameplayState.selectRefused;
        printf("[ACTION] DOOR line=%u opcode=%u status=%s open=%u->%u locked=%u mutation=no broadFallback=deferred\n",
               (unsigned int)result.lineIndex,
               (unsigned int)result.codeId,
               EspNativeGameplayAction_statusName(status),
               (unsigned int)result.openBefore,
               (unsigned int)result.openAfter,
               (unsigned int)result.locked);
        printf("[RESIDENTGAMEPLAY] SELECT-REFUSED n=%u seq=%u status=%s worldStable=yes turnAdvance=no\n",
               (unsigned int)gameplayState.selectRefused,
               (unsigned int)intent->sequence,
               EspNativeGameplayAction_statusName(status));
        return;
    }

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT ||
        status == ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE ||
        status == ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT ||
        status == ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT) {
        if (status == ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT ||
            status == ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT) {
            logDeferredSelectEvent(result.eventIndex);
        }
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] SELECT-DEFER n=%u seq=%u status=%s unsupported=%u entity/otherSemantics=deferred mutation=no\n",
               (unsigned int)gameplayState.deferred,
               (unsigned int)intent->sequence,
               EspNativeGameplayAction_statusName(status),
               (unsigned int)result.unsupportedCodeId);
        return;
    }

    disableGameplay(EspNativeGameplayAction_statusName(status));
}

static void serviceDialogAction(Render_t* render,
                                const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayDialogClose close;
    EspNativeGameplayDialogResumeResult resume;
    EspNativeGameplayDialogInputStatus inputStatus;
    EspNativeGameplayDialogResumeStatus resumeStatus;

    memset(&close, 0, sizeof(close));
    memset(&resume, 0, sizeof(resume));
    if (render == NULL || intent == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != view->destAngle ||
        (view->viewAngle & 63) != 0 ||
        !EspNativeGameplayDialog_isActive()) {
        disableGameplay("dialog-action-context");
        return;
    }

    inputStatus = EspNativeGameplayDialog_handleAction(intent->action, &close);
    if (inputStatus == ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID) {
        disableGameplay("dialog-input");
        return;
    }
    if (inputStatus == ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_IGNORED) {
        printf("[RESIDENTGAMEPLAY] DIALOG-IGNORE seq=%u action=%s active=yes\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action));
        return;
    }
    if (inputStatus == ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN) {
        printf("[RESIDENTGAMEPLAY] DIALOG-INPUT seq=%u action=%s active=yes redraw=dialog-only\n",
               (unsigned int)intent->sequence,
               EspNativeGameplayInput_actionName(intent->action));
        return;
    }

    if (inputStatus == ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_CANCEL) {
        if (!renderCurrent(render, (uint8_t)view->viewAngle, "DIALOG-CANCEL")) {
            disableGameplay("dialog-cancel-render");
            return;
        }
        ++gameplayState.dialogCancels;
        printf("[RESIDENTGAMEPLAY] DIALOG-CANCEL n=%u seq=%u event=%u resume=no stateMutation=no turnAdvance=no\n",
               (unsigned int)gameplayState.dialogCancels,
               (unsigned int)intent->sequence,
               (unsigned int)close.sourceEventIndex);
        return;
    }

    if (inputStatus != ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_RESUME) {
        disableGameplay("dialog-input-status");
        return;
    }

    resumeStatus = EspNativeGameplayDialog_resume(&close, &resume);
    if (resumeStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
        resumeStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) {
        printf("[RESIDENTGAMEPLAY] DIALOG-RESUME-FAILED seq=%u event=%u offset=%u status=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)close.sourceEventIndex,
               (unsigned int)close.resumeCommandOffset,
               EspNativeGameplayDialog_resumeStatusName(resumeStatus));
        disableGameplay("dialog-resume");
        return;
    }

    if (!renderCurrent(render, (uint8_t)view->viewAngle, "DIALOG-RESUME")) {
        if (resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
            resume.rollbackAvailable != 0U &&
            EspNativeGameplayDialog_rollbackResume(&resume) &&
            renderCurrent(render, (uint8_t)view->viewAngle,
                          "DIALOG-RESUME-ROLLBACK")) {
            printf("[RESIDENTGAMEPLAY] DIALOG ROLLBACK seq=%u event=%u opcode=%u restored=yes\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)close.sourceEventIndex,
                   (unsigned int)resume.codeId);
            return;
        }
        disableGameplay("dialog-resume-render-rollback");
        return;
    }

    ++gameplayState.dialogResumes;
    printf("[RESIDENTGAMEPLAY] DIALOG-RESUME n=%u seq=%u event=%u offset=%u opcode=%u stateMutation=%u redraw=yes turnAdvance=deferred dialog=closed\n",
           (unsigned int)gameplayState.dialogResumes,
           (unsigned int)intent->sequence,
           (unsigned int)close.sourceEventIndex,
           (unsigned int)close.resumeCommandOffset,
           (unsigned int)(resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                              ? resume.codeId
                              : 0U),
           (unsigned int)(resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                              ? resume.mutated
                              : 0U));
}

static void servicePasswordCompletion(
    Render_t* render,
    const EspNativeGameplayPasswordCompletion* completion) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayHudState* hud;
    EspNativeGameplayHudStats hudStats;
    EspNativeGameplayHudStatus hudStatus;
    EspNativeGameplayDialogResumeResult resume;
    EspNativeGameplayDialogResumeStatus resumeStatus =
        ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND;
    EspNativeGameplayDialogBeginStatus dialogStatus =
        ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    const int resumeIntoDialog =
        completion != NULL && completion->correct != 0U &&
        completion->close.resumeHasCommand != 0U &&
        (completion->close.resumeCodeId == ESP_MAP_OPCODE_DIALOG ||
         completion->close.resumeCodeId == ESP_MAP_OPCODE_DIALOG_NO_BACK) &&
        completion->resumeDialogOffset != UINT8_MAX;
    int feedbackQueued = 0;

    memset(&hudStats, 0, sizeof(hudStats));
    memset(&resume, 0, sizeof(resume));
    if (render == NULL || completion == NULL || completion->pending != 1U ||
        view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0) {
        disableGameplay("password-completion-context");
        return;
    }

    /*
     * The keypad is intentionally a full-screen modal and therefore overwrites
     * both 20-row HUD bands. The gameplay frame compositor preserves those bands
     * by design, so reconstruct them from the wrapped/current native HUD model
     * before any world redraw. This is presentation-only: no dirty intent is
     * consumed and no HUD/player owner is mutated.
     */
    hud = EspNativeGameplayHud_view();
    if (hud == NULL) {
        disableGameplay("password-hud-view");
        return;
    }
    hudStatus = EspNativeGameplayHud_repaint(hud, &hudStats);
    if (hudStatus != ESP_NATIVE_GAMEPLAY_HUD_OK) {
        printf("[RESIDENTGAMEPLAY] PASSWORD-HUD-RESTORE-FAILED event=%u status=%d mutation=no\n",
               (unsigned int)completion->close.sourceEventIndex,
               (int)hudStatus);
        disableGameplay("password-hud-repaint");
        return;
    }
    printf("[RESIDENTGAMEPLAY] PASSWORD-HUD-RESTORE event=%u pixels=%u reads=%u bytes=%u exactSource=current-native-model dirtyConsume=no\n",
           (unsigned int)completion->close.sourceEventIndex,
           (unsigned int)hudStats.pixelsWritten,
           (unsigned int)hudStats.packReads,
           (unsigned int)hudStats.bytesRead);

    if (completion->correct != 0U) {
        feedbackQueued = EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT,
            "Correct code!", 0U);

        if (!resumeIntoDialog) {
            resumeStatus =
                EspNativeGameplayDialog_resume(&completion->close, &resume);
            if (resumeStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
                resumeStatus != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) {
                printf("[RESIDENTGAMEPLAY] PASSWORD-RESUME-FAILED event=%u offset=%u status=%s\n",
                       (unsigned int)completion->close.sourceEventIndex,
                       (unsigned int)completion->close.resumeCommandOffset,
                       EspNativeGameplayDialog_resumeStatusName(resumeStatus));
                disableGameplay("password-resume");
                return;
            }
        }
    }
    else if (completion->hadInput != 0U) {
        feedbackQueued = EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT,
            "Invalid code!", 0U);
    }

    if (!renderCurrent(render, (uint8_t)view->viewAngle,
                       completion->correct != 0U
                           ? "PASSWORD-CORRECT"
                           : "PASSWORD-INVALID")) {
        if (completion->correct != 0U && !resumeIntoDialog &&
            resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
            resume.rollbackAvailable != 0U &&
            EspNativeGameplayDialog_rollbackResume(&resume) &&
            renderCurrent(render, (uint8_t)view->viewAngle,
                          "PASSWORD-RESUME-ROLLBACK")) {
            printf("[RESIDENTGAMEPLAY] PASSWORD ROLLBACK event=%u opcode=%u restored=yes\n",
                   (unsigned int)completion->close.sourceEventIndex,
                   (unsigned int)resume.codeId);
            return;
        }
        disableGameplay("password-render-rollback");
        return;
    }

    if (resumeIntoDialog) {
        dialogStatus = EspNativeGameplayEventChain_beginDialogCommand(
            completion->close.sourceEventIndex,
            completion->resumeDialogOffset,
            completion->close.runFlags);
        if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            printf("[RESIDENTGAMEPLAY] PASSWORD-DIALOG-DEFER event=%u cmd=%u opcode=%u status=%s worldRedrawn=yes\n",
                   (unsigned int)completion->close.sourceEventIndex,
                   (unsigned int)completion->resumeDialogOffset,
                   (unsigned int)completion->close.resumeCodeId,
                   EspNativeGameplayDialog_beginStatusName(dialogStatus));
            disableGameplay("password-dialog-open");
            return;
        }

        ++gameplayState.dialogs;
        printf("[RESIDENTGAMEPLAY] PASSWORD-CLOSE event=%u entered=%u/%u result=correct continuation=dialog-open opcode=%u cmd=%u mutation=0 redraw=yes message=%s dialogActive=yes back=%s turnAdvance=deferred\n",
               (unsigned int)completion->close.sourceEventIndex,
               (unsigned int)completion->enteredLength,
               (unsigned int)completion->expectedLength,
               (unsigned int)completion->close.resumeCodeId,
               (unsigned int)completion->resumeDialogOffset,
               feedbackQueued ? "queued" : "none",
               completion->close.resumeCodeId == ESP_MAP_OPCODE_DIALOG
                   ? "yes" : "no");
        return;
    }

    printf("[RESIDENTGAMEPLAY] PASSWORD-CLOSE event=%u entered=%u/%u result=%s continuation=%s opcode=%u mutation=%u redraw=yes message=%s turnAdvance=deferred\n",
           (unsigned int)completion->close.sourceEventIndex,
           (unsigned int)completion->enteredLength,
           (unsigned int)completion->expectedLength,
           completion->correct != 0U ? "correct" : "invalid",
           completion->correct != 0U
               ? (resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                      ? "executed"
                      : "empty")
               : "blocked",
           (unsigned int)(resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                              ? resume.codeId
                              : 0U),
           (unsigned int)(resumeStatus == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK
                              ? resume.mutated
                              : 0U),
           feedbackQueued ? "queued" : "none");
}

static int publishCurrentAutomapVisibility(Render_t* render,
                                           const char* reason) {
    EspNativeBspVisibilityState* visibility;
    uint16_t linesMutated = 0U;
    uint16_t spritesMutated = 0U;
    int ok;

    if (render == NULL) return 0;
    visibility = (EspNativeBspVisibilityState*)SDL_malloc(sizeof(*visibility));
    if (visibility == NULL) {
        printf("[AUTOMAPVIS] FAILED source=automap reason=owner-allocation bytes=%u\n",
               (unsigned int)sizeof(*visibility));
        return 0;
    }
    memset(visibility, 0, sizeof(*visibility));
    ok = EspNativeBspVisibility_build(render, visibility) &&
         EspNativeBspVisibility_publishAutomap(
             visibility, &linesMutated, &spritesMutated);
    SDL_free(visibility);
    if (!ok) {
        printf("[AUTOMAPVIS] FAILED source=automap reason=%s\n",
               reason != NULL ? reason : "frame");
        return 0;
    }
    if (linesMutated != 0U || spritesMutated != 0U) {
        printf("[AUTOMAPVIS] PUBLISH source=automap reason=%s lines+=%u sprites+=%u owner=transient-no-framebuffer\n",
               reason != NULL ? reason : "frame",
               (unsigned int)linesMutated,
               (unsigned int)spritesMutated);
    }
    return 1;
}

static int renderAutomapCurrent(Render_t* render, const char* reason) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayAutomapStats stats;
    uint16_t uncovered = 0U;
    int ok = 0;

    memset(&stats, 0, sizeof(stats));
    if (view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle ||
        (view->viewAngle & 63) != 0 ||
        !EspNativeGameplayAutomap_uncoverAt(
            view->destX, view->destY, &uncovered) ||
        !publishCurrentAutomapVisibility(render, reason)) {
        return 0;
    }

    if (EspNativeDoorAnimator_hasPendingFrames()) {
        const EspNativeDoorAnimatorView* before = EspNativeDoorAnimator_view();
        uint32_t completedBefore =
            before != NULL ? before->completedTransitions : 0U;

        if (!EspNativeDoorAnimator_validateLineState()) {
            printf("[DOORANIM] AUTOMAP lease-canceled-before-render; presenting stable state\n");
        }

        while (EspNativeDoorAnimator_hasPendingFrames()) {
            EspNativeDoorAnimationFrame animationFrame;
            memset(&animationFrame, 0, sizeof(animationFrame));
            memset(&stats, 0, sizeof(stats));
            if (!EspNativeDoorAnimator_prepareFrame(&animationFrame)) {
                EspNativeDoorAnimator_reset();
                EspNativeGameplayMoveEvents_onFrameResult(0);
                printf("[DOORANIM] AUTOMAP FAILED reason=prepare-frame\n");
                return 0;
            }
            ok = EspNativeGameplayAutomap_render(
                view->viewX, view->viewY, (uint8_t)view->viewAngle, &stats);
            if (!EspNativeDoorAnimator_finishFrame(ok)) {
                EspNativeDoorAnimator_reset();
                EspNativeGameplayMoveEvents_onFrameResult(0);
                printf("[DOORANIM] AUTOMAP FAILED reason=finish-frame\n");
                return 0;
            }
            printf("[DOORANIM] AUTOMAP-FRAME %u/%u angle=%u lines=%u geometry=%s frame=%08x render=%s\n",
                   (unsigned int)animationFrame.ordinal,
                   (unsigned int)animationFrame.totalFrames,
                   (unsigned int)view->viewAngle,
                   (unsigned int)animationFrame.activeLines,
                   animationFrame.geometryActive != 0U ? "moving" : "stable",
                   (unsigned int)stats.frameFNV1a,
                   ok ? "ok" : "failed");
            if (!ok) {
                EspNativeGameplayMoveEvents_onFrameResult(0);
                EspNativeDoorAnimator_reset();
                return 0;
            }
        }
        EspNativeGameplayMoveEvents_onFrameResult(1);
        {
            const EspNativeDoorAnimatorView* after = EspNativeDoorAnimator_view();
            uint32_t completedAfter =
                after != NULL ? after->completedTransitions : completedBefore;
            printf("[DOORANIM] AUTOMAP-COMPLETE transitions=%u frames=%u state=stable transaction=committed\n",
                   (unsigned int)(completedAfter - completedBefore),
                   (unsigned int)ESP_NATIVE_DOOR_ANIMATION_FRAMES);
        }
    }
    else {
        ok = EspNativeGameplayAutomap_render(
            view->viewX, view->viewY, (uint8_t)view->viewAngle, &stats);
        EspNativeGameplayMoveEvents_onFrameResult(ok);
        if (!ok) return 0;
    }

    printf("[RESIDENTGAMEPLAY] AUTOMAP-FRAME reason=%s uncover=%u lines=%u visited=%u player=%u,%u frame=%08x turnAdvance=semantic\n",
           reason != NULL ? reason : "AUTOMAP",
           (unsigned int)uncovered,
           (unsigned int)stats.revealedLines,
           (unsigned int)stats.visitedCells,
           (unsigned int)stats.playerX,
           (unsigned int)stats.playerY,
           (unsigned int)stats.frameFNV1a);
    return 1;
}

static int closeAutomap(Render_t* render, const char* reason) {
    const EspNativeGameplayHudState* hud = EspNativeGameplayHud_view();
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayHudStats hudStats;

    memset(&hudStats, 0, sizeof(hudStats));
    if (render == NULL || hud == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != view->destAngle ||
        (view->viewAngle & 63) != 0 ||
        EspNativeGameplayHud_repaint(hud, &hudStats) !=
            ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !renderCurrent(render, (uint8_t)view->viewAngle,
                       reason != NULL ? reason : "AUTOMAP-CLOSE")) {
        return 0;
    }
    gameplayState.modeFlags =
        (uint8_t)(gameplayState.modeFlags & (uint8_t)~RESIDENT_MODE_AUTOMAP);
    printf("[RESIDENTGAMEPLAY] AUTOMAP-CLOSE hudPixels=%u worldRedraw=yes mode=world turnAdvance=no\n",
           (unsigned int)hudStats.pixelsWritten);
    return 1;
}

static int restoreWorldAfterHub(Render_t* render, const char* reason) {
    const EspPlayerViewState* view = EspPlayerView_view();
    if (render == NULL || view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0) {
        return 0;
    }
    return renderCurrent(render, (uint8_t)view->viewAngle, reason);
}

void EspNativeResidentGameplay_reset(void) {
    PlatformInput_setTapCallback(NULL);
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(0, NULL);
    }
    EspNativeGameplayHub_reset();
    EspNativeGameplayDialog_reset();
    EspNativeGameplayPassword_reset();
    EspNativeGameplayControls_reset();
    EspNativeGameplayInput_reset();
    memset(&gameplayState, 0, sizeof(gameplayState));
}

int EspNativeResidentGameplay_armCheckpointResume(void) {
    if (gameplayState.active != 0U || gameplayState.failed != 0U ||
        gameplayState.checkpointResumeArmed != 0U) {
        return 0;
    }
    gameplayState.checkpointResumeArmed = 1U;
    return 1;
}

int EspNativeResidentGameplay_isActive(void) {
    return gameplayState.active != 0U && gameplayState.failed == 0U;
}

int EspNativeResidentGameplay_isAutomapActive(void) {
    return EspNativeResidentGameplay_isActive() && automapActive();
}

int EspNativeResidentGameplay_redrawAutomap(
    struct Render_s* render,
    const char* reason) {
    if (!EspNativeResidentGameplay_isAutomapActive() || render == NULL) return 0;
    return renderAutomapCurrent((Render_t*)render, reason);
}

int EspNativeResidentGameplay_exitAutomapForDamage(
    struct Render_s* render,
    const char* reason) {
    if (!EspNativeResidentGameplay_isAutomapActive()) return 1;
    printf("[RESIDENTGAMEPLAY] AUTOMAP-DAMAGE-EXIT reason=%s legacyPlayerPain=yes\n",
           reason != NULL ? reason : "damage");
    return closeAutomap((Render_t*)render,
                        reason != NULL ? reason : "AUTOMAP-DAMAGE");
}

void EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeGameplayInputState intent;
    EspNativeGameplayInputStatus inputStatus;
    EspNativeGameplayDispatchStatus dispatchStatus;
    EspNativeGameplayControlsStats feedbackStats;
    const EspNativeGameplayInputState* pending;

    if (gameplayState.failed) return;

    if (!gameplayState.active) {
        const int checkpointResume =
            gameplayState.checkpointResumeArmed != 0U;
        if (doomRpg == NULL || doomRpg->render == NULL ||
            !EspNativeGameplayHud_isReady() ||
            (!EspNativeFirstFrame_isReady() && !checkpointResume) ||
            (checkpointResume &&
             (!EspAssetPack_isResident() ||
              !EspAssetPack_isResidentLargeRangeEnabled() ||
              EspAssetPack_isOpen()))) {
            return;
        }
        if (!EspNativeGameplayDispatch_isReady()) {
            dispatchStatus = EspNativeGameplayDispatch_adoptView();
            if (dispatchStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_OK &&
                dispatchStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_ALREADY_ACTIVE) {
                printf("[RESIDENTGAMEPLAY] WAIT dispatch adopt status=%d\n",
                       (int)dispatchStatus);
                return;
            }
        }
        if (!ensureCollisionCatalog()) {
            printf("[RESIDENTGAMEPLAY] WAIT collision catalog entities.db\n");
            return;
        }

        EspNativeGameplayHub_reset();
        EspNativeGameplayDialog_reset();
        EspNativeGameplayPassword_reset();
        EspNativeGameplayControls_reset();
        EspNativeGameplayInput_reset();
        {
            const uint8_t resumed = gameplayState.checkpointResumeArmed;
            const EspPlayerViewState* view = EspPlayerView_view();
            uint16_t uncovered = 0U;
            if (view == NULL ||
                !EspNativeGameplayAutomap_uncoverAt(
                    view->destX, view->destY, &uncovered)) {
                printf("[RESIDENTGAMEPLAY] WAIT automap initial uncover\n");
                return;
            }
            gameplayState.checkpointResumeArmed = 0U;
            gameplayState.active = 1U;
            PlatformInput_setTapCallback(onGameplayTap);
            printf("[AUTOMAP] UNCOVER reason=SESSION-ARM mutated=%u state=ready\n",
                   (unsigned int)uncovered);
            printf("\n=== Doom RPG ESP32-native resident gameplay service ===\n");
            printf("[RESIDENTGAMEPLAY] READY map=current entry=%s touch=invisible-12-zone+120ms-feedback dispatch=TURN+MOVE+SELECT_DOOR6/15/16/17+SELECT_DIALOG8/26+PASSWORD10+PASS_TURN+MENU_HUB collision=native/entityDefs=%u moveEvents=door15/16+force24+enter-dialog8/26-live-other-deferred doorAnimation=regular4frame-live password=touch-keypad-0-9+DEL+VALID menu=inventory-weapon-select-no-turn SELECT-entity/other=deferred AUTOMAP=move+turn+select-live/other-actions-deferred PASS_TURN-message=topbar-live+type10/11-touch=deferred\n",
                   resumed != 0U ? "checkpoint-resume" : "fresh-first-frame",
                   (unsigned int)EspEntityDefTypeCatalog_definitionCount());
        }
        return;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        disableGameplay("missing-render");
        return;
    }

    if (EspNativeGameplayControls_isActive()) {
        if (!EspNativeGameplayControls_isExpired()) return;
        memset(&feedbackStats, 0, sizeof(feedbackStats));
        if (!EspNativeGameplayControls_restore(1, &feedbackStats)) {
            disableGameplay("touch-feedback-restore");
            return;
        }
        printf("[TOUCHFEEDBACK] RESTORE zone=%u action=%s edits=%u frame=%08x exact=yes idle=invisible\n",
               (unsigned int)feedbackStats.zone,
               EspNativeGameplayInput_actionName(feedbackStats.action),
               (unsigned int)feedbackStats.edits,
               (unsigned int)feedbackStats.baselineFNV);
    }

    pending = EspNativeGameplayInput_peek();

    if ((EspNativeGameplayHub_isActive() &&
         (EspNativeGameplayDialog_isActive() ||
          EspNativeGameplayPassword_isActive() ||
          EspNativeGameplayPassword_hasPendingCompletion() ||
          automapActive())) ||
        (automapActive() &&
         (EspNativeGameplayDialog_isActive() ||
          EspNativeGameplayPassword_isActive() ||
          EspNativeGameplayPassword_hasPendingCompletion())) ||
        (EspNativeGameplayDialog_isActive() &&
         (EspNativeGameplayPassword_isActive() ||
          EspNativeGameplayPassword_hasPendingCompletion()))) {
        disableGameplay("modal-overlap");
        return;
    }

    if (EspNativeGameplayPassword_hasPendingCompletion()) {
        EspNativeGameplayPasswordCompletion completion;
        memset(&completion, 0, sizeof(completion));
        if (!EspNativeGameplayPassword_takeCompletion(&completion)) {
            disableGameplay("password-completion-consume");
            return;
        }
        servicePasswordCompletion(doomRpg->render, &completion);
        return;
    }
    if (EspNativeGameplayPassword_isActive()) {
        return;
    }

    /*
     * Dialog input wins over the time-based typewriter after the 120-ms touch
     * feedback restores its exact framebuffer baseline. This preserves the
     * semantic state visible when the player tapped SELECT instead of letting
     * the typewriter advance first and accidentally turn a fast-forward into a
     * close/resume. With no pending input the typewriter continues normally.
     */
    if (EspNativeGameplayDialog_isActive()) {
        if (pending != NULL && pending->pending != 0U) {
            memset(&intent, 0, sizeof(intent));
            inputStatus = EspNativeGameplayInput_consume(&intent);
            if (inputStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK) {
                disableGameplay("dialog-input-consume");
                return;
            }
            ++gameplayState.actions;
            if (intent.action == ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN) {
                printf("[RESIDENTGAMEPLAY] DIALOG-IGNORE seq=%u action=PASS_TURN reason=legacy-key14-not-dialog-select\n",
                       (unsigned int)intent.sequence);
                return;
            }
            serviceDialogAction(doomRpg->render, &intent);
            return;
        }
        if (!EspNativeGameplayDialog_tick()) {
            disableGameplay("dialog-tick");
        }
        return;
    }

    if (automapActive()) {
        if (pending == NULL || pending->pending == 0U) return;
        memset(&intent, 0, sizeof(intent));
        inputStatus = EspNativeGameplayInput_consume(&intent);
        if (inputStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK) {
            disableGameplay("automap-input-consume");
            return;
        }
        ++gameplayState.actions;

        switch (intent.action) {
        case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
            if (!closeAutomap(doomRpg->render, "AUTOMAP-CLOSE")) {
                disableGameplay("automap-close-render");
            }
            return;

        case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
        case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
            serviceTurn(doomRpg->render, &intent);
            return;

        case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
        case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
        case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
        case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
            serviceMove(doomRpg->render, &intent);
            return;

        case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
            serviceSelect(doomRpg, doomRpg->render, &intent);
            return;

        default:
            printf("[RESIDENTGAMEPLAY] AUTOMAP-IGNORE seq=%u action=%s phase=move+turn+select-live otherLegacyActions=deferred mutation=no turnAdvance=no\n",
                   (unsigned int)intent.sequence,
                   EspNativeGameplayInput_actionName(intent.action));
            return;
        }
    }

    /* HUB owns the input domain while active. No world action is allowed to
     * fall through this branch, including PASS_TURN, SELECT or weapon cycling. */
    if (EspNativeGameplayHub_isActive()) {
        EspNativeGameplayHubStatus hubStatus;
        if (pending == NULL || pending->pending == 0U) return;
        memset(&intent, 0, sizeof(intent));
        inputStatus = EspNativeGameplayInput_consume(&intent);
        if (inputStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK) {
            disableGameplay("hub-input-consume");
            return;
        }
        ++gameplayState.actions;
        hubStatus = EspNativeGameplayHub_handleAction(intent.action);
        if (hubStatus == ESP_NATIVE_GAMEPLAY_HUB_CLOSED) {
            if (!restoreWorldAfterHub(doomRpg->render, "HUB-CLOSE")) {
                disableGameplay("hub-close-world-render");
                return;
            }
            printf("[RESIDENTGAMEPLAY] HUB-CLOSE seq=%u action=%s worldRedraw=yes closeMutation=no retainedPlayerMutation=weapon-only-possible turnAdvance=no packClosed=yes\n",
                   (unsigned int)intent.sequence,
                   EspNativeGameplayInput_actionName(intent.action));
            return;
        }
        if (hubStatus == ESP_NATIVE_GAMEPLAY_HUB_REDRAWN ||
            hubStatus == ESP_NATIVE_GAMEPLAY_HUB_IGNORED ||
            hubStatus == ESP_NATIVE_GAMEPLAY_HUB_OK) {
            printf("[RESIDENTGAMEPLAY] HUB-INPUT seq=%u action=%s status=%s worldDispatch=blocked turnAdvance=no\n",
                   (unsigned int)intent.sequence,
                   EspNativeGameplayInput_actionName(intent.action),
                   EspNativeGameplayHub_statusName(hubStatus));
            return;
        }

        ++gameplayState.deferred;
        EspNativeGameplayHub_reset();
        if (!restoreWorldAfterHub(doomRpg->render, "HUB-RECOVER")) {
            disableGameplay("hub-error-world-render");
            return;
        }
        printf("[RESIDENTGAMEPLAY] HUB-RECOVER n=%u seq=%u action=%s status=%s hubClosed=yes worldRedraw=yes recoveryMutation=no retainedPlayerMutation=weapon-only-possible turnAdvance=no\n",
               (unsigned int)gameplayState.deferred,
               (unsigned int)intent.sequence,
               EspNativeGameplayInput_actionName(intent.action),
               EspNativeGameplayHub_statusName(hubStatus));
        return;
    }

    if (pending == NULL || pending->pending == 0U) return;

    memset(&intent, 0, sizeof(intent));
    inputStatus = EspNativeGameplayInput_consume(&intent);
    if (inputStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK) {
        disableGameplay("input-consume");
        return;
    }
    ++gameplayState.actions;

    switch (intent.action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
        serviceTurn(doomRpg->render, &intent);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
        serviceMove(doomRpg->render, &intent);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
        serviceSelect(doomRpg, doomRpg->render, &intent);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:
        (void)EspNativeGameplayPassTurn_execute(&intent);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON:
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON:
        serviceWeaponControl(doomRpg->render, &intent);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
        gameplayState.modeFlags =
            (uint8_t)(gameplayState.modeFlags | RESIDENT_MODE_AUTOMAP);
        if (!renderAutomapCurrent(doomRpg->render, "OPEN")) {
            gameplayState.modeFlags =
                (uint8_t)(gameplayState.modeFlags &
                          (uint8_t)~RESIDENT_MODE_AUTOMAP);
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] AUTOMAP-OPEN-DEFER n=%u seq=%u mutation=visit-only-possible turnAdvance=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent.sequence);
            return;
        }
        printf("[RESIDENTGAMEPLAY] AUTOMAP-OPEN seq=%u mode=view-only worldDispatch=blocked turnAdvance=no fullScreen=yes\n",
               (unsigned int)intent.sequence);
        break;

    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN: {
        EspNativeGameplayHubStatus hubStatus = EspNativeGameplayHub_open();
        if (hubStatus == ESP_NATIVE_GAMEPLAY_HUB_OK) {
            printf("[RESIDENTGAMEPLAY] HUB-OPEN seq=%u page=inventory-weapon-select openMutation=no turnAdvance=no worldDispatch=blocked packClosed=yes\n",
                   (unsigned int)intent.sequence);
        }
        else {
            ++gameplayState.deferred;
            printf("[RESIDENTGAMEPLAY] HUB-OPEN-DEFER n=%u seq=%u status=%s mutation=no turnAdvance=no\n",
                   (unsigned int)gameplayState.deferred,
                   (unsigned int)intent.sequence,
                   EspNativeGameplayHub_statusName(hubStatus));
        }
        break;
    }

    default:
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] DEFER n=%u action=%s id=%u semantic-not-enabled\n",
               (unsigned int)gameplayState.deferred,
               EspNativeGameplayInput_actionName(intent.action),
               (unsigned int)intent.action);
        break;
    }
}
