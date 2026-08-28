#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_move_events.h"
#include "esp_native_gameplay_select.h"
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
    uint8_t reserved[2];
} EspNativeResidentGameplayState;

static EspNativeResidentGameplayState gameplayState;

static void disableGameplay(const char* reason) {
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(1, NULL);
    }
    EspNativeGameplayDialog_reset();
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
               EspNativeGameplayDialog_isActive() ? "DIALOG" : "WORLD");
        printf("[TOUCHFEEDBACK] FLASH zone=%u action=%s edits=%u hold=%ums frame=%08x->%08x style=junction-neon-double-ring+vector-glyph\n",
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

    if (!renderCurrent(render, (uint8_t)afterView.viewAngle, "TURN")) {
        status = EspNativeGameplayDispatch_rollbackTurn(
            &afterView, &beforeView, &afterTurn, &beforeTurn, &result);
        if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            !renderCurrent(render, (uint8_t)beforeView.viewAngle, "TURN-ROLLBACK")) {
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

    memset(&beforeView, 0, sizeof(beforeView));
    memset(&afterView, 0, sizeof(afterView));
    memset(&result, 0, sizeof(result));
    memset(&moveDialog, 0, sizeof(moveDialog));

    status = EspNativeGameplayDispatch_prepareMove(
        intent, &beforeView, &afterView, &result);
    if (status == ESP_NATIVE_GAMEPLAY_DISPATCH_COLLISION_BLOCKED) {
        ++gameplayState.blocked;
        printf("[RESIDENTGAMEPLAY] MOVE-BLOCKED n=%u action=%s tile=%u->%u blocker=%u type=%u\n",
               (unsigned int)gameplayState.blocked,
               EspNativeGameplayInput_actionName(intent->action),
               (unsigned int)result.sourceTile,
               (unsigned int)result.destTile,
               (unsigned int)result.blockerSpriteIndex,
               (unsigned int)result.blockerType);
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

    if (!renderCurrent(render, (uint8_t)afterView.viewAngle, "MOVE")) {
        status = EspNativeGameplayDispatch_rollbackMove(
            &afterView, &beforeView, &result);
        if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            !renderCurrent(render, (uint8_t)beforeView.viewAngle, "MOVE-ROLLBACK")) {
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
        EspNativeGameplayDialogBeginStatus dialogStatus =
            EspNativeGameplayDialog_begin(
                moveDialog.eventIndex,
                moveDialog.commandOffset,
                moveDialog.runFlags);
        if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            status = EspNativeGameplayDispatch_rollbackMove(
                &afterView, &beforeView, &result);
            if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
                !renderCurrent(render, (uint8_t)beforeView.viewAngle,
                               "MOVE-DIALOG-ROLLBACK")) {
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

static void serviceSelect(Render_t* render,
                          const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayActionResult result;
    EspNativeGameplayActionStatus status;

    memset(&result, 0, sizeof(result));
    if (view == NULL || view->active != 1U ||
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

    if (status == ESP_NATIVE_GAMEPLAY_ACTION_DIALOG_READY) {
        EspNativeGameplayDialogBeginStatus dialogStatus =
            EspNativeGameplayDialog_begin(
                result.eventIndex,
                result.commandOffset,
                ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS);
        if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
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
        if (!renderCurrent(render, (uint8_t)view->viewAngle, "SELECT-DOOR")) {
            if (!EspNativeGameplayAction_rollbackSelect(&result) ||
                !renderCurrent(render, (uint8_t)view->viewAngle,
                               "SELECT-DOOR-ROLLBACK")) {
                disableGameplay("select-door-render-rollback");
                return;
            }
            printf("[RESIDENTGAMEPLAY] SELECT ROLLBACK seq=%u line=%u open=%u restored=yes\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)result.lineIndex,
                   (unsigned int)result.openBefore);
            return;
        }

        ++gameplayState.selects;
        printf("[ACTION] DOOR line=%u opcode=%u status=OK open=%u->%u locked=%u removed=%u->%u effects=%02x sound=%u\n",
               (unsigned int)result.lineIndex,
               (unsigned int)result.codeId,
               (unsigned int)result.openBefore,
               (unsigned int)result.openAfter,
               (unsigned int)result.locked,
               (unsigned int)result.removedBefore,
               (unsigned int)result.removedAfter,
               (unsigned int)result.effectFlags,
               (unsigned int)result.soundId);
        printf("[RESIDENTGAMEPLAY] SELECT n=%u seq=%u door=%u committed=yes redraw=yes collision=live animation=regular4frame-live sound=deferred entityRelink=deferred turnAdvance=deferred\n",
               (unsigned int)gameplayState.selects,
               (unsigned int)intent->sequence,
               (unsigned int)result.lineIndex);
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

void EspNativeResidentGameplay_reset(void) {
    PlatformInput_setTapCallback(NULL);
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(0, NULL);
    }
    EspNativeGameplayDialog_reset();
    EspNativeGameplayControls_reset();
    EspNativeGameplayInput_reset();
    memset(&gameplayState, 0, sizeof(gameplayState));
}

int EspNativeResidentGameplay_isActive(void) {
    return gameplayState.active != 0U && gameplayState.failed == 0U;
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
        if (doomRpg == NULL || doomRpg->render == NULL ||
            !EspNativeFirstFrame_isReady() || !EspNativeGameplayHud_isReady()) {
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

        EspNativeGameplayDialog_reset();
        EspNativeGameplayControls_reset();
        EspNativeGameplayInput_reset();
        gameplayState.active = 1U;
        PlatformInput_setTapCallback(onGameplayTap);
        printf("\n=== Doom RPG ESP32-native resident gameplay service ===\n");
        printf("[RESIDENTGAMEPLAY] READY map=current touch=invisible-12-zone+120ms-feedback dispatch=TURN+MOVE+SELECT_DOOR15/16+SELECT_DIALOG8/26 collision=native/entityDefs=%u moveEvents=door15/16+force24+enter-dialog8/26-live-other-deferred doorAnimation=regular4frame-live SELECT-entity/other/turn-advance/menu/automap/weapons=deferred\n",
               (unsigned int)EspEntityDefTypeCatalog_definitionCount());
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
        serviceSelect(doomRpg->render, &intent);
        break;

    default:
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] DEFER n=%u action=%s id=%u semantic-not-enabled\n",
               (unsigned int)gameplayState.deferred,
               EspNativeGameplayInput_actionName(intent.action),
               (unsigned int)intent.action);
        break;
    }
}
