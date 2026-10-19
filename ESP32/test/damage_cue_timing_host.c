/* Exercise the real FIFO/compositor/expiry code, not the pure blend fixture.
 * Clock, physical present, PAK/font, touch and level-up are host seams. */
#include <assert.h>
#include "../src/esp_native_gameplay_action_engine.c"

static uint16_t pixels[160U * 120U + 2U];
static uint32_t clockMs;
static int controls, levelUp, pack, failPresent, automapOwner, worldActive, facingDirty;
static unsigned topBarTiles;
static unsigned presents, concurrentPresents, damageGlyphs, groupedTextPresents;
unsigned int DoomRPG_GetUpTimeMS(void) { return clockMs; }
const EspPlayerViewState* EspPlayerView_view(void) { return NULL; }
void* Esp32PlatformVideo_framebuffer(void) { return &pixels[1]; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return 160U * 120U * 2U; }
int EspNativeGameplayControls_isActive(void) { return controls; }
int EspNativeGameplayLevelUp_isActive(void) { return levelUp; }
/* This fixture drives the real in-world feedback compositor, not MENU_MAIN. */
int EspNativeResidentGameplay_isActive(void) { return worldActive; }
int EspNativeResidentGameplay_isAutomapActive(void) { return automapOwner; }
int EspAssetPack_isOpen(void) { return pack; }
int EspAssetPack_open(const char* path) { (void)path; assert(!pack); pack = 1; return 1; }
void EspAssetPack_close(void) { pack = 0; }
int EspNativeGameplayFacingLabel_isDirty(void) { return facingDirty; }
const EspNativeGameplayFacingLabelView* EspNativeGameplayFacingLabel_view(void) { return NULL; }
void EspNativeGameplayFacingLabel_markPainted(void) { facingDirty = 0; }
const EspMapStatusMessageState* EspNativeGameplayStatusMessage_view(void) { return NULL; }
int EspMapStatusMessage_isActive(const EspMapStatusMessageState* state) { (void)state; return 0; }
int EspNativeGameplayStatusMessage_repaintCurrent(void) { return 1; }
int EspNativeGameplayHud_paintTopTouchNotches(void) { return 1; }
EspNativeIndexedBmpStatus EspNativeIndexedBmp_open(
    const char* name, EspNativeIndexedBmp* bmp, EspNativeIndexedBmpStats* stats) {
    /* The original compositor tests used only k.bmp (HUD strip). Grouped
     * DAMAGE text also opens a.bmp (144x72 font): model both real contracts
     * instead of returning a 20x20 strip for every resource. */
    (void)stats;
    if (strcmp(name, "a.bmp") == 0) {
        bmp->width = FEEDBACK_FONT_SOURCE_WIDTH;
        bmp->height = FEEDBACK_FONT_SOURCE_HEIGHT;
    }
    else {
        assert(strcmp(name, "k.bmp") == 0);
        bmp->width = 20U;
        bmp->height = FEEDBACK_TOP_HEIGHT;
    }
    return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_tile(
    const EspNativeIndexedBmp* bmp, uint16_t* fb, uint16_t fw, uint16_t fh,
    int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t transparent,
    EspNativeIndexedBmpStats* stats) {
    (void)bmp; (void)fb; (void)fw; (void)fh; (void)x; (void)y;
    (void)transparent; (void)stats;
    if (x == 0 && y == 0 && w == 160U && h == FEEDBACK_TOP_HEIGHT)
        ++topBarTiles;
    return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
    const EspNativeIndexedBmp* bmp, uint16_t* fb, uint16_t fw, uint16_t fh,
    uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, int16_t x, int16_t y,
    uint8_t transparent, EspNativeIndexedBmpStats* stats) {
    (void)bmp; (void)fb; (void)fw; (void)fh; (void)sx; (void)sy;
    (void)w; (void)h; (void)x; (void)y; (void)transparent; (void)stats;
    if (actionState.feedbackPending &&
        actionState.feedbackKind == ACTION_FEEDBACK_DAMAGE)
        ++damageGlyphs;
    return ESP_NATIVE_INDEXED_BMP_OK;
}
int __real_Esp32PlatformVideo_present(void) {
    ++presents;
    if (failPresent) { --failPresent; return 0; }
    unsigned x = 80U, y = 20U;
    if (directionCue.current.side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK) y = 99U;
    if (directionCue.current.side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT) { x = 0; y = 50; }
    if (directionCue.current.side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT) { x = 159; y = 50; }
    if (directionCue.snapshotValid && directionCue.phase == 1U &&
        pixels[1 + y * 160U + x] ==
            EspNativeGameplayDamageCue_blend565(0xb800U, DAMAGE_CUE_COLOR565)) {
        ++concurrentPresents;
        if (damageGlyphs != 0U) ++groupedTextPresents;
    }
    return 1;
}
static void seedUnpainted(void) {
    memset(&actionState, 0, sizeof(actionState));
    memset(&directionCue, 0, sizeof(directionCue));
    for (unsigned i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) pixels[i] = 0x1234;
    clockMs = 1000;
    controls = levelUp = pack = failPresent = automapOwner = facingDirty = 0;
    worldActive = 1;
    topBarTiles = 0;
    presents = concurrentPresents = damageGlyphs = groupedTextPresents = 0;
    actionState.viewportFlashPending = 1;
    actionState.viewportFlashColor565 = 0xb800;
    actionState.viewportFlashDurationMs = 500;
}
static void seed(void) {
    seedUnpainted();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(actionState.viewportFlashVisible && presents == 1);
}
static void poll(void) {
    /* Real production entry: no SELECT transaction, so only UI work runs.
     * -O2 specializes the NULL runtime to discard unreachable world branches. */
    assert(EspNativeGameplayActionEngine_service(NULL));
}
static void finish(void) {
    clockMs += DAMAGE_CUE_ON_MS;
    poll();
    clockMs += DAMAGE_CUE_GAP_MS;
    poll();
    clockMs += DAMAGE_CUE_ON_MS;
    poll();
    assert(directionCue.phase == 0U);
    assert(directionCue.observedOnMs[0] == DAMAGE_CUE_ON_MS);
    assert(directionCue.observedOnMs[1] == DAMAGE_CUE_ON_MS);
    assert(directionCue.observedGapMs == DAMAGE_CUE_GAP_MS);
    clockMs += 501;
    poll();
    assert(!actionState.viewportFlashVisible);
    for (unsigned i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) assert(pixels[i] == 0x1234);
}
int main(void) {
    seed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(5, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    /* Before any other monster/member runs, the first pulse is already ON. */
    assert(directionCue.phase == 2U && concurrentPresents == 1U);
    clockMs += 818; /* Original ranged witness: next service arrives too late. */
    poll();
    assert(actionState.viewportFlashVisible && directionCue.phase == 3U);
    assert(directionCue.observedOnMs[0] == 818U);
    assert(directionCue.observedOnMs[1] == 0U);
    clockMs += DAMAGE_CUE_GAP_MS;
    poll();
    clockMs += DAMAGE_CUE_ON_MS;
    poll();
    assert(directionCue.phase == 0U && !actionState.viewportFlashVisible);
    assert(directionCue.observedOnMs[0] == 818U);
    assert(directionCue.observedOnMs[1] == DAMAGE_CUE_ON_MS);
    assert(directionCue.observedGapMs == DAMAGE_CUE_GAP_MS);
    for (unsigned i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) assert(pixels[i] == 0x1234);

    /* Real CYD witness: after a monster hit, the synchronous 818ms turn tail
     * used to extend the already-lit first pulse. Start only after that tail;
     * do not wait for pulses or change attack/turn ordering. */
    seed();
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        14, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(directionCue.queued.count == 1U &&
           directionCue.phase == 0U && presents == 1U);
    clockMs += 818U; /* simulated heavy member/FX tail on loopTask */
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(directionCue.phase == 2U && directionCue.queued.count == 0U &&
           concurrentPresents == 1U && presents == 2U);
    finish();
    assert(directionCue.observedOnMs[0] == 85U &&
           directionCue.observedOnMs[1] == 85U);

    /* Real sprite-157 CYD regression: earlier correction moved the first side
     * ON past an ~800ms monster tail, but left the thin red flash visible for
     * that full wait. Nonlethal staging now atomically paints base+side after
     * the synchronous work, while the validated hit can render earlier. */
    seedUnpainted();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    memcpy(actionState.feedbackText, "8 damage!", sizeof("8 damage!"));
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    assert(directionCue.stageNonlethalBorder == 1U);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && damageGlyphs == 0U &&
           groupedTextPresents == 0U &&
           actionState.feedbackPending == 1U &&
           actionState.feedbackVisible == 0U &&
           actionState.feedbackShownAtMs == 0U &&
           actionState.viewportFlashPending &&
           !actionState.viewportFlashVisible &&
           !actionState.viewportFlashSnapshotValid);
    assert(pixels[1 + 20U * 160U + 80U] == 0x1234);
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        17, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(directionCue.queued.count == 1U && directionCue.phase == 0U);
    clockMs += 818U;
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    fprintf(stderr,
            "[DAMAGEGROUP-HOST] presents=%u concurrent=%u grouped=%u glyphs=%u feedbackVisibleKind=%u damageKind=%u feedbackVisible=%u feedbackPending=%u feedbackShown=%u clock=%u phase=%u flashVisible=%u flashShown=%u flashPending=%u snapshot=%u\n",
            presents, concurrentPresents, groupedTextPresents, damageGlyphs,
            actionState.feedbackVisibleKind, ACTION_FEEDBACK_DAMAGE,
            actionState.feedbackVisible, actionState.feedbackPending,
            actionState.feedbackShownAtMs, clockMs, directionCue.phase,
            actionState.viewportFlashVisible, actionState.viewportFlashShownAtMs,
            actionState.viewportFlashPending,
            actionState.viewportFlashSnapshotValid);
    assert(presents == 2U && concurrentPresents == 1U &&
           groupedTextPresents == 1U && damageGlyphs != 0U &&
           actionState.feedbackVisibleKind == ACTION_FEEDBACK_DAMAGE &&
           actionState.feedbackVisible == 1U &&
           actionState.feedbackPending == 0U &&
           actionState.feedbackShownAtMs == clockMs &&
           directionCue.phase == 2U && actionState.viewportFlashVisible &&
           actionState.viewportFlashShownAtMs == clockMs &&
           !actionState.viewportFlashPending &&
           actionState.viewportFlashSnapshotValid);
    assert(pixels[1 + 20U * 160U + 80U] ==
           EspNativeGameplayDamageCue_blend565(0xb800U, DAMAGE_CUE_COLOR565));
    finish(); /* First ON, second ON and GAP all start after 818ms tail. */
    assert(directionCue.observedOnMs[0] == DAMAGE_CUE_ON_MS &&
           directionCue.observedOnMs[1] == DAMAGE_CUE_ON_MS);
    /* No direction: both text and border must still appear, together. */
    seedUnpainted();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    memcpy(actionState.feedbackText, "7 damage!", sizeof("7 damage!"));
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(!actionState.viewportFlashVisible && presents == 1U &&
           damageGlyphs == 0U && actionState.feedbackPending == 1U);
    clockMs += 818U;
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(actionState.viewportFlashVisible && !actionState.viewportFlashPending &&
           actionState.feedbackPending == 0U &&
           actionState.feedbackVisibleKind == ACTION_FEEDBACK_DAMAGE &&
           damageGlyphs != 0U && directionCue.phase == 0U && presents == 2U);
    clockMs += 501U;
    poll();
    assert(!actionState.viewportFlashVisible);
    /* Lethal / non-staged DAMAGE keeps original immediate presentation. */
    seedUnpainted();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    memcpy(actionState.feedbackText, "9 damage!", sizeof("9 damage!"));
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(actionState.viewportFlashVisible && presents == 1U &&
           damageGlyphs != 0U && actionState.feedbackVisible == 1U &&
           actionState.feedbackPending == 0U);
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(presents == 1U);

    /* Every strict UI/transaction barrier defers paint, not intent or gameplay. */
    for (unsigned barrier = 0; barrier < 4U; ++barrier) {
        seed();
        if (barrier == 0) controls = 1;
        if (barrier == 1) levelUp = 1;
        if (barrier == 2) pack = 1;
        if (barrier == 3) actionState.pending.active = 1;
        assert(EspNativeGameplayActionEngine_queueDirectionalDamage(6, 159, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
        assert(directionCue.queued.count == 1U && directionCue.phase == 0U && presents == 1U);
        clockMs += 600;
        assert(serviceViewportFlashExpiry() && actionState.viewportFlashVisible);
        controls = levelUp = pack = 0;
        actionState.pending.active = 0;
        poll();
        assert(directionCue.phase == 2U && concurrentPresents == 1U);
        finish();
    }
    seed();
    failPresent = 1;
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(7, 159, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(directionCue.phase == 1U && directionCue.needsRepaint && concurrentPresents == 0U);
    clockMs += 600;
    poll();
    assert(directionCue.phase == 2U && concurrentPresents == 1U);
    finish();

    seed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(8, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(9, 159, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(directionCue.current.probe == 8 && directionCue.queued.count == 1);
    clockMs += DAMAGE_CUE_ON_MS; poll();
    clockMs += DAMAGE_CUE_GAP_MS; poll();
    clockMs += DAMAGE_CUE_ON_MS; poll();
    assert(directionCue.phase == 0U && directionCue.queued.count == 1);
    clockMs += 600;
    assert(serviceViewportFlashExpiry() && actionState.viewportFlashVisible);
    poll();
    assert(directionCue.current.probe == 9 && directionCue.phase == 2U);
    finish();

    for (unsigned side = 1; side <= 4U; ++side) {
        seed();
        assert(EspNativeGameplayActionEngine_queueDirectionalDamage(10, 157, (EspNativeGameplayDamageSide)side));
        assert(directionCue.phase == 2U && concurrentPresents == 1U);
        finish();
    }
    seed();
    clockMs = UINT32_MAX - 100U;
    actionState.viewportFlashShownAtMs = clockMs;
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(11, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    finish(); /* Cross clock wrap during the two pulses. */

    seed();
    failPresent = 100;
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(12, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    clockMs += DAMAGE_CUE_MAX_PHASE_MS;
    failPresent = 0;
    poll();
    assert(directionCue.phase == 0U && !actionState.viewportFlashVisible);
    for (unsigned i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) assert(pixels[i] == 0x1234);

    seed();
    actionState.pending.active = 1;
    for (unsigned i = 0; i < DAMAGE_CUE_QUEUE_MAX; ++i)
        assert(EspNativeGameplayActionEngine_queueDirectionalDamage(i + 20, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    DirectionalCueOwner beforeOverflow = directionCue;
    assert(!EspNativeGameplayActionEngine_queueDirectionalDamage(99, 159, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(memcmp(&beforeOverflow, &directionCue, sizeof(directionCue)) == 0 && presents == 1U);

    seed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(13, 157, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    /* Fresh world repaint during pulse: both snapshots must adopt new pixels. */
    for (unsigned y = 20; y < 100; ++y)
        for (unsigned x = 0; x < 160; ++x) pixels[1 + y * 160 + x] = 0x5678;
    EspNativeGameplayActionEngine_markFreshFrame();
    assert(__wrap_Esp32PlatformVideo_present());
    clockMs += DAMAGE_CUE_ON_MS; poll();
    clockMs += DAMAGE_CUE_GAP_MS; poll();
    clockMs += DAMAGE_CUE_ON_MS; poll();
    clockMs += 501; poll();
    assert(!actionState.viewportFlashVisible && directionCue.phase == 0U);
    assert(pixels[0] == 0x1234 && pixels[160U * 120U + 1U] == 0x1234);
    for (unsigned y = 0; y < 120; ++y)
        for (unsigned x = 0; x < 160; ++x)
            assert(pixels[1 + y * 160 + x] == (y >= 20 && y < 100 ? 0x5678 : 0x1234));
    /* Full-screen automap takes priority over all transient 3D effects.
     * Neither queued monster hits nor timed restores may touch map pixels. */
    seedUnpainted();
    automapOwner = 1;
    for (unsigned i = 1U; i < 160U * 120U + 1U; ++i) pixels[i] = 0x4567U;
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    strcpy(actionState.feedbackText, "5 damage!");
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && damageGlyphs == 0U);
    assert(!actionState.feedbackPending && !actionState.feedbackVisible);
    assert(!actionState.viewportFlashPending &&
           !actionState.viewportFlashVisible &&
           !actionState.viewportFlashSnapshotValid);
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        31U, 7U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(directionCue.phase == 0U && directionCue.queued.count == 0U);
    clockMs += 2000U;
    poll();
    assert(presents == 1U && damageGlyphs == 0U);
    for (unsigned i = 1U; i < 160U * 120U + 1U; ++i)
        assert(pixels[i] == 0x4567U);
    automapOwner = 0;
    poll();
    assert(presents == 1U && !actionState.feedbackVisible &&
           !actionState.viewportFlashVisible &&
           directionCue.phase == 0U);
    /* Real compositor wrapper regression: EXIT resets facing.dirty, but the
     * first full-frame SAVESLOTS present belongs to MENU_MAIN, not gameplay.
     * The old code retiled k.bmp onto the first 20 pixels rows, exactly once;
     * a second LOAD was clean because the dirty flag had been consumed. */
    seedUnpainted();
    actionState.viewportFlashPending = 0U;
    worldActive = 0;
    facingDirty = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 0U && facingDirty == 1 && presents == 1U);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 0U && facingDirty == 1 && presents == 2U);
    worldActive = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 1U && facingDirty == 0 && presents == 3U);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 1U && presents == 4U);
    puts("damage_cue_timing_host: PASS fullscreen AUTOMAP mutes transient damage UI without framebuffer writes; normal DAMAGE timing/restoration/failures unchanged; EXIT->LOAD compositor gate");
    return 0;
}
