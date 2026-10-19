/* Real permanent PASS_TURN executor: AUTOMAP must request the same one turn
 * as WORLD while retaining exclusive framebuffer ownership. No PAK or map
 * fixture: tile-touch, HUD, death, feedback and turn producer are mocked.
 * The resident action-route is asserted by automap_pass_turn_routing_host.py. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/esp_native_gameplay_pass_turn.c"

static EspPlayerViewState view;
static EspNativeGameplayHudState hud;
static EspNativeGameplayPlayerState player;
static EspNativeGameplayInputState input;
static int automap, hazardKind, turnAccept, presentCount, worldHudPaints;
static int requestCount, hazardCount, rollbackCount, queueCount, cancelCount;
static int damageNotes, deathArms;
static uint16_t damageNoted;
static uint32_t originalParam;

const EspPlayerViewState* EspPlayerView_view(void) { return &view; }
const EspNativeGameplayHudState* EspNativeGameplayHud_view(void) { return &hud; }
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    return &player;
}
int EspNativeResidentGameplay_isAutomapActive(void) { return automap; }
int EspMapSpriteTopology_isReady(void) { return 1; }
int EspNativeGameplayDialog_isActive(void) { return 0; }

EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processPassTurn(
    EspNativeGameplayHazardPassTurnUndo* undo) {
    ++hazardCount;
    memset(undo, 0, sizeof(*undo));
    if (hazardKind == 0) return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE;
    undo->committed = 1U;
    undo->feedbackQueued = 1U;
    undo->param1Before = player.param1;
    undo->tileIndex = 309U;
    /* 30 HP, 10 armor -> 27 HP, 8 armor, or lethal. */
    player.param1 = (player.param1 & 0xff00ff00U) |
                    (hazardKind == 2 ? 0U : 27U) | (8U << 16U);
    return hazardKind == 2
        ? ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_LETHAL_COMMITTED
        : ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED;
}
int EspNativeGameplayHazardTouch_rollbackPassTurn(
    const EspNativeGameplayHazardPassTurnUndo* undo) {
    assert(undo && undo->committed);
    ++rollbackCount;
    player.param1 = undo->param1Before;
    return 1;
}
int EspNativeGameplayActionEngine_queueFeedback(
    EspNativeGameplayActionFeedback feedback) {
    assert(feedback == ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN);
    ++queueCount;
    return 1;
}
int EspNativeGameplayActionEngine_cancelQueuedFeedback(
    EspNativeGameplayActionFeedback feedback) {
    assert(feedback == ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PASS_TURN);
    ++cancelCount;
    return 1;
}
int EspNativeGameplayMonsterTurn_requestPassTurn(uint32_t seq) {
    assert(seq == input.sequence);
    ++requestCount;
    return turnAccept;
}
int Esp32PlatformVideo_present(void) { ++presentCount; return 1; }
void EspNativeGameplayAutomap_noteDamage(uint16_t damage, uint8_t crit) {
    assert(automap && crit == 0U && damage != 0U);
    ++damageNotes;
    damageNoted = damage;
}
int EspNativeGameplayPlayerDeath_arm(struct DoomRPG_s* doomRpg,
                                     uint32_t seq, uint16_t tile) {
    assert(doomRpg && seq == input.sequence && tile == 309U);
    assert(automap && hazardKind == 2);
    ++deathArms;
    return 1;
}
EspNativeGameplayHudStatus EspNativeGameplayHud_repaint(
    const EspNativeGameplayHudState* state, EspNativeGameplayHudStats* stats) {
    assert(!automap && state == &hud);
    ++worldHudPaints;
    memset(stats, 0, sizeof(*stats));
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}

static void seed(int isAutomap, int hazard, int accept) {
    memset(&view, 0, sizeof(view));
    memset(&hud, 0, sizeof(hud));
    memset(&player, 0, sizeof(player));
    memset(&input, 0, sizeof(input));
    view.active = 1U;
    view.viewAngle = view.destAngle = 64;
    view.viewX = view.destX = 1376;
    view.viewY = view.destY = 608;
    hud.active = hud.painted = 1U;
    player.active = 1U;
    player.param1 = 30U | (10U << 16U);
    originalParam = player.param1;
    input.action = ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN;
    input.sequence = 44U;
    automap = isAutomap;
    hazardKind = hazard;
    turnAccept = accept;
    presentCount = worldHudPaints = requestCount = hazardCount = 0;
    rollbackCount = queueCount = cancelCount = damageNotes = deathArms = 0;
    damageNoted = 0U;
}

int main(void) {
    uint8_t dummyGame = 0;
    struct DoomRPG_s* game = (struct DoomRPG_s*)&dummyGame;

    seed(1, 0, 1);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_OK);
    assert(requestCount == 1 && hazardCount == 1 && presentCount == 1);
    assert(worldHudPaints == 0 && damageNotes == 0 && queueCount == 1);
    assert(player.param1 == originalParam && automap);

    seed(1, 1, 1);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_OK);
    assert(requestCount == 1 && hazardCount == 1 && presentCount == 1);
    assert(worldHudPaints == 0 && damageNotes == 1 && damageNoted == 5U);
    assert((player.param1 & 0xffU) == 27U);
    assert(((player.param1 >> 16U) & 0xffU) == 8U);

    seed(1, 1, 0);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_REQUEST_BUSY);
    assert(requestCount == 1 && rollbackCount == 1 && presentCount == 0);
    assert(worldHudPaints == 0 && damageNotes == 0);
    assert(player.param1 == originalParam && automap);

    seed(1, 2, 1);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_OK);
    assert(deathArms == 1 && requestCount == 0 && worldHudPaints == 0);
    assert(damageNotes == 0 && presentCount == 1 && automap);

    seed(0, 0, 1);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_OK);
    assert(requestCount == 1 && queueCount == 1 && presentCount == 1);
    assert(worldHudPaints == 0 && damageNotes == 0);

    seed(0, 1, 1);
    assert(EspNativeGameplayPassTurn_execute(game, &input) ==
           ESP_NATIVE_GAMEPLAY_PASS_TURN_OK);
    assert(worldHudPaints == 1 && requestCount == 1 && damageNotes == 0);

    puts("pass_turn_automap_host: PASS one turn in AUTOMAP, hazard 5-damage map FX after commit, rollback on busy, lethal arms death, WORLD routing unchanged, no automap HUD pixels");
    return 0;
}
