#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_move_events.h"

#define RENDER_LINE_FLAG_REGULAR_DOOR  0x00000004UL
#define RENDER_LINE_FLAG_AXIS_X        0x00000008UL
#define RENDER_LINE_FLAG_X_NUDGE       0x00000200UL
#define RENDER_LINE_FLAG_REVERSE_TEX   0x00008000UL
#define RENDER_LINE_FLAG_SPRITE_SPAN   0x00000002UL
#define RENDER_LINE_FLAG_OCCLUDER_ONLY 0x20000000UL

typedef struct AnimatedClipContext_s {
    int32_t builtZ1;
    int32_t builtZ2;
    int32_t desiredZ1;
    int32_t desiredZ2;
    uint16_t lineIndex;
    uint8_t valid;
    uint8_t reserved;
} AnimatedClipContext;

/*
 * The historical first-frame renderer was validated while every native line
 * was closed and therefore still rejects lineState.openCount != 0. Production
 * gameplay owns dynamic door state and now a tiny regular-door animation
 * overlay, so adapt only the renderer's read view while one complete gameplay
 * frame is being composed.
 *
 * No source/runtime data is mutated. Fully-open lines are copied from the
 * immutable arena and marked as renderer-only non-wall spans. Mutable door
 * texture variants are overlaid from EspMapLineTextureState on that copy, so
 * an EV_UNLOCK can change the red/green indicator without touching the arena.
 * During a regular legacy door animation the compact copy also receives the
 * recovered x/y displacement, while Render_clipLine receives the matching
 * transient texture z coordinates. The same scoped view is seen by world
 * raster and BSP sprite depth because both run inside renderTurn().
 */
static uint8_t dynamicFrameActive;
static uint8_t dynamicAnimationFault;
static uint32_t dynamicOpenLineReads;
static uint32_t dynamicAnimatedLineReads;
static uint32_t dynamicTextureVariantReads;
static EspMapLineStateView renderLineStateView;
static AnimatedClipContext animatedClip;

int __real_EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);
int __real_EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine);
const EspMapLineStateView* __real_EspMapLineState_view(void);
boolean __real_Render_clipLine(Render_t* render, Line_t* line);

static void compactLineZ(const EspMapLine* line,
                         int32_t* outZ1,
                         int32_t* outZ2) {
    int32_t dx;
    int32_t dy;
    int32_t extent;

    if (outZ1 != NULL) *outZ1 = 0;
    if (outZ2 != NULL) *outZ2 = 0;
    if (line == NULL || outZ1 == NULL || outZ2 == NULL) return;

    dx = (int32_t)line->x2 - (int32_t)line->x1;
    if (dx < 0) dx = -dx;
    dy = (int32_t)line->y2 - (int32_t)line->y1;
    if (dy < 0) dy = -dy;
    extent = dx > dy ? dx : dy;

    if ((line->flags & RENDER_LINE_FLAG_REVERSE_TEX) == 0U) {
        *outZ1 = 0;
        *outZ2 = extent;
    }
    else {
        *outZ1 = extent;
        *outZ2 = 0;
    }
}

static int adjustU16(uint16_t* value, int32_t delta) {
    int32_t next;
    if (value == NULL) return 0;
    next = (int32_t)*value + delta;
    if (next < 0 || next > UINT16_MAX) return 0;
    *value = (uint16_t)next;
    return 1;
}

static int applyAnimatedLine(uint32_t index,
                             EspMapLine* line,
                             int16_t displacement) {
    EspMapLine source;
    int32_t desiredZ1;
    int32_t desiredZ2;
    int32_t builtZ1;
    int32_t builtZ2;
    int32_t delta = (int32_t)displacement;

    if (line == NULL || delta < 0 ||
        delta > (int32_t)(ESP_NATIVE_DOOR_ANIMATION_MOVING_FRAMES *
                          ESP_NATIVE_DOOR_ANIMATION_STEP) ||
        (line->flags & RENDER_LINE_FLAG_REGULAR_DOOR) == 0U) {
        return 0;
    }

    source = *line;
    compactLineZ(&source, &desiredZ1, &desiredZ2);

    if ((source.flags & RENDER_LINE_FLAG_X_NUDGE) != 0U) {
        if ((source.flags & RENDER_LINE_FLAG_AXIS_X) != 0U) {
            if (!adjustU16(&line->y1, delta) ||
                !adjustU16(&line->x2, -delta)) {
                return 0;
            }
        }
        else {
            if (!adjustU16(&line->y1, delta)) return 0;
            desiredZ2 -= delta;
        }
    }
    else if ((source.flags & RENDER_LINE_FLAG_AXIS_X) != 0U) {
        if (!adjustU16(&line->x1, delta)) return 0;
        desiredZ2 -= delta;
    }
    else {
        if (!adjustU16(&line->x1, delta) ||
            !adjustU16(&line->y2, -delta)) {
            return 0;
        }
    }

    compactLineZ(line, &builtZ1, &builtZ2);
    animatedClip.builtZ1 = builtZ1;
    animatedClip.builtZ2 = builtZ2;
    animatedClip.desiredZ1 = desiredZ1;
    animatedClip.desiredZ2 = desiredZ2;
    animatedClip.lineIndex = (uint16_t)index;
    animatedClip.valid = 1U;
    return 1;
}

int __wrap_EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine) {
    uint8_t open;
    uint16_t effectiveTexture;
    int16_t displacement;

    animatedClip.valid = 0U;
    if (!__real_EspMapRuntime_getLine(index, outLine)) return 0;
    if (!dynamicFrameActive) return 1;
    if (!EspMapLineState_getOpen(index, &open)) return 0;

    if (EspMapLineTextureState_isReady()) {
        if (!EspMapLineTextureState_getEffectiveTexture(index,
                                                        &effectiveTexture)) {
            dynamicAnimationFault = 1U;
            return 0;
        }
        if (outLine->texture != effectiveTexture) {
            outLine->texture = effectiveTexture;
            ++dynamicTextureVariantReads;
        }
    }

    if (EspNativeDoorAnimator_getLineDisplacement(index, &displacement)) {
        if (!applyAnimatedLine(index, outLine, displacement)) {
            dynamicAnimationFault = 1U;
            return 0;
        }
        ++dynamicAnimatedLineReads;
        return 1;
    }

    if (open == 0U) return 1;
    outLine->flags &= ~RENDER_LINE_FLAG_OCCLUDER_ONLY;
    outLine->flags |= RENDER_LINE_FLAG_SPRITE_SPAN;
    ++dynamicOpenLineReads;
    return 1;
}

boolean __wrap_Render_clipLine(Render_t* render, Line_t* line) {
    if (dynamicFrameActive && animatedClip.valid != 0U && line != NULL) {
        if (line->vert1.z == animatedClip.builtZ1 &&
            line->vert2.z == animatedClip.builtZ2) {
            line->vert1.z = animatedClip.desiredZ1;
            line->vert2.z = animatedClip.desiredZ2;
        }
        else if (line->vert1.z == animatedClip.builtZ2 &&
                 line->vert2.z == animatedClip.builtZ1) {
            line->vert1.z = animatedClip.desiredZ2;
            line->vert2.z = animatedClip.desiredZ1;
        }
        else if (line->vert1.z == 0 && line->vert2.z == 0) {
            /* EspNativeBspVisibility intentionally omits texture-z because its
             * sprite-depth pass consumes only transformed x/y. The compact x/y
             * door displacement above still applies there; no z fix is needed. */
        }
        else {
            dynamicAnimationFault = 1U;
            printf("[DOORANIM] GEOMETRY-FAULT line=%u builtZ=%d,%d observedZ=%d,%d\n",
                   (unsigned int)animatedClip.lineIndex,
                   (int)animatedClip.builtZ1,
                   (int)animatedClip.builtZ2,
                   (int)line->vert1.z,
                   (int)line->vert2.z);
        }
        animatedClip.valid = 0U;
    }
    return __real_Render_clipLine(render, line);
}

const EspMapLineStateView* __wrap_EspMapLineState_view(void) {
    const EspMapLineStateView* real = __real_EspMapLineState_view();

    if (!dynamicFrameActive || real == NULL) return real;
    renderLineStateView = *real;
    /* The renderer's old zero-open gate was a fixed-pose witness, not a
     * production semantic. Per-line dynamic adaptation above owns openness. */
    renderLineStateView.openCount = 0U;
    return &renderLineStateView;
}

static int renderDynamicFrame(struct Render_s* render,
                              uint8_t angle,
                              EspNativeGameplayFrameStats* outStats,
                              const EspNativeDoorAnimationFrame* animationFrame) {
    const EspMapLineStateView* realBefore = __real_EspMapLineState_view();
    uint32_t openCount = realBefore != NULL ? realBefore->openCount : 0U;
    int ok;

    dynamicOpenLineReads = 0U;
    dynamicAnimatedLineReads = 0U;
    dynamicTextureVariantReads = 0U;
    dynamicAnimationFault = 0U;
    memset(&animatedClip, 0, sizeof(animatedClip));
    dynamicFrameActive = 1U;
    ok = __real_EspNativeGameplayFrame_renderTurn(render, angle, outStats);
    dynamicFrameActive = 0U;
    animatedClip.valid = 0U;

    if (dynamicAnimationFault != 0U) ok = 0;

    if (animationFrame != NULL) {
        printf("[DOORANIM] FRAME %u/%u angle=%u lines=%u geometry=%s animatedReads=%u openReads=%u textureVariants=%u frame=%08x render=%s\n",
               (unsigned int)animationFrame->ordinal,
               (unsigned int)animationFrame->totalFrames,
               (unsigned int)angle,
               (unsigned int)animationFrame->activeLines,
               animationFrame->geometryActive != 0U ? "moving" : "stable",
               (unsigned int)dynamicAnimatedLineReads,
               (unsigned int)dynamicOpenLineReads,
               (unsigned int)dynamicTextureVariantReads,
               outStats != NULL ? (unsigned int)outStats->frameAfterFNV : 0U,
               ok ? "ok" : "failed");
    }

    if (openCount != 0U || dynamicOpenLineReads != 0U ||
        dynamicAnimatedLineReads != 0U || dynamicTextureVariantReads != 0U) {
        printf("[DYNAMICLINES] FRAME angle=%u open=%u adaptedReads=%u animatedReads=%u textureVariants=%u render=%s immutableRuntime=yes\n",
               (unsigned int)angle,
               (unsigned int)openCount,
               (unsigned int)dynamicOpenLineReads,
               (unsigned int)dynamicAnimatedLineReads,
               (unsigned int)dynamicTextureVariantReads,
               ok ? "ok" : "failed");
    }
    return ok;
}

int __wrap_EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats) {
    int ok;

    if (dynamicFrameActive) return 0;

    if (EspNativeDoorAnimator_hasPendingFrames()) {
        const EspNativeDoorAnimatorView* before = EspNativeDoorAnimator_view();
        uint32_t completedBefore =
            before != NULL ? before->completedTransitions : 0U;

        if (!EspNativeDoorAnimator_validateLineState()) {
            printf("[DOORANIM] LEASE canceled-before-render; presenting stable state\n");
        }

        while (EspNativeDoorAnimator_hasPendingFrames()) {
            EspNativeDoorAnimationFrame animationFrame;
            memset(&animationFrame, 0, sizeof(animationFrame));
            if (!EspNativeDoorAnimator_prepareFrame(&animationFrame)) {
                EspNativeDoorAnimator_reset();
                EspNativeGameplayMoveEvents_onFrameResult(0);
                printf("[DOORANIM] FAILED reason=prepare-frame\n");
                return 0;
            }

            ok = renderDynamicFrame(render, angle, outStats, &animationFrame);
            if (!EspNativeDoorAnimator_finishFrame(ok)) {
                EspNativeDoorAnimator_reset();
                EspNativeGameplayMoveEvents_onFrameResult(0);
                printf("[DOORANIM] FAILED reason=finish-frame\n");
                return 0;
            }
            if (!ok) {
                EspNativeGameplayMoveEvents_onFrameResult(0);
                EspNativeDoorAnimator_reset();
                return 0;
            }
        }

        EspNativeGameplayMoveEvents_onFrameResult(1);
        {
            const EspNativeDoorAnimatorView* after = EspNativeDoorAnimator_view();
            uint32_t completedAfter =
                after != NULL ? after->completedTransitions : completedBefore;
            printf("[DOORANIM] COMPLETE transitions=%u frames=%u state=stable transaction=committed\n",
                   (unsigned int)(completedAfter - completedBefore),
                   (unsigned int)ESP_NATIVE_DOOR_ANIMATION_FRAMES);
        }
        return 1;
    }

    ok = renderDynamicFrame(render, angle, outStats, NULL);
    EspNativeGameplayMoveEvents_onFrameResult(ok);
    return ok;
}
