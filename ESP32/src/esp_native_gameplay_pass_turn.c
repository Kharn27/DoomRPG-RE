#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_hazard_touch.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_pass_turn.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

#define PASS_TURN_MAP_WIDTH 32U

static int tileForView(const EspPlayerViewState* view, uint16_t* outTile) {
    uint32_t x;
    uint32_t y;
    if (outTile == NULL || view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle ||
        view->viewX < 0 || view->viewY < 0) {
        return 0;
    }
    x = (uint32_t)view->viewX >> 6;
    y = (uint32_t)view->viewY >> 6;
    if (x >= PASS_TURN_MAP_WIDTH || y >= PASS_TURN_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(y * PASS_TURN_MAP_WIDTH + x);
    return 1;
}

EspNativeGameplayPassTurnStatus EspNativeGameplayPassTurn_execute(
    const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayHazardPassTurnUndo hazardUndo;
    EspNativeGameplayHazardTouchStatus hazardStatus;
    uint16_t tile;
    int feedbackRollback;
    int hazardRollback;
    int feedbackPresented;

    if (intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN) {
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_INVALID;
    }
    if (EspNativeGameplayDialog_isActive() || !EspMapSpriteTopology_isReady() ||
        !tileForView(view, &tile)) {
        printf("[PASSTURN] DEFER seq=%u reason=not-ready mutation=no\n",
               (unsigned int)intent->sequence);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY;
    }

    memset(&hazardUndo, 0, sizeof(hazardUndo));
    hazardStatus = EspNativeGameplayHazardTouch_processPassTurn(&hazardUndo);
    if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_FATAL ||
        hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED) {
        printf("[PASSTURN] DEFER seq=%u tile=%u tileTouch=hazard-deferred status=%u monsterTurn=no mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               (unsigned int)hazardStatus);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_TILE_TOUCH_DEFERRED;
    }

    if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE &&
        !EspNativeGameplayActionEngine_queueFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN)) {
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=feedback-queue-not-ready mutation=no monsterTurn=no\n",
               (unsigned int)intent->sequence, (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY;
    }

    if (!EspNativeGameplayMonsterTurn_requestPassTurn(intent->sequence)) {
        feedbackRollback = 1;
        hazardRollback = 1;
        if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED) {
            hazardRollback = EspNativeGameplayHazardTouch_rollbackPassTurn(
                &hazardUndo);
        }
        else {
            feedbackRollback = EspNativeGameplayActionEngine_cancelQueuedFeedback(
                ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN);
        }
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=turn-request-busy hazard=%s hazardRollback=%s feedbackRollback=%s mutation=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED
                   ? "committed" : "none",
               hazardRollback ? "yes" : "NO",
               feedbackRollback ? "yes" : "NO",
               (hazardRollback && feedbackRollback) ? "rolled-back" : "ROLLBACK-FAILED");
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_REQUEST_BUSY;
    }

    /* The shared feedback service expires the previous visible message before
     * it paints a newly queued one. If a PASS TURN lands exactly as that older
     * lease crosses 1200 ms, expiry can otherwise replace the new pending kind
     * with NONE before the next resident service. Once the monster-turn request
     * is accepted there is no remaining gameplay rollback edge, so consume the
     * already-queued feedback through the normal wrapped presenter immediately.
     * A failed physical present deliberately leaves the feedback pending for the
     * regular service retry; gameplay state and turn ownership stay committed. */
    feedbackPresented = Esp32PlatformVideo_present();
    if (!feedbackPresented) {
        printf("[PASSTURN] FEEDBACK-DEFER seq=%u tile=%u cause=present-failed pending=retained monsterTurn=requested\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile);
    }

    if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED) {
        printf("[PASSTURN] REQUEST seq=%u tile=%u pos=%d,%d angle=%d tileTouch=hazard-committed type10/11=owned message=\"Turn passed.\"-legacy-superseded-by-hazard monsterTurn=requested playerMutation=hazard-owned feedbackPresent=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               (int)view->viewX,
               (int)view->viewY,
               (int)view->viewAngle,
               feedbackPresented ? "immediate" : "deferred");
    }
    else {
        printf("[PASSTURN] REQUEST seq=%u tile=%u pos=%d,%d angle=%d tileTouch=none type10/11=absent message=\"Turn passed.\"-queued monsterTurn=requested playerMutation=no feedbackPresent=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               (int)view->viewX,
               (int)view->viewY,
               (int)view->viewAngle,
               feedbackPresented ? "immediate" : "deferred");
    }
    return ESP_NATIVE_GAMEPLAY_PASS_TURN_OK;
}
