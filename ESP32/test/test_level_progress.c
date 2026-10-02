/* Host regression: cc -std=c11 -Wall -Wextra -Werror -IESP32/include
 * ESP32/test/test_level_progress.c ESP32/src/esp_player_fresh_map_state.c
 * ESP32/src/esp_map_catalog.c -o /tmp/test_level_progress && /tmp/test_level_progress
 * Only player/HUD view boundaries are mocked; the progress owner is production C. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp_player_fresh_map_state.h"
#include "esp_native_gameplay_player_state.h"

static EspPlayerViewState view;
static EspHudRefreshState hud;
static EspNativeGameplayPlayerState player;

const EspPlayerViewState* EspPlayerView_view(void) { return &view; }
const EspHudRefreshState* EspHudRefresh_view(void) { return &hud; }
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    return &player;
}
int EspPlayerView_consumePlayerSetup(uint8_t map, uint8_t loadMap, uint8_t load) {
    assert(map == view.targetMapId && loadMap == view.gameplayLoadMapId &&
           load == view.loadType);
    view.playerSetupPending = 0U;
    return 1;
}

static void fresh(uint8_t map, uint32_t xp) {
    EspPlayerFreshMap_reset();
    memset(&view, 0, sizeof(view));
    memset(&hud, 0, sizeof(hud));
    view.targetMapId = map;
    view.gameplayLoadMapId = map;
    view.active = view.spawnApplied = 1U;
    view.viewX = view.destX = 32;
    view.viewY = view.destY = 32;
    view.viewZ = 36;
    view.viewZOld = 4;
    view.facingRefreshPending = view.playerSetupPending = view.tileEnterPending = 1U;
    hud.targetMapId = hud.gameplayLoadMapId = map;
    hud.reason = ESP_HUD_REFRESH_REASON_POST_SPAWN;
    hud.refreshPending = hud.routed = hud.active = 1U;
    player.xpGained = xp;
    assert(EspPlayerFreshMap_route(100U, 0U) == ESP_PLAYER_FRESH_MAP_OK);
}

int main(void) {
    EspPlayerLevelProgress p, saved, invalid;
    assert(sizeof(p) == 16U && sizeof(player) == 52U);
    EspPlayerFreshMap_reset();
    EspPlayerFreshMap_recordMove();
    assert(!EspPlayerFreshMap_snapshotProgress(1000U, &p));
    assert(!EspPlayerFreshMap_snapshotProgress(1000U, NULL));
    fresh(1U, 200U);
    assert(EspPlayerFreshMap_snapshotProgress(10000U, &p));
    assert(p.elapsedMs == 0U && p.moves == 0U && p.complete == 1U &&
           p.xpBaseline == 200U); /* Loading must not count. */
    EspPlayerFreshMap_beginTimer(10000U);
    EspPlayerFreshMap_beginTimer(11000U); /* Re-arming must not restart it. */
    EspPlayerFreshMap_recordMove();
    EspPlayerFreshMap_recordMove();
    player.xpGained += 75U;
    player.currentXP = 5U; /* A level-up must not erase the level's earned XP. */
    assert(EspPlayerFreshMap_snapshotProgress(12500U, &saved));
    assert(saved.elapsedMs == 2500U && saved.moves == 2U &&
           player.xpGained - saved.xpBaseline == 75U);
    EspPlayerFreshMap_reset();
    assert(EspPlayerFreshMap_restoreProgress(&saved));
    assert(EspPlayerFreshMap_snapshotProgress(300000U, &p));
    assert(p.elapsedMs == 2500U && p.moves == 2U); /* No offline/LOAD duration. */
    EspPlayerFreshMap_beginTimer(300000U);
    EspPlayerFreshMap_recordMove();
    assert(EspPlayerFreshMap_snapshotProgress(301500U, &p));
    assert(p.elapsedMs == 4000U && p.moves == 3U && p.complete == 1U);
    invalid = saved;
    invalid.targetMapId = 0U;
    assert(!EspPlayerFreshMap_restoreProgress(&invalid));
    invalid = saved;
    invalid.complete = 2U;
    assert(!EspPlayerFreshMap_restoreProgress(&invalid));
    invalid = saved;
    invalid.reserved[1] = 1U;
    assert(!EspPlayerFreshMap_restoreProgress(&invalid));
    assert(!EspPlayerFreshMap_restoreProgress(NULL));
    assert(EspPlayerFreshMap_snapshotProgress(301500U, &p) && p.moves == 3U);
    EspPlayerFreshMap_resumeLegacy(1U, player.xpGained);
    assert(EspPlayerFreshMap_snapshotProgress(999U, &p));
    assert(p.complete == 0U && p.elapsedMs == 0U && p.moves == 0U &&
           p.xpBaseline == player.xpGained);
    assert(EspPlayerFreshMap_restoreProgress(&p)); /* Partial flag survives saving. */
    assert(EspPlayerFreshMap_snapshotProgress(999U, &p) && !p.complete);
    fresh(2U, player.xpGained);
    assert(EspPlayerFreshMap_snapshotProgress(999U, &p) && p.complete);
    EspPlayerFreshMap_beginTimer(UINT32_MAX - 999U);
    assert(EspPlayerFreshMap_snapshotProgress(1000U, &p) && p.elapsedMs == 2000U);
    p.moves = UINT32_MAX;
    p.elapsedMs = UINT32_MAX - 10U;
    assert(EspPlayerFreshMap_restoreProgress(&p));
    EspPlayerFreshMap_beginTimer(0U);
    EspPlayerFreshMap_recordMove();
    assert(EspPlayerFreshMap_snapshotProgress(1000U, &p));
    assert(p.moves == UINT32_MAX && p.elapsedMs == UINT32_MAX);
    puts("level progress: fresh/reset, XP baseline, resume, legacy, wrap, saturation PASS");
    return 0;
}
