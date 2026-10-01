#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_hazard_touch.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_pass_turn.h"
#include "esp_native_gameplay_player_death.h"
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

static int repaintCurrentHud(uint32_t sequence,
                             uint16_t tile,
                             const char* phase) {
    const EspNativeGameplayHudState* hud = EspNativeGameplayHud_view();
    EspNativeGameplayHudStats stats;
    EspNativeGameplayHudStatus status;

    memset(&stats, 0, sizeof(stats));
    if (hud == NULL || hud->active != 1U || hud->painted != 1U) {
        printf("[PASSTURN] HUD-REPAINT-DEFER seq=%u tile=%u phase=%s cause=current-hud-not-ready framebufferMutation=unknown\n",
               (unsigned int)sequence,
               (unsigned int)tile,
               phase != NULL ? phase : "unknown");
        return 0;
    }

    status = EspNativeGameplayHud_repaint(hud, &stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) {
        printf("[PASSTURN] HUD-REPAINT-DEFER seq=%u tile=%u phase=%s status=%u health=%u/%u armor=%u/%u framebufferMutation=possible\n",
               (unsigned int)sequence,
               (unsigned int)tile,
               phase != NULL ? phase : "unknown",
               (unsigned int)status,
               (unsigned int)hud->model.health,
               (unsigned int)hud->model.maxHealth,
               (unsigned int)hud->model.armor,
               (unsigned int)hud->model.maxArmor);
        return 0;
    }

    printf("[PASSTURN] HUD-REPAINT seq=%u tile=%u phase=%s health=%u/%u armor=%u/%u pixels=%u reads=%u bytes=%u source=current-player-overlay dirtyConsume=no present=no\n",
           (unsigned int)sequence,
           (unsigned int)tile,
           phase != NULL ? phase : "unknown",
           (unsigned int)hud->model.health,
           (unsigned int)hud->model.maxHealth,
           (unsigned int)hud->model.armor,
           (unsigned int)hud->model.maxArmor,
           (unsigned int)stats.pixelsWritten,
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead);
    return 1;
}

EspNativeGameplayPassTurnStatus EspNativeGameplayPassTurn_execute(
    struct DoomRPG_s* doomRpg,
    const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayHazardPassTurnUndo hazardUndo;
    EspNativeGameplayHazardTouchStatus hazardStatus;
    uint16_t tile;
    int feedbackRollback;
    int hazardRollback;
    int hudRollback;
    int feedbackPresented;

    if (doomRpg == NULL || intent == NULL ||
        intent->action != ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN) {
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

    if (hazardStatus ==
        ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_LETHAL_COMMITTED) {
        if (!EspNativeGameplayPlayerDeath_arm(
                doomRpg, intent->sequence, tile)) {
            hazardRollback =
                EspNativeGameplayHazardTouch_rollbackPassTurn(&hazardUndo);
            hudRollback = hazardRollback
                              ? repaintCurrentHud(intent->sequence, tile,
                                                  "lethal-arm-rollback")
                              : 0;
            printf("[PASSTURN] DEFER seq=%u tile=%u reason=player-death-arm hazardRollback=%s hudRollback=%s monsterTurn=no mutation=%s\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)tile,
                   hazardRollback ? "yes" : "NO",
                   hudRollback ? "yes" : "NO",
                   (hazardRollback && hudRollback)
                       ? "rolled-back" : "ROLLBACK-FAILED");
            return ESP_NATIVE_GAMEPLAY_PASS_TURN_TILE_TOUCH_DEFERRED;
        }

        feedbackPresented = Esp32PlatformVideo_present();
        if (!feedbackPresented) {
            printf("[PASSTURN] DEATH-FEEDBACK-DEFER seq=%u tile=%u cause=present-failed death=committed pending=retained\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)tile);
        }
        printf("[PASSTURN] DEATH seq=%u tile=%u tileTouch=hazard-lethal-committed deathOwner=armed monsterTurn=no input=blocked feedbackPresent=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               feedbackPresented ? "immediate" : "deferred");
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_OK;
    }

    if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE &&
        !EspNativeGameplayActionEngine_queueFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN)) {
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=feedback-queue-not-ready mutation=no monsterTurn=no\n",
               (unsigned int)intent->sequence, (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY;
    }

    /*
     * processPassTurn() intentionally commits only PlayerState + feedback and
     * leaves a rollback owner armed. Unlike MOVE-on-hazard, PASS_TURN does not
     * render a fresh world frame before its immediate feedback present. Repaint
     * the retained HUD bands now from EspNativeGameplayHud_view(); that symbol
     * is wrapped by PlayerResources and therefore overlays the authoritative
     * current PlayerState health/armor/weapon values without introducing a
     * second HUD owner.
     *
     * Do this before the MonsterTurn request so a paint failure can still roll
     * back both gameplay state and framebuffer presentation exactly.
     */
    if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED &&
        !repaintCurrentHud(intent->sequence, tile, "hazard-commit")) {
        hazardRollback =
            EspNativeGameplayHazardTouch_rollbackPassTurn(&hazardUndo);
        hudRollback = hazardRollback
                          ? repaintCurrentHud(intent->sequence, tile,
                                              "hazard-paint-rollback")
                          : 0;
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=hazard-hud-repaint hazardRollback=%s hudRollback=%s monsterTurn=no mutation=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               hazardRollback ? "yes" : "NO",
               hudRollback ? "yes" : "NO",
               (hazardRollback && hudRollback)
                   ? "rolled-back" : "ROLLBACK-FAILED");
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_TILE_TOUCH_DEFERRED;
    }

    if (!EspNativeGameplayMonsterTurn_requestPassTurn(intent->sequence)) {
        feedbackRollback = 1;
        hazardRollback = 1;
        hudRollback = 1;
        if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED) {
            hazardRollback = EspNativeGameplayHazardTouch_rollbackPassTurn(
                &hazardUndo);
            hudRollback = hazardRollback
                              ? repaintCurrentHud(intent->sequence, tile,
                                                  "turn-request-rollback")
                              : 0;
        }
        else {
            feedbackRollback = EspNativeGameplayActionEngine_cancelQueuedFeedback(
                ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN);
        }
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=turn-request-busy hazard=%s hazardRollback=%s hudRollback=%s feedbackRollback=%s mutation=%s\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED
                   ? "committed" : "none",
               hazardRollback ? "yes" : "NO",
               hudRollback ? "yes" : "NO",
               feedbackRollback ? "yes" : "NO",
               (hazardRollback && hudRollback && feedbackRollback)
                   ? "rolled-back" : "ROLLBACK-FAILED");
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
