/* Exercise the real FIFO/compositor/expiry code, not the pure blend fixture.
 * Clock, physical present, PAK/font, touch and level-up are host seams. */
#include <assert.h>
#include "../src/esp_native_gameplay_action_engine.c"

static uint16_t pixels[160U * 120U + 2U];
static uint32_t clockMs;
static int controls, levelUp, pack, failPresent, automapOwner, worldActive, firstWorldLease, facingDirty;
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
int EspNativeResidentGameplay_ownsWorldPresent(void) {
    return worldActive || firstWorldLease;
}
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
    if (failPresent != 0) { --failPresent; return 0; }
    if (directionCue.pendingValid != 0U &&
        pixels[1U + 20U * 160U + 80U] == FEEDBACK_DAMAGE_RED565) {
        ++concurrentPresents; /* same real framebuffer present as red border */
        if (damageGlyphs != 0U) ++groupedTextPresents;
    }
    return 1;
}
static void seedUnpainted(void) {
    memset(&actionState, 0, sizeof(actionState));
    memset(&directionCue, 0, sizeof(directionCue));
    for (unsigned i = 0U; i < sizeof(pixels) / sizeof(pixels[0]); ++i)
        pixels[i] = 0x1234U;
    clockMs = 1000U;
    controls = levelUp = pack = failPresent = automapOwner = facingDirty = 0;
    worldActive = 1;
    firstWorldLease = 0;
    topBarTiles = 0U;
    presents = concurrentPresents = damageGlyphs = groupedTextPresents = 0U;
}
static void armRed(void) {
    actionState.viewportFlashPending = 1U;
    actionState.viewportFlashDurationMs = 500U;
    actionState.viewportFlashColor565 = FEEDBACK_DAMAGE_RED565;
}
static void poll(void) {
    assert(EspNativeGameplayActionEngine_service(NULL));
}
static void expectClean(uint16_t world) {
    assert(pixels[0] == 0x1234U &&
           pixels[160U * 120U + 1U] == 0x1234U);
    for (unsigned y = 0U; y < 120U; ++y)
        for (unsigned x = 0U; x < 160U; ++x)
            assert(pixels[1U + y * 160U + x] ==
                   (y >= 20U && y < 100U ? world : 0x1234U));
}
static void expectRed(unsigned side, uint16_t clean) {
    unsigned count = 0U;
    for (unsigned y = 20U; y < 100U; ++y) {
        for (unsigned x = 0U; x < 160U; ++x) {
            const int outer = y < 22U || y >= 98U ||
                              x < 2U || x >= 158U;
            const int extra =
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT &&
                 y >= 22U && y < 30U && x >= 2U && x < 158U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK &&
                 y >= 90U && y < 98U && x >= 2U && x < 158U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT &&
                 x >= 2U && x < 10U && y >= 22U && y < 98U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT &&
                 x >= 150U && x < 158U && y >= 22U && y < 98U);
            const uint16_t expected = outer || extra
                ? FEEDBACK_DAMAGE_RED565 : clean;
            assert(pixels[1U + y * 160U + x] == expected);
            if (outer || extra) ++count;
        }
    }
    assert(count == FEEDBACK_BORDER_PIXELS +
           (side == 0U ? 0U : (side == 1U || side == 3U
                                      ? 1248U : 608U)));
}
static void expireRed(unsigned before) {
    assert(actionState.viewportFlashVisible);
    clockMs += 501U;
    poll();
    assert(!actionState.viewportFlashVisible);
    assert(!actionState.viewportFlashSnapshotValid);
    assert(!directionCue.snapshotValid);
    assert(presents == before + 1U); /* ONLY one expiry present */
    expectClean(0x1234U);
}

int main(void) {
    /* Every direction is part of ONE red 500ms border, never orange, never
     * a separate two-pulse presentation. Side thickness includes the 2px
     * existing perimeter; the inner 8px uses bounded BSS only. */
    for (unsigned side = 1U; side <= 4U; ++side) {
        seedUnpainted();
        armRed();
        assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
            10U + side, 157U, (EspNativeGameplayDamageSide)side));
        assert(presents == 0U);
        assert(__wrap_Esp32PlatformVideo_present());
        assert(presents == 1U && concurrentPresents == 1U);
        assert(actionState.viewportFlashVisible);
        assert(directionCue.snapshotValid);
        assert(directionCue.activeSide == side);
        expectRed(side, 0x1234U);
        clockMs += 120U;
        poll();
        assert(presents == 1U); /* no ON/OFF/GAP/ON pulses */
        expectRed(side, 0x1234U);
        clockMs += 200U;
        poll();
        assert(presents == 1U); /* still ONE border until 500ms */
        clockMs += 181U;
        poll();
        assert(presents == 2U && !actionState.viewportFlashVisible);
        expectClean(0x1234U);
    }

    /* Physically proven ordering: committed monster damage text, the
     * existing border and the enlarged red side are ONE present after
     * potentially slow synchronous FX/monster tail. */
    seedUnpainted();
    armRed();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    strcpy(actionState.feedbackText, "8 damage!");
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    assert(directionCue.stageNonlethalBorder);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashVisible &&
           actionState.feedbackPending && !damageGlyphs);
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        77U, 322U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    clockMs += 818U; /* no timer / paint during long synchronous work */
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(presents == 2U && concurrentPresents == 1U &&
           groupedTextPresents == 1U && damageGlyphs != 0U);
    assert(actionState.viewportFlashVisible &&
           actionState.viewportFlashShownAtMs == clockMs);
    assert(actionState.feedbackShownAtMs == clockMs);
    expectRed(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT, 0x1234U);
    expireRed(2U);

    /* No direction is still the original 944px red 500ms border. */
    seedUnpainted();
    armRed();
    assert(__wrap_Esp32PlatformVideo_present());
    expectRed(0U, 0x1234U);
    expireRed(1U);

    /* Two hits refresh ONE lease. Previous side's interior is restored
     * BEFORE painting the replacement, including when sides differ. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        10U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(__wrap_Esp32PlatformVideo_present());
    clockMs += 10U;
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        11U, 159U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT));
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 2U && concurrentPresents == 2U);
    expectRed(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT, 0x1234U);
    expireRed(2U);

    /* A failed first present retries from the ORIGINAL clean snapshot,
     * never accidentally captures the already painted red as baseline. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        12U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK));
    failPresent = 1;
    assert(!__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashVisible &&
           actionState.viewportFlashSnapshotValid);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 2U && actionState.viewportFlashVisible);
    expectRed(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK, 0x1234U);
    expireRed(2U);

    /* A fresh world repaint invalidates both old snapshots; expiry must
     * restore the new world, not stale pixels from a previous camera pose. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        13U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT));
    assert(__wrap_Esp32PlatformVideo_present());
    for (unsigned y = 20U; y < 100U; ++y)
        for (unsigned x = 0U; x < 160U; ++x)
            pixels[1U + y * 160U + x] = 0x5678U;
    EspNativeGameplayActionEngine_markFreshFrame();
    assert(__wrap_Esp32PlatformVideo_present());
    expectRed(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT, 0x5678U);
    clockMs += 501U;
    poll();
    assert(!actionState.viewportFlashVisible);
    expectClean(0x5678U);

    /* The touch edit lease and fullscreen AUTOMAP do not acquire a red
     * snapshot or overwrite pixels from another screen owner. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        14U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    controls = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashSnapshotValid);
    controls = 0;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(actionState.viewportFlashSnapshotValid);
    expectRed(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT, 0x1234U);
    expireRed(2U);

    seedUnpainted();
    automapOwner = 1;
    armRed();
    for (unsigned i = 1U; i <= 160U * 120U; ++i) pixels[i] = 0x4567U;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashPending &&
           !actionState.viewportFlashVisible);
    assert(!EspNativeGameplayActionEngine_queueDirectionalDamage(
        15U, 7U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    clockMs += 2000U;
    poll();
    assert(presents == 1U && !directionCue.snapshotValid);
    for (unsigned i = 1U; i <= 160U * 120U; ++i)
        assert(pixels[i] == 0x4567U);

    /* Non-world owners, including SAVE slots after EXIT, must never get a
     * gameplay topbar tile or stale world border. First-world lease remains
     * the only exception while the gameplay activation handoff completes. */
    seedUnpainted();
    worldActive = 0;
    facingDirty = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 0U && facingDirty == 1 && presents == 2U);
    firstWorldLease = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(topBarTiles == 1U && facingDirty == 0 && presents == 3U);
    firstWorldLease = 0;
    worldActive = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 4U);
    puts("damage_cue_timing_host: PASS one red 500ms border + 10px directional side, one present+one expiry, no independent pulses/queue, exact refresh/retry/fresh-frame/automap/world-owner restoration");
    return 0;
}
