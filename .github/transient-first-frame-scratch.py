from pathlib import Path

c = Path('ESP32/src/esp_native_first_frame.c')
s = c.read_text()

old = '''static EspNativeFirstFrameState frameState;
/* renderFrame() is reachable below gameplay/combat. Its large bounded work and
 * render-save scratch must not live on loopTask stack. Single-threaded native
 * rendering permits one non-reentrant BSS workspace. */
static FirstFrameWork frameWorkScratch;
static RenderScratch frameRenderScratch;
static uint8_t frameRenderBusy;
static uint8_t frameScratchLogged;
/* Failure-only BSS witness, printed after renderer work has unwound. */
static FirstFrameFailure frameFailure;'''
new = '''static EspNativeFirstFrameState frameState;
static uint8_t frameScratchLogged;
/* Failure-only BSS witness, printed after renderer work has unwound. */
static FirstFrameFailure frameFailure;'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''    FirstFrameWork* work = &frameWorkScratch;
    RenderScratch* scratch = &frameRenderScratch;
    EspAssetPackEntry mappings;'''
new = '''    FirstFrameWork* work = NULL;
    RenderScratch scratch;
    const EspMapRuntimeView* runtime;
    EspAssetPackEntry mappings;'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''    lineState = EspMapLineState_view();
    if (lineState == NULL || lineState->openCount != 0U ||
        frameRenderBusy != 0U) return 0;

    memset(work, 0, sizeof(*work));
    work->render = render;
    work->runtime = EspMapRuntime_view();
    resourceName = EspMapCatalog_nameForId(playerView->targetMapId);
    if (work->runtime == NULL || work->runtime->lineCount == 0U ||
        work->runtime->nodeCount == 0U || work->runtime->sourceBytes == 0U ||
        work->runtime->sourceCrc32 == 0U || resourceName == NULL) return 0;

    frameRenderBusy = 1U;
    saveRenderScratch(render, scratch);
    if (frameScratchLogged == 0U) {
        printf("[NATIVEFRAME] SCRATCH owner=BSS bytes=%u work=%u render=%u stack=bounded reentrant=no\\n",
               (unsigned int)(sizeof(frameWorkScratch) + sizeof(frameRenderScratch)),
               (unsigned int)sizeof(frameWorkScratch),
               (unsigned int)sizeof(frameRenderScratch));
        frameScratchLogged = 1U;
    }
'''
new = '''    lineState = EspMapLineState_view();
    if (lineState == NULL || lineState->openCount != 0U) return 0;

    runtime = EspMapRuntime_view();
    resourceName = EspMapCatalog_nameForId(playerView->targetMapId);
    if (runtime == NULL || runtime->lineCount == 0U ||
        runtime->nodeCount == 0U || runtime->sourceBytes == 0U ||
        runtime->sourceCrc32 == 0U || resourceName == NULL) return 0;

    work = (FirstFrameWork*)calloc(1U, sizeof(*work));
    if (work == NULL) {
        printf("[NATIVEFRAME] SCRATCH-ALLOC failed bytes=%u owner=heap-transient\\n",
               (unsigned int)sizeof(*work));
        return 0;
    }
    work->render = render;
    work->runtime = runtime;
    saveRenderScratch(render, &scratch);
    if (frameScratchLogged == 0U) {
        printf("[NATIVEFRAME] SCRATCH owner=heap-transient bytes=%u stackRender=%u lifetime=one-render\\n",
               (unsigned int)sizeof(*work),
               (unsigned int)sizeof(scratch));
        frameScratchLogged = 1U;
    }
'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''done:
    releaseCache(work);
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    restoreRenderScratch(render, scratch);
    frameRenderBusy = 0U;
    return ok;
}'''
new = '''done:
    releaseCache(work);
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    restoreRenderScratch(render, &scratch);
    free(work);
    return ok;
}'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''void EspNativeFirstFrame_reset(void) {
    memset(&frameState, 0, sizeof(frameState));
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    if (frameRenderBusy == 0U) {
        memset(&frameWorkScratch, 0, sizeof(frameWorkScratch));
        memset(&frameRenderScratch, 0, sizeof(frameRenderScratch));
        frameScratchLogged = 0U;
    }
}'''
new = '''void EspNativeFirstFrame_reset(void) {
    memset(&frameState, 0, sizeof(frameState));
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    frameScratchLogged = 0U;
}'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

c.write_text(s)

h = Path('ESP32/include/esp_native_first_frame.h')
t = h.read_text()
old = ''' * historical first-frame state. The implementation uses one bounded,
 * non-reentrant BSS work/render scratch so deep gameplay/combat call chains do
 * not place the large wall resolver and column-scale save on loopTask stack.
 * The caller receives the local render witness in outState and owns the later
'''
new = ''' * historical first-frame state. The large wall-resolution workspace is a
 * bounded transient heap scratch with one-render lifetime, keeping it off the
 * deep gameplay/combat loopTask stack without reserving permanent BSS RAM.
 * The smaller column-scale save remains stack-local. Allocation failure is a
 * fail-closed render failure. The caller receives the local render witness and
'''
assert t.count(old) == 1
t = t.replace(old, new, 1)
h.write_text(t)
