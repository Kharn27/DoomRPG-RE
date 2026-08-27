#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_input.h"
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
    uint32_t blocked;
    uint32_t deferred;
    uint8_t active;
    uint8_t failed;
    uint8_t reserved[2];
} EspNativeResidentGameplayState;

static EspNativeResidentGameplayState gameplayState;

static void disableGameplay(const char* reason) {
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(1, NULL);
    }
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
        printf("[RESIDENTGAMEPLAY] QUEUE tap=%u action=%s zone=%u logical=%d,%d\n",
               (unsigned int)gameplayState.taps,
               EspNativeGameplayInput_actionName(hit.action),
               (unsigned int)hit.zone,
               logicalX,
               logicalY);
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
    EspNativeGameplayDispatchStatus status;

    memset(&beforeView, 0, sizeof(beforeView));
    memset(&afterView, 0, sizeof(afterView));
    memset(&result, 0, sizeof(result));

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

    ++gameplayState.moves;
    printf("[RESIDENTGAMEPLAY] MOVE n=%u seq=%u action=%s tile=%u->%u delta=%d,%d pos=%d,%d tileEvents=deferred committed=yes\n",
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

void EspNativeResidentGameplay_reset(void) {
    PlatformInput_setTapCallback(NULL);
    if (EspNativeGameplayControls_isActive()) {
        (void)EspNativeGameplayControls_restore(0, NULL);
    }
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

        EspNativeGameplayControls_reset();
        EspNativeGameplayInput_reset();
        gameplayState.active = 1U;
        PlatformInput_setTapCallback(onGameplayTap);
        printf("\n=== Doom RPG ESP32-native resident gameplay service ===\n");
        printf("[RESIDENTGAMEPLAY] READY map=current touch=invisible-12-zone+120ms-junction-feedback dispatch=TURN+MOVE collision=native/entityDefs=%u tileEvents=deferred SELECT/menu/automap/weapons=deferred\n",
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

    if (EspNativeGameplayInput_peek() == NULL ||
        EspNativeGameplayInput_peek()->pending == 0U) {
        return;
    }

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

    default:
        ++gameplayState.deferred;
        printf("[RESIDENTGAMEPLAY] DEFER n=%u action=%s id=%u semantic-not-enabled\n",
               (unsigned int)gameplayState.deferred,
               EspNativeGameplayInput_actionName(intent.action),
               (unsigned int)intent.action);
        break;
    }
}
