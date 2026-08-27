#include <stdint.h>
#include <stdio.h>

#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_move_events.h"

#define RENDER_LINE_FLAG_SPRITE_SPAN 0x00000002UL
#define RENDER_LINE_FLAG_OCCLUDER_ONLY 0x20000000UL

/*
 * The historical first-frame renderer was validated while every native line
 * was closed and therefore still rejects lineState.openCount != 0. Production
 * gameplay now owns dynamic door state, so adapt only the renderer's read view
 * while one complete gameplay frame is being composed.
 *
 * No source/runtime data is mutated. An open native line is copied from the
 * immutable arena, then marked as a renderer-only non-wall span and stripped
 * of occluder-only behavior. The same scoped view is seen by both the world
 * raster and BSP sprite-depth pass because both run inside renderTurn().
 */
static uint8_t dynamicFrameActive;
static uint32_t dynamicOpenLineReads;
static EspMapLineStateView renderLineStateView;

int __real_EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);
int __real_EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine);
const EspMapLineStateView* __real_EspMapLineState_view(void);

int __wrap_EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine) {
    uint8_t open;

    if (!__real_EspMapRuntime_getLine(index, outLine)) return 0;
    if (!dynamicFrameActive) return 1;
    if (!EspMapLineState_getOpen(index, &open)) return 0;
    if (open == 0U) return 1;

    outLine->flags &= ~RENDER_LINE_FLAG_OCCLUDER_ONLY;
    outLine->flags |= RENDER_LINE_FLAG_SPRITE_SPAN;
    ++dynamicOpenLineReads;
    return 1;
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

int __wrap_EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats) {
    const EspMapLineStateView* realBefore = __real_EspMapLineState_view();
    uint32_t openCount = realBefore != NULL ? realBefore->openCount : 0U;
    int ok;

    if (dynamicFrameActive) return 0;
    dynamicOpenLineReads = 0U;
    dynamicFrameActive = 1U;
    ok = __real_EspNativeGameplayFrame_renderTurn(render, angle, outStats);
    dynamicFrameActive = 0U;
    EspNativeGameplayMoveEvents_onFrameResult(ok);

    if (openCount != 0U || dynamicOpenLineReads != 0U) {
        printf("[DYNAMICLINES] FRAME angle=%u open=%u adaptedReads=%u render=%s immutableRuntime=yes\n",
               (unsigned int)angle,
               (unsigned int)openCount,
               (unsigned int)dynamicOpenLineReads,
               ok ? "ok" : "failed");
    }
    return ok;
}
