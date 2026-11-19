#ifndef DOOMRPG_ESP32_NATIVE_DOOR_ANIMATOR_H
#define DOOMRPG_ESP32_NATIVE_DOOR_ANIMATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_DOOR_ANIMATION_MAX_LINES 8U
#define ESP_NATIVE_DOOR_ANIMATION_FRAMES 4U
#define ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES 3U
#define ESP_NATIVE_DOOR_ANIMATION_STEP 16U

typedef enum EspNativeDoorAnimationStatus_e {
    ESP_NATIVE_DOOR_ANIMATION_INVALID = 0,
    ESP_NATIVE_DOOR_ANIMATION_UNSUPPORTED = 1,
    ESP_NATIVE_DOOR_ANIMATION_SCHEDULED = 2
} EspNativeDoorAnimationStatus;

typedef struct EspNativeDoorAnimationFrame_s {
    uint8_t ordinal;
    uint8_t totalFrames;
    uint8_t activeLines;
    uint8_t geometryActive;
} EspNativeDoorAnimationFrame;

typedef struct EspNativeDoorAnimatorView_s {
    uint8_t activeLines;
    uint8_t completedFrames;
    uint8_t framePrepared;
    uint8_t geometryActive;
    uint32_t scheduledTransitions;
    uint32_t completedTransitions;
} EspNativeDoorAnimatorView;

/*
 * Tiny resident visual owner for regular legacy doors (line flags & 0x4).
 * The immutable EspMapRuntime remains untouched. A door slot stores only its
 * line index and one 0..3 displacement position. The renderer asks for a
 * transient displacement while composing one gameplay frame.
 *
 * Legacy defaults are recovered exactly: animFrames=4 and animPos=16. The
 * first three frames move geometry by one 16-unit step; the fourth presents
 * the stable target state. Up to eight lines may share one animation batch,
 * matching DoomCanvas.openDoors[8].
 */
void EspNativeDoorAnimator_reset(void);
EspNativeDoorAnimationStatus EspNativeDoorAnimator_begin(
    uint16_t lineIndex,
    uint8_t openBefore,
    uint8_t openAfter);
int EspNativeDoorAnimator_hasPendingFrames(void);

/*
 * A MOVE-triggered close may commit collision immediately while keeping the
 * regular door visually at its openBefore position through the monster turn.
 * This is presentation-only: EspMapLineState remains the gameplay authority.
 *
 * The hold is accepted only for an active 1->0 regular-door slot before any
 * animation frame has been prepared. If every pending slot is held this way,
 * the dynamic renderer may present their current (fully-open) displacement
 * without advancing the four-frame animation. The active-monster sequencer
 * releases the hold only after the turn, including its final attack, is done.
 */
int EspNativeDoorAnimator_deferCloseUntilMonsterTurn(uint16_t lineIndex);
int EspNativeDoorAnimator_isHoldingDeferredClose(void);
uint8_t EspNativeDoorAnimator_deferredCloseCount(void);
uint8_t EspNativeDoorAnimator_releaseDeferredCloses(void);

/* Sound is committed only after a successful MOVE frame, but a held 1->0
 * door has not started visibly closing yet. Reuse one reserved byte per
 * 8-byte slot; no new allocation or sound ownership in the animator.
 * Stage only a committed close still held at position 3. Following the
 * monster turn, take each cue once after a successfully rendered first
 * moving frame (completedFrames == 1). Aborted/reset slots discard cues. */
int EspNativeDoorAnimator_stageCommittedDeferredCloseSound(uint16_t lineIndex);
int EspNativeDoorAnimator_takePresentedDeferredCloseSound(
    uint16_t* outLineIndex);

/* Cancel a stale visual lease if its already-committed line state was rolled
 * back by an outer transaction before rendering. Returns 1 when the current
 * batch is still coherent or no batch exists, 0 when a stale batch was reset. */
int EspNativeDoorAnimator_validateLineState(void);

/* Prepare/finish exactly one visible animation frame. */
int EspNativeDoorAnimator_prepareFrame(EspNativeDoorAnimationFrame* outFrame);
int EspNativeDoorAnimator_finishFrame(int renderOk);

/* Returns 1 only for a line participating in the currently prepared moving
 * frame. displacement is relative to the immutable closed geometry: 0..48. */
int EspNativeDoorAnimator_getLineDisplacement(uint32_t lineIndex,
                                               int16_t* outDisplacement);

const EspNativeDoorAnimatorView* EspNativeDoorAnimator_view(void);

#ifdef __cplusplus
}
#endif

#endif
