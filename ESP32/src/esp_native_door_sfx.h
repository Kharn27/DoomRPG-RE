#ifndef DOOMRPG_ESP32_NATIVE_DOOR_SFX_H
#define DOOMRPG_ESP32_NATIVE_DOOR_SFX_H

#include <stdint.h>
#include <stdio.h>

#include "esp_map_line_state.h"
#include "esp_native_audio_intent.h"
#include "esp_native_door_animator.h"

/*
 * Door sound is a POST-COMMIT side effect, never part of the line applier,
 * animator or render preflight. Only committed 0->1 / 1->0 transitions may
 * enqueue one legacy SFX, after every enclosing rollback lease is closed.
 * No allocations, SD access, renderer changes, or extra turns.
 *
 * Legacy source: Game_performDoorEvent() -> Sound_playSound(id, 0, 2).
 */
static inline int EspNativeDoorSfx_publishCommitted(
    uint16_t lineIndex,
    uint8_t openBefore,
    uint8_t openAfter,
    uint16_t soundId,
    uint8_t effectFlags) {
    const uint16_t expected =
        openAfter != 0U ? ESP_MAP_LINE_SOUND_OPEN : ESP_MAP_LINE_SOUND_CLOSE;
    int published;

    if ((effectFlags & ESP_MAP_LINE_EFFECT_PLAY_SOUND) == 0U ||
        openBefore > 1U || openAfter > 1U ||
        openBefore == openAfter || soundId != expected) {
        return 0;
    }

    /* MOVE closes are transactionally committed before the monster turn,
     * but HELD animation has not moved onscreen yet. Reserve its 5064 in
     * the existing animation slot and publish at first visible close frame. */
    if (openBefore == 1U && openAfter == 0U &&
        EspNativeDoorAnimator_stageCommittedDeferredCloseSound(lineIndex)) {
        printf("[DOORSFX] CLOSE-STAGED line=%u open=1->0 sound=5064 priority=2 queued=no phase=held-before-monster-turn rollback=closed\\n",
               (unsigned int)lineIndex);
        return 1;
    }

    published = EspNativeAudioIntent_publish(soundId, 0U, 2U);
    printf("[DOORSFX] COMMIT line=%u open=%u->%u sound=%u priority=2 queued=%s rollback=closed\n",
           (unsigned int)lineIndex,
           (unsigned int)openBefore,
           (unsigned int)openAfter,
           (unsigned int)soundId,
           published != 0 ? "yes" : "no");
    return published != 0;
}

/*
 * SELECT open-only first-visual-frame audio lease.
 * A completed line mutation is staged before rendering but NOT enqueued.
 * The animation owner publishes after the first real frame succeeds; the
 * existing safe main-task music pump is then invoked immediately.
 * Ordinary late publication remains the fallback for non-animated doors.
 * Held MOVE-close 5064 is staged at commit and published after the first
 * visible moving frame, following the monster turn; other CLOSEs still
 * follow the ordinary post-commit path.
 *
 * A visible first animation frame cannot be rolled back acoustically if
 * a later frame fails; the sound represents the already observed motion.
 * No sound is emitted for an unpresented/failed FIRST frame.
 */
struct EspNativeGameplayActionResult_s;
void EspNativeDoorSfx_stageSelectOpenings(
    const struct EspNativeGameplayActionResult_s* result);
uint8_t EspNativeDoorSfx_onFirstPresentedOpeningFrame(void);
uint8_t EspNativeDoorSfx_onFirstPresentedHeldClosingFrame(void);
void EspNativeDoorSfx_cancelSelect(void);
void EspNativeDoorSfx_finishSelect(
    const struct EspNativeGameplayActionResult_s* result);

#endif
