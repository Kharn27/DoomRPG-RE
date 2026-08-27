#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_native_door_animator.h"
#include "esp_native_resident_gameplay.h"

#define LEGACY_REGULAR_DOOR_FLAG 0x00000004UL

typedef struct EspNativeDoorAnimationSlot_s {
    uint16_t lineIndex;
    uint8_t position;
    uint8_t targetOpen;
    uint8_t openBefore;
    uint8_t active;
    uint8_t reserved[2];
} EspNativeDoorAnimationSlot;

typedef struct EspNativeDoorAnimatorState_s {
    EspNativeDoorAnimationSlot slots[ESP_NATIVE_DOOR_ANIMATION_MAX_LINES];
    EspNativeDoorAnimatorView view;
} EspNativeDoorAnimatorState;

typedef char EspNativeDoorAnimationSlot_must_be_8_bytes[
    sizeof(EspNativeDoorAnimationSlot) == 8U ? 1 : -1];
typedef char EspNativeDoorAnimatorState_must_be_76_bytes[
    sizeof(EspNativeDoorAnimatorState) == 76U ? 1 : -1];

static EspNativeDoorAnimatorState animator;

EspMapLineDoorStatus __real_EspMapLineState_applyDoorCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineDoorResult* outResult);

static void clearBatchPreserveCounters(void) {
    uint32_t scheduled = animator.view.scheduledTransitions;
    uint32_t completed = animator.view.completedTransitions;
    memset(&animator, 0, sizeof(animator));
    animator.view.scheduledTransitions = scheduled;
    animator.view.completedTransitions = completed;
}

static int slotForLine(uint16_t lineIndex) {
    uint32_t i;
    for (i = 0U; i < ESP_NATIVE_DOOR_ANIMATION_MAX_LINES; ++i) {
        if (animator.slots[i].active != 0U &&
            animator.slots[i].lineIndex == lineIndex) {
            return (int)i;
        }
    }
    return -1;
}

static int freeSlot(void) {
    uint32_t i;
    for (i = 0U; i < ESP_NATIVE_DOOR_ANIMATION_MAX_LINES; ++i) {
        if (animator.slots[i].active == 0U) return (int)i;
    }
    return -1;
}

void EspNativeDoorAnimator_reset(void) {
    memset(&animator, 0, sizeof(animator));
}

EspNativeDoorAnimationStatus EspNativeDoorAnimator_begin(
    uint16_t lineIndex,
    uint8_t openBefore,
    uint8_t openAfter) {
    EspMapLine line;
    uint8_t currentOpen;
    int index;

    if (openBefore > 1U || openAfter > 1U || openBefore == openAfter ||
        !EspMapRuntime_isLoaded() || !EspMapLineState_isReady() ||
        !EspMapRuntime_getLine(lineIndex, &line) ||
        !EspMapLineState_getOpen(lineIndex, &currentOpen) ||
        currentOpen != openAfter) {
        return ESP_NATIVE_DOOR_ANIMATION_INVALID;
    }

    if ((line.flags & LEGACY_REGULAR_DOOR_FLAG) == 0U) {
        printf("[DOORANIM] SNAP line=%u open=%u->%u flags=%08x reason=non-regular-door\n",
               (unsigned int)lineIndex,
               (unsigned int)openBefore,
               (unsigned int)openAfter,
               (unsigned int)line.flags);
        return ESP_NATIVE_DOOR_ANIMATION_UNSUPPORTED;
    }

    if (animator.view.framePrepared != 0U ||
        animator.view.completedFrames != 0U ||
        slotForLine(lineIndex) >= 0) {
        return ESP_NATIVE_DOOR_ANIMATION_INVALID;
    }

    index = freeSlot();
    if (index < 0) return ESP_NATIVE_DOOR_ANIMATION_INVALID;

    animator.slots[index].lineIndex = lineIndex;
    animator.slots[index].position = openBefore != 0U
                                         ? ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES
                                         : 0U;
    animator.slots[index].targetOpen = openAfter;
    animator.slots[index].openBefore = openBefore;
    animator.slots[index].active = 1U;
    ++animator.view.activeLines;
    ++animator.view.scheduledTransitions;

    printf("[DOORANIM] ARM line=%u open=%u->%u flags=%08x frames=%u moving=%u step=%u ownerBytes=%u generic=yes\n",
           (unsigned int)lineIndex,
           (unsigned int)openBefore,
           (unsigned int)openAfter,
           (unsigned int)line.flags,
           (unsigned int)ESP_NATIVE_DOOR_ANIMATION_FRAMES,
           (unsigned int)ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES,
           (unsigned int)ESP_NATIVE_DOOR_ANIMATION_STEP,
           (unsigned int)sizeof(animator));
    return ESP_NATIVE_DOOR_ANIMATION_SCHEDULED;
}

int EspNativeDoorAnimator_hasPendingFrames(void) {
    return animator.view.activeLines != 0U;
}

int EspNativeDoorAnimator_validateLineState(void) {
    uint32_t i;

    if (!EspNativeDoorAnimator_hasPendingFrames()) return 1;
    if (!EspMapRuntime_isLoaded() || !EspMapLineState_isReady()) {
        printf("[DOORANIM] CANCEL reason=runtime-not-ready\n");
        clearBatchPreserveCounters();
        return 0;
    }

    for (i = 0U; i < ESP_NATIVE_DOOR_ANIMATION_MAX_LINES; ++i) {
        EspNativeDoorAnimationSlot* slot = &animator.slots[i];
        uint8_t currentOpen;
        if (slot->active == 0U) continue;
        if (!EspMapLineState_getOpen(slot->lineIndex, &currentOpen) ||
            currentOpen != slot->targetOpen) {
            printf("[DOORANIM] CANCEL line=%u expectedOpen=%u reason=outer-rollback\n",
                   (unsigned int)slot->lineIndex,
                   (unsigned int)slot->targetOpen);
            clearBatchPreserveCounters();
            return 0;
        }
    }
    return 1;
}

int EspNativeDoorAnimator_prepareFrame(EspNativeDoorAnimationFrame* outFrame) {
    uint32_t i;
    uint8_t geometryActive;

    if (outFrame != NULL) memset(outFrame, 0, sizeof(*outFrame));
    if (outFrame == NULL || !EspNativeDoorAnimator_hasPendingFrames() ||
        animator.view.framePrepared != 0U ||
        animator.view.completedFrames >= ESP_NATIVE_DOOR_ANIMATION_FRAMES ||
        !EspNativeDoorAnimator_validateLineState()) {
        return 0;
    }

    geometryActive = animator.view.completedFrames <
                     ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES;
    if (geometryActive != 0U) {
        for (i = 0U; i < ESP_NATIVE_DOOR_ANIMATION_MAX_LINES; ++i) {
            EspNativeDoorAnimationSlot* slot = &animator.slots[i];
            if (slot->active == 0U) continue;
            if (slot->targetOpen != 0U) {
                if (slot->position >= ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES) {
                    return 0;
                }
                ++slot->position;
            }
            else {
                if (slot->position == 0U) return 0;
                --slot->position;
            }
        }
    }

    animator.view.framePrepared = 1U;
    animator.view.geometryActive = geometryActive;
    outFrame->ordinal = (uint8_t)(animator.view.completedFrames + 1U);
    outFrame->totalFrames = ESP_NATIVE_DOOR_ANIMATION_FRAMES;
    outFrame->activeLines = animator.view.activeLines;
    outFrame->geometryActive = geometryActive;
    return 1;
}

int EspNativeDoorAnimator_finishFrame(int renderOk) {
    if (!EspNativeDoorAnimator_hasPendingFrames() ||
        animator.view.framePrepared != 1U) {
        return 0;
    }

    animator.view.framePrepared = 0U;
    animator.view.geometryActive = 0U;
    if (!renderOk) return 1;

    ++animator.view.completedFrames;
    if (animator.view.completedFrames == ESP_NATIVE_DOOR_ANIMATION_FRAMES) {
        animator.view.completedTransitions += animator.view.activeLines;
        clearBatchPreserveCounters();
    }
    return 1;
}

int EspNativeDoorAnimator_getLineDisplacement(uint32_t lineIndex,
                                               int16_t* outDisplacement) {
    int index;
    if (outDisplacement != NULL) *outDisplacement = 0;
    if (outDisplacement == NULL || lineIndex > UINT16_MAX ||
        animator.view.framePrepared != 1U ||
        animator.view.geometryActive != 1U) {
        return 0;
    }

    index = slotForLine((uint16_t)lineIndex);
    if (index < 0) return 0;
    *outDisplacement = (int16_t)(
        animator.slots[index].position * ESP_NATIVE_DOOR_ANIMATION_STEP);
    return 1;
}

const EspNativeDoorAnimatorView* EspNativeDoorAnimator_view(void) {
    return &animator.view;
}

EspMapLineDoorStatus __wrap_EspMapLineState_applyDoorCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineDoorResult* outResult) {
    EspMapLineDoorStatus status =
        __real_EspMapLineState_applyDoorCommand(descriptor,
                                                commandOffset,
                                                outResult);
    EspNativeDoorAnimationStatus animationStatus;

    if (status != ESP_MAP_LINE_DOOR_OK || outResult == NULL ||
        outResult->mutated != 1U || !EspNativeResidentGameplay_isActive()) {
        return status;
    }

    animationStatus = EspNativeDoorAnimator_begin(outResult->lineIndex,
                                                  outResult->openBefore,
                                                  outResult->openAfter);
    if (animationStatus == ESP_NATIVE_DOOR_ANIMATION_INVALID) {
        clearBatchPreserveCounters();
        if (!EspMapLineState_setOpen(outResult->lineIndex,
                                     outResult->openBefore)) {
            return ESP_MAP_LINE_DOOR_INVALID;
        }
        outResult->openAfter = outResult->openBefore;
        outResult->mutated = 0U;
        outResult->effectFlags = 0U;
        outResult->soundId = 0U;
        printf("[DOORANIM] FAILED line=%u reason=schedule rollback=open-%u\n",
               (unsigned int)outResult->lineIndex,
               (unsigned int)outResult->openBefore);
        return ESP_MAP_LINE_DOOR_INVALID;
    }

    return status;
}
