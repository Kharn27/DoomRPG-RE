from pathlib import Path

p = Path("ESP32/src/esp_native_first_frame.c")
s = p.read_text()

old = """static EspNativeFirstFrameState frameState;
/* Failure-only BSS witness. It deliberately adds no fields to FirstFrameWork,
 * whose large automatic instance is already close to the classic-CYD loopTask
 * stack boundary. The witness is printed only after renderFrame() has returned
 * and its BSP/raster stack has fully unwound. */
static FirstFrameFailure frameFailure;"""
new = """static EspNativeFirstFrameState frameState;
/* renderFrame() is reachable below gameplay/combat. Its large bounded work and
 * render-save scratch must not live on loopTask stack. Single-threaded native
 * rendering permits one non-reentrant BSS workspace. */
static FirstFrameWork frameWorkScratch;
static RenderScratch frameRenderScratch;
static uint8_t frameRenderBusy;
static uint8_t frameScratchLogged;
/* Failure-only BSS witness, printed after renderer work has unwound. */
static FirstFrameFailure frameFailure;"""
assert s.count(old) == 1
s = s.replace(old, new, 1)

a = s.index("static int renderFrame(Render_t* render,")
b = s.index("\nstatic int renderFrameWithLegacyGuardRecovery(", a)
f = s[a:b]
assert f.count("    FirstFrameWork work;\n    RenderScratch scratch;\n") == 1
f = f.replace(
    "    FirstFrameWork work;\n    RenderScratch scratch;\n",
    "    FirstFrameWork* work = &frameWorkScratch;\n"
    "    RenderScratch* scratch = &frameRenderScratch;\n", 1)

old = """    lineState = EspMapLineState_view();
    if (lineState == NULL || lineState->openCount != 0U) return 0;

    memset(&work, 0, sizeof(work));
    work.render = render;
    work.runtime = EspMapRuntime_view();
    resourceName = EspMapCatalog_nameForId(playerView->targetMapId);
    if (work.runtime == NULL || work.runtime->lineCount == 0U ||
        work.runtime->nodeCount == 0U || work.runtime->sourceBytes == 0U ||
        work.runtime->sourceCrc32 == 0U || resourceName == NULL) return 0;

    saveRenderScratch(render, &scratch);
"""
new = """    lineState = EspMapLineState_view();
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
"""
assert f.count(old) == 1
f = f.replace(old, new, 1)
f = f.replace("work.", "work->").replace("&work", "work").replace("&scratch", "scratch")

old = """done:
    releaseCache(work);
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    restoreRenderScratch(render, scratch);
    return ok;
}"""
new = """done:
    releaseCache(work);
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    restoreRenderScratch(render, scratch);
    frameRenderBusy = 0U;
    return ok;
}"""
assert f.count(old) == 1
f = f.replace(old, new, 1)
s = s[:a] + f + s[b:]

old = """void EspNativeFirstFrame_reset(void) {
    memset(&frameState, 0, sizeof(frameState));
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
}"""
new = """void EspNativeFirstFrame_reset(void) {
    memset(&frameState, 0, sizeof(frameState));
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    if (frameRenderBusy == 0U) {
        memset(&frameWorkScratch, 0, sizeof(frameWorkScratch));
        memset(&frameRenderScratch, 0, sizeof(frameRenderScratch));
        frameScratchLogged = 0U;
    }
}"""
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)
