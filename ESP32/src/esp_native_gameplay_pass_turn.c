#include <stdint.h>
#include <stdio.h>

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_pass_turn.h"
#include "esp_player_view_state.h"

#define PASS_TURN_MAP_WIDTH 32U
#define PASS_TURN_TOUCH_TYPE_A 10U
#define PASS_TURN_TOUCH_TYPE_B 11U

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

static int deferredTileTouch(uint16_t tile,
                             uint16_t* outSprite,
                             uint8_t* outType) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;

    if (outSprite != NULL) *outSprite = UINT16_MAX;
    if (outType != NULL) *outType = 0U;
    if (topology == NULL || topology->spriteCount > UINT16_MAX) return -1;

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            return -1;
        }
        (void)subtype;
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile) {
            continue;
        }
        if (type == PASS_TURN_TOUCH_TYPE_A || type == PASS_TURN_TOUCH_TYPE_B) {
            if (outSprite != NULL) *outSprite = (uint16_t)i;
            if (outType != NULL) *outType = type;
            return 1;
        }
    }
    return 0;
}

EspNativeGameplayPassTurnStatus EspNativeGameplayPassTurn_execute(
    const EspNativeGameplayInputState* intent) {
    const EspPlayerViewState* view = EspPlayerView_view();
    uint16_t tile;
    uint16_t sprite = UINT16_MAX;
    uint8_t type = 0U;
    int touched;

    if (intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN) {
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_INVALID;
    }
    if (EspNativeGameplayDialog_isActive() || !EspMapSpriteTopology_isReady() ||
        !tileForView(view, &tile)) {
        printf("[PASSTURN] DEFER seq=%u reason=not-ready mutation=no\n",
               (unsigned int)intent->sequence);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY;
    }

    /* Legacy DoomCanvas PASSTURN first calls Game_touchTile(..., false).
     * That path invokes Entity_touched() only for linked type 10/11 entities on
     * the player's current tile. Those semantics are not yet natively owned, so
     * never silently skip them: fail closed before requesting the monster turn. */
    touched = deferredTileTouch(tile, &sprite, &type);
    if (touched < 0) {
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=topology-query mutation=no\n",
               (unsigned int)intent->sequence, (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY;
    }
    if (touched > 0) {
        printf("[PASSTURN] DEFER seq=%u tile=%u sprite=%u type=%u reason=legacy-touchTile-false-unowned mutation=no monsterTurn=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)tile,
               (unsigned int)sprite,
               (unsigned int)type);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_TILE_TOUCH_DEFERRED;
    }

    if (!EspNativeGameplayMonsterTurn_requestPassTurn(intent->sequence)) {
        printf("[PASSTURN] DEFER seq=%u tile=%u reason=turn-request-busy mutation=no\n",
               (unsigned int)intent->sequence, (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_PASS_TURN_REQUEST_BUSY;
    }

    printf("[PASSTURN] REQUEST seq=%u tile=%u pos=%d,%d angle=%d tileTouch=none type10/11=absent message=\"Turn passed.\"-deferred monsterTurn=requested playerMutation=no\n",
           (unsigned int)intent->sequence,
           (unsigned int)tile,
           (int)view->viewX,
           (int)view->viewY,
           (int)view->viewAngle);
    return ESP_NATIVE_GAMEPLAY_PASS_TURN_OK;
}
