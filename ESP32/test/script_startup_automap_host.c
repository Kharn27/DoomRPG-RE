/* Real resident SELECT, automap mode and restoration routing. Script startup,
 * renderer/assets and external gameplay owners are mocked; the credit/debit
 * transaction itself is covered separately by paid_interaction_host.c. */
#include <assert.h>
#include <stdio.h>
#include "../src/esp_native_resident_gameplay.c"

static EspPlayerViewState testView;
static EspNativeGameplayHudState testHud;
static DoomRPG_t testGame;
static Render_t testRender;
static EspNativeGameplayInputState testIntent;
static EspNativeGameplayDialogResumeStatus startupStatus;
static int worldFrames, automapFrames, chainCalls;
static int worldOk, automapOk;

/* Disable-gameplay teardown is expected only when restoration itself fails. */
int EspNativeGameplayControls_isActive(void) { return 0; }
int EspNativeGameplayControls_restore(int present, EspNativeGameplayControlsStats* out) {
    (void)present; (void)out; assert(0); return 0;
}
void EspNativeGameplayHub_reset(void) {}
void EspNativeGameplayDialog_reset(void) {}
void EspNativeGameplayLevelUp_reset(void) {}
void EspNativeGameplayPassword_reset(void) {}
void EspNativeGameplayStore_reset(void) {}
void PlatformInput_setTapCallback(PlatformTapCallback callback) { assert(callback == NULL); }

/* Unselected branches must not accidentally advance a turn or use another
 * modal/transaction owner. These stubs deliberately fail on any invocation. */
int EspMapRuntime_getEvent(uint32_t index, uint32_t* out) {
    (void)index; (void)out; assert(0); return 0;
}
int EspMapEvents_describe(const EspMapEventRef* ref, EspMapEventDescriptor* out) {
    (void)ref; (void)out; assert(0); return 0;
}
int EspMapEvents_getCommand(const EspMapEventDescriptor* descriptor, uint32_t offset,
                            EspMapByteCode* out) {
    (void)descriptor; (void)offset; (void)out; assert(0); return 0;
}
int EspMapRuntime_getLine(uint32_t index, EspMapLine* out) {
    (void)index; (void)out; assert(0); return 0;
}
int EspNativeGameplayActionEngine_queueTextFeedback(EspNativeGameplayActionFeedback kind,
                                                   const char* text, uint16_t flash) {
    (void)kind; (void)text; (void)flash; assert(0); return 0;
}
int EspNativeGameplayActionEngine_cancelQueuedFeedback(EspNativeGameplayActionFeedback kind) {
    (void)kind; assert(0); return 0;
}
int EspNativeGameplayDialog_rollbackResume(const EspNativeGameplayDialogResumeResult* result) {
    (void)result; assert(0); return 0;
}
int EspNativeGameplayEventChain_commit(const EspNativeGameplayDialogResumeResult* result) {
    (void)result; assert(0); return 0;
}
int EspNativeGameplayStore_begin(uint8_t store, uint16_t event, uint8_t offset) {
    (void)store; (void)event; (void)offset; assert(0); return 0;
}
EspNativeGameplayPasswordBeginStatus EspNativeGameplayPassword_begin(
    uint16_t event, uint8_t offset, uint32_t flags) {
    (void)event; (void)offset; (void)flags; assert(0);
    return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
}
const char* EspNativeGameplayPassword_beginStatusName(EspNativeGameplayPasswordBeginStatus status) {
    (void)status; assert(0); return "unreachable";
}
EspNativeGameplayDialogBeginStatus EspNativeGameplayDialog_begin(
    uint16_t event, uint8_t offset, uint32_t flags) {
    (void)event; (void)offset; (void)flags; assert(0);
    return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
}
const char* EspNativeGameplayDialog_beginStatusName(EspNativeGameplayDialogBeginStatus status) {
    (void)status; assert(0); return "unreachable";
}
int EspNativeGameplayTransition_isWaitingDoor(void) { assert(0); return 0; }
int EspNativeGameplayAction_rollbackSelect(const EspNativeGameplayActionResult* result) {
    (void)result; assert(0); return 0;
}
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) {
    (void)out; assert(0); return 0;
}
int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState* state) {
    (void)state; assert(0); return 0;
}
int EspNativeGameplayPlayerState_applyXp(DoomRPG_t* game, uint32_t xp,
                                        EspNativeGameplayPlayerXpResult* out) {
    (void)game; (void)xp; (void)out; assert(0); return 0;
}
int EspNativeGameplayTransition_abortDoor(uint32_t seq, uint16_t event, uint16_t line) {
    (void)seq; (void)event; (void)line; assert(0); return 0;
}
EspNativeGameplayTransitionStatus EspNativeGameplayTransition_finishDoor(
    uint32_t seq, uint16_t event, uint16_t line) {
    (void)seq; (void)event; (void)line; assert(0);
    return ESP_NATIVE_GAMEPLAY_TRANSITION_INVALID;
}
const char* EspNativeGameplayTransition_statusName(EspNativeGameplayTransitionStatus status) {
    (void)status; assert(0); return "unreachable";
}
int EspNativeGameplayMonsterTurn_requestSelectDoor(uint32_t seq) {
    (void)seq; assert(0); return 0;
}
const EspNativeDoorAnimatorView* EspNativeDoorAnimator_view(void) { assert(0); return NULL; }
int EspNativeDoorAnimator_validateLineState(void) { assert(0); return 0; }
int EspNativeDoorAnimator_prepareFrame(EspNativeDoorAnimationFrame* out) {
    (void)out; assert(0); return 0;
}
void EspNativeDoorAnimator_reset(void) { assert(0); }
int EspNativeDoorAnimator_finishFrame(int ok) { (void)ok; assert(0); return 0; }

const EspPlayerViewState* EspPlayerView_view(void) { return &testView; }
const EspNativeGameplayHudState* EspNativeGameplayHud_view(void) { return &testHud; }
int EspNativeGameplayFacingLabel_refresh(const char* reason) { (void)reason; return 1; }
int EspNativeGameplayFrame_renderTurn(Render_t* render, uint8_t angle,
                                     EspNativeGameplayFrameStats* out) {
    assert(render == &testRender && angle == testView.viewAngle);
    ++worldFrames;
    memset(out, 0, sizeof(*out));
    return worldOk;
}
EspNativeGameplayHudStatus EspNativeGameplayHud_repaint(
    const EspNativeGameplayHudState* state, EspNativeGameplayHudStats* out) {
    assert(state == &testHud);
    memset(out, 0, sizeof(*out));
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}
EspNativeGameplayActionStatus EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent, EspNativeGameplayActionResult* out) {
    assert(intent == &testIntent);
    memset(out, 0, sizeof(*out));
    out->eventIndex = 67U;
    out->commandOffset = 9U;
    out->codeId = 23U;
    return ESP_NATIVE_GAMEPLAY_ACTION_CHAIN_READY;
}
const char* EspNativeGameplayAction_statusName(EspNativeGameplayActionStatus status) {
    (void)status; return "CHAIN_READY";
}
EspNativeGameplayDialogResumeStatus EspNativeGameplayEventChain_execute(
    uint16_t event, uint8_t offset, uint32_t flags,
    EspNativeGameplayDialogResumeResult* out) {
    assert(event == 67U && offset == 9U && flags == ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS);
    assert(!automapActive()); /* Startup happens after successful close. */
    ++chainCalls;
    memset(out, 0, sizeof(*out));
    return startupStatus;
}
const char* EspNativeGameplayDialog_resumeStatusName(EspNativeGameplayDialogResumeStatus status) {
    (void)status; return "mocked-startup";
}
int EspNativeGameplayAutomap_uncoverAt(int32_t x, int32_t y, uint16_t* mutated) {
    assert(x == testView.destX && y == testView.destY);
    assert(automapActive());
    *mutated = 0U; return 1;
}
int EspNativeBspVisibility_build(Render_t* render, EspNativeBspVisibilityState* out) {
    assert(render == &testRender); (void)out; return 1;
}
int EspNativeBspVisibility_publishAutomap(const EspNativeBspVisibilityState* visibility,
                                        uint16_t* lines, uint16_t* sprites) {
    (void)visibility; *lines = *sprites = 0U; return 1;
}
int EspNativeGameplayAutomap_render(int32_t x, int32_t y, uint8_t angle,
                                   EspNativeGameplayAutomapStats* out) {
    assert(x == testView.viewX && y == testView.viewY && angle == testView.viewAngle);
    assert(automapActive());
    ++automapFrames;
    memset(out, 0, sizeof(*out));
    return automapOk;
}
int EspNativeDoorAnimator_hasPendingFrames(void) { return 0; }
void EspNativeGameplayMoveEvents_onFrameResult(int ok) { (void)ok; }

static void seed(int inAutomap, EspNativeGameplayDialogResumeStatus status) {
    memset(&gameplayState, 0, sizeof(gameplayState));
    memset(&testGame, 0, sizeof(testGame));
    memset(&testRender, 0, sizeof(testRender));
    memset(&testView, 0, sizeof(testView));
    memset(&testIntent, 0, sizeof(testIntent));
    gameplayState.active = 1U;
    /* Preserve unrelated mode bits when restoring automap. */
    gameplayState.modeFlags = (uint8_t)(0x80U | (inAutomap ? RESIDENT_MODE_AUTOMAP : 0U));
    testView.active = 1U;
    testView.viewAngle = testView.destAngle = 64;
    testView.viewX = testView.destX = 224;
    testView.viewY = testView.destY = 1312;
    startupStatus = status;
    worldFrames = automapFrames = chainCalls = 0;
    worldOk = automapOk = 1;
}

int main(void) {
    const EspNativeGameplayDialogResumeStatus failures[] = {
        ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED,
        ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID
    };
    for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        seed(1, failures[i]);
        serviceSelect(&testGame, &testRender, &testIntent);
        assert(automapActive() && gameplayState.modeFlags == 0x81U);
        assert(gameplayState.active && !gameplayState.failed);
        assert(worldFrames == 1 && automapFrames == 1 && chainCalls == 1);
        assert(gameplayState.deferred == 1 && gameplayState.selects == 0);

        seed(0, failures[i]);
        serviceSelect(&testGame, &testRender, &testIntent);
        assert(!automapActive() && gameplayState.modeFlags == 0x80U);
        assert(gameplayState.active && !gameplayState.failed);
        assert(worldFrames == 1 && automapFrames == 0 && chainCalls == 1);
    }
    seed(1, ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED);
    automapOk = 0;
    serviceSelect(&testGame, &testRender, &testIntent);
    assert(automapActive() && gameplayState.failed && !gameplayState.active);
    assert(worldFrames == 1 && automapFrames == 1 && chainCalls == 1);

    seed(0, ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED);
    worldOk = 0;
    serviceSelect(&testGame, &testRender, &testIntent);
    assert(!automapActive() && gameplayState.failed && !gameplayState.active);

    seed(1, ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED);
    worldOk = 0; /* Close failed: keep mode and don't start/debit the script. */
    serviceSelect(&testGame, &testRender, &testIntent);
    assert(automapActive() && gameplayState.active && !gameplayState.failed);
    assert(chainCalls == 0 && automapFrames == 0 && gameplayState.deferred == 1);

    seed(1, ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED);
    serviceSelect(&testGame, &testRender, &testIntent);
    assert(!automapActive() && gameplayState.active && !gameplayState.failed);
    assert(worldFrames == 1 && automapFrames == 0 && chainCalls == 1);
    assert(gameplayState.selects == 1 && gameplayState.deferred == 0);

    puts("script_startup_automap_host: PASS automap/world failure restore, failed restore/close, successful pause");
    return 0;
}
