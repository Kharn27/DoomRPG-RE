/*
 * Actual native animator + SFX owner, using only host map/audio stand-ins.
 * Exercise the exact hardware sequence observed on Entrance line 102:
 * MOVE commit/HELD-FRAME -> monster turn -> RELEASE -> first presented
 * closing frame -> 5064, once. No audio during the monster's move/attack.
 *
 * Link the actual production C units and drop unrelated unused entry points.
 * This is host logic validation, not a claim about physical CYD playback.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_runtime.h"
#include "esp_map_line_state.h"
#include "esp_native_door_animator.h"
#include "esp_native_door_sfx.h"

static uint8_t openState[512];
static uint16_t played[32];
static uint8_t flags[32], priorities[32];
static unsigned playedCount;

int EspMapRuntime_isLoaded(void) { return 1; }
int EspMapLineState_isReady(void) { return 1; }
int EspMapRuntime_getLine(uint32_t index, EspMapLine* out) {
    if (index >= 512U || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->flags = 0x00000004UL; /* original regular-door flag */
    return 1;
}
int EspMapLineState_getOpen(uint32_t index, uint8_t* out) {
    if (index >= 512U || out == NULL) return 0;
    *out = openState[index];
    return 1;
}
int EspNativeAudioIntent_publish(uint16_t id, uint8_t f, uint8_t priority) {
    assert(playedCount < 32U);
    played[playedCount] = id;
    flags[playedCount] = f;
    priorities[playedCount] = priority;
    ++playedCount;
    return 1;
}

static void reset(void) {
    EspNativeDoorAnimator_reset();
    memset(openState, 0, sizeof(openState));
    memset(played, 0, sizeof(played));
    playedCount = 0U;
}
static void armHeldClose(uint16_t line) {
    /* The caller has already committed line 1 -> 0 transactionally. */
    openState[line] = 0U;
    assert(EspNativeDoorAnimator_begin(line, 1U, 0U) ==
           ESP_NATIVE_DOOR_ANIMATION_SCHEDULED);
    assert(EspNativeDoorAnimator_deferCloseUntilMonsterTurn(line));
    assert(EspNativeDoorAnimator_deferredCloseCount() >= 1U);
    assert(EspNativeDoorSfx_publishCommitted(
        line, 1U, 0U, ESP_MAP_LINE_SOUND_CLOSE,
        ESP_MAP_LINE_EFFECT_PLAY_SOUND));
}
static void presentFirstClosingFrame(void) {
    EspNativeDoorAnimationFrame frame = {0};
    assert(EspNativeDoorAnimator_prepareFrame(&frame));
    assert(frame.ordinal == 1U && frame.geometryActive == 1U);
    assert(EspNativeDoorAnimator_finishFrame(1)); /* physical frame OK */
}
static void assertAudio(unsigned count) {
    unsigned i;
    assert(playedCount == count);
    for (i = 0U; i < playedCount; ++i) {
        assert(played[i] == 5064U);
        assert(flags[i] == 0U && priorities[i] == 2U);
    }
}
int main(void) {
    uint16_t sentinel = 0xffffU;
    EspNativeDoorAnimationFrame frame = {0};

    /* Actual line102 incident: the 5064 stays silent during HOLD and
     * even if a second post-commit publication is replayed. */
    reset();
    armHeldClose(102U);
    assert(EspNativeDoorAnimator_isHoldingDeferredClose());
    assert(EspNativeDoorSfx_publishCommitted(
        102U, 1U, 0U, ESP_MAP_LINE_SOUND_CLOSE,
        ESP_MAP_LINE_EFFECT_PLAY_SOUND)); /* duplicate: idempotent */
    assertAudio(0U);
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    assertAudio(0U);
    assert(!EspNativeDoorAnimator_takePresentedDeferredCloseSound(&sentinel));
    assert(sentinel == 0xffffU);
    assert(EspNativeDoorAnimator_releaseDeferredCloses() == 1U);
    assertAudio(0U);
    presentFirstClosingFrame();
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 1U);
    assertAudio(1U);
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    assertAudio(1U);

    /* Three remaining frames cannot replay 5064. */
    for (unsigned i = 0U; i < 3U; ++i) {
        assert(EspNativeDoorAnimator_prepareFrame(&frame));
        assert(EspNativeDoorAnimator_finishFrame(1));
        assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    }
    assert(!EspNativeDoorAnimator_hasPendingFrames());
    assertAudio(1U);

    /* Two simultaneous regular doors: one sound per distinct committed line. */
    reset();
    armHeldClose(102U);
    armHeldClose(113U);
    assert(EspNativeDoorAnimator_deferredCloseCount() == 2U);
    assertAudio(0U);
    assert(EspNativeDoorAnimator_releaseDeferredCloses() == 2U);
    presentFirstClosingFrame();
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 2U);
    assertAudio(2U);
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);

    /* First frame failed -> no acoustic effect, then reset drops lease. */
    reset();
    armHeldClose(102U);
    assert(EspNativeDoorAnimator_releaseDeferredCloses() == 1U);
    assert(EspNativeDoorAnimator_prepareFrame(&frame));
    assert(EspNativeDoorAnimator_finishFrame(0)); /* render failure */
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    EspNativeDoorAnimator_reset(); /* real renderer abort path */
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    assertAudio(0U);

    /* Canonical line state rollback kills the animation and its staged cue. */
    reset();
    armHeldClose(102U);
    openState[102] = 1U;
    assert(!EspNativeDoorAnimator_validateLineState());
    assert(!EspNativeDoorAnimator_hasPendingFrames());
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    assertAudio(0U);

    /* Non-held committed 5064 retains legacy immediate publication. */
    reset();
    openState[102] = 0U;
    assert(EspNativeDoorAnimator_begin(102U, 1U, 0U) ==
           ESP_NATIVE_DOOR_ANIMATION_SCHEDULED);
    assert(EspNativeDoorSfx_publishCommitted(
        102U, 1U, 0U, ESP_MAP_LINE_SOUND_CLOSE,
        ESP_MAP_LINE_EFFECT_PLAY_SOUND));
    assertAudio(1U);
    presentFirstClosingFrame();
    assert(EspNativeDoorSfx_onFirstPresentedHeldClosingFrame() == 0U);
    assertAudio(1U);

    puts("native_door_held_close_sfx_host: PASS committed 5064 after first visible frame only, dual, duplicate, rollback, failed render, normal close");
    return 0;
}
