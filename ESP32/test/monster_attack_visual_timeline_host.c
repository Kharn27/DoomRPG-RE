/* Execute the REAL native monster attack timeline with mocked clock, frame
 * presenter and transient sprite. No duplicate state machine in this test.
 * -O2/--gc-sections discard unrelated startup/monster functions. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/esp_native_gameplay_monster_attack_visual.c"

static DoomRPG_t game;
static Render_t render;
static EspPlayerViewState playerView;
static uint32_t fakeMs;
static uint32_t renderCalls;
static uint32_t transientClears;
static int failFrame;
static int touchLease;

unsigned int DoomRPG_GetUpTimeMS(void) { return fakeMs; }
const EspPlayerViewState* EspPlayerView_view(void) { return &playerView; }
int EspNativeGameplayControls_isActive(void) { return touchLease; }
int EspNativeGameplayFrame_renderTurn(struct Render_s* owner,
                                      uint8_t angle,
                                      EspNativeGameplayFrameStats* stats) {
    (void)angle;
    assert(owner == &render);
    ++renderCalls;
    if (failFrame) return 0;
    if (stats) {
        stats->frameAfterFNV = 0x12345678U;
        stats->finalPresented = 1U;
    }
    return 1;
}
int EspNativeSpriteRenderer_armTransient(uint16_t logical, uint8_t anim,
                                         int16_t worldX, int16_t worldY) {
    (void)anim; (void)worldX; (void)worldY;
    assert(logical == 204U);
    return 1;
}
void EspNativeSpriteRenderer_clearTransient(void) { ++transientClears; }
const EspNativeGraphicsCatalogRecord*
EspNativeGraphicsCatalog_findSprite(uint16_t logical) {
    (void)logical;
    return NULL;
}

static void prime(uint32_t probe, uint8_t total, uint8_t completed) {
    memset(&game, 0, sizeof(game));
    memset(&playerView, 0, sizeof(playerView));
    EspNativeGameplayMonsterAttackVisual_reset();
    game.render = &render;
    playerView.active = 1U;
    playerView.viewAngle = playerView.destAngle = 0;
    playerView.viewX = playerView.destX = 672;
    playerView.viewY = playerView.destY = 544;
    fakeMs = 1000U;
    renderCalls = transientClears = 0U;
    failFrame = touchLease = 0;
    attackVisual.active = 1U;
    attackVisual.activeProbe = probe;
    attackVisual.activeSpriteIndex = 157U;
    attackVisual.poseActive = 1U;
    attackVisual.clearAtMs = fakeMs;
    activeSequence = 1U;
    activeVisualFrame = activeAttackFrame = 1U;
    activeTotalLoops = total;
    activeCompletedLoops = completed;
    activePhaseMs = total == 1U ? 500U : 250U;
}
static void advance(void) { serviceTimeline(&game); }

int main(void) {
    /* One shot, no projectile: final IDLE present unblocks retaliation
     * in the same service call, NOT after a second 500ms timeout. */
    prime(17U, 1U, 1U);
    advance();
    assert(renderCalls == 1U && fakeMs == 1000U &&
           attackVisual.completedProbe == 17U && activeSequence == 0U &&
           EspNativeGameplayMonsterAttackVisual_isProbeComplete(17U));

    /* Failed final-idle presentation does not resolve or mutate the RNG.
     * The presentation retry can close the probe on its first success. */
    prime(18U, 1U, 1U);
    const Random_t randomBefore = game.random;
    failFrame = 1;
    advance();
    assert(attackVisual.completedProbe == 0U && activeSequence == 1U &&
           attackVisual.poseActive == 1U && attackVisual.clearAtMs == 1001U);
    assert(memcmp(&game.random, &randomBefore, sizeof(randomBefore)) == 0);
    failFrame = 0;
    fakeMs = 1001U;
    advance();
    assert(renderCalls == 2U && attackVisual.completedProbe == 18U &&
           activeSequence == 0U);

    /* Touch snapshot still prevents a world redraw until release. */
    prime(19U, 1U, 1U);
    touchLease = 1;
    advance();
    assert(renderCalls == 0U && activeSequence == 1U);
    touchLease = 0;
    advance();
    assert(attackVisual.completedProbe == 19U && renderCalls == 1U);

    /* Three-shot cadence: intermediary attack/idle transitions still hold
     * exactly 250ms each; ONLY the redundant last idle hold is removed. */
    prime(20U, 3U, 1U);
    advance(); /* shot 1 -> idle */
    assert(activeSequence && !attackVisual.poseActive &&
           attackVisual.completedProbe == 0U &&
           attackVisual.clearAtMs == 1250U);
    fakeMs = 1250U; advance(); /* idle -> shot 2 */
    assert(activeCompletedLoops == 2U && attackVisual.poseActive);
    fakeMs = 1500U; advance(); /* shot 2 -> idle */
    assert(activeSequence && !attackVisual.poseActive &&
           attackVisual.clearAtMs == 1750U);
    fakeMs = 1750U; advance(); /* idle -> shot 3 */
    assert(activeCompletedLoops == 3U && attackVisual.poseActive);
    fakeMs = 2000U; advance(); /* shot 3 -> idle -> ready */
    assert(attackVisual.completedProbe == 20U &&
           activeSequence == 0U && renderCalls == 5U);

    /* Malwrath's last sprite 204 transient may still be on screen when
     * final idle paints. Drain its last clear frame before resolving. */
    prime(21U, 3U, 3U);
    projectileEnabled = 1U;
    projectileShots = 3U;
    projectileSubtype = 6U;
    projectileStep = 3U;
    projectileNextMs = 1100U;
    advance(); /* last idle presented at t=1000 */
    assert(attackVisual.completedProbe == 0U &&
           activeSequence == 1U && !attackVisual.poseActive &&
           attackVisual.clearAtMs == 1001U);
    fakeMs = 1100U;
    failFrame = 1; advance(); /* clear present failed -> pending */
    assert(attackVisual.completedProbe == 0U &&
           activeSequence == 1U && projectileEnabled != 0U &&
           projectileNextMs == 1101U);
    fakeMs = 1101U;
    failFrame = 0; advance(); /* successful projectile clear => resolve */
    assert(projectileEnabled == 0U && attackVisual.completedProbe == 21U &&
           activeSequence == 0U && renderCalls == 3U);

    puts("monster_attack_visual_timeline_host: PASS real one-shot same-tick, render rollback, touch lease, triple-shot cadence and last-projectile clear");
    return 0;
}
