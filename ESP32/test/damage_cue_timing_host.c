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
static uint16_t tinted(uint16_t world) {
    return (uint16_t)(((world & 0xf7deU) >> 1U) +
                      ((DAMAGE_BORDER_TINT565 & 0xf7deU) >> 1U));
}
static void expectBorder(unsigned side, uint16_t clean, uint16_t outer) {
    unsigned count = 0U;
    for (unsigned y = 20U; y < 100U; ++y) {
        for (unsigned x = 0U; x < 160U; ++x) {
            const int edge = y < 22U || y >= 98U ||
                             x < 2U || x >= 158U;
            const int extra =
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT &&
                 y >= 22U && y < 32U && x >= 2U && x < 158U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK &&
                 y >= 88U && y < 98U && x >= 2U && x < 158U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT &&
                 x >= 2U && x < 12U && y >= 22U && y < 98U) ||
                (side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT &&
                 x >= 148U && x < 158U && y >= 22U && y < 98U);
            const uint16_t expected = edge ? outer :
                                      extra ? tinted(clean) : clean;
            if (pixels[1U + y * 160U + x] != expected) {
                fprintf(stderr, "[BLINK-HOST] mismatch x=%u y=%u side=%u observed=%04x expected=%04x lit=%u snapshot=%u present=%u clock=%u color=%04x\n",
                        x, y, side, pixels[1U + y * 160U + x],
                        expected, actionState.viewportFlashLit,
                        directionCue.snapshotValid, presents, clockMs,
                        actionState.viewportFlashColor565);
                assert(0 && "red border and translucent side diverged");
            }
            if (edge || extra) ++count;
        }
    }
    assert(count == FEEDBACK_BORDER_PIXELS +
           (side == 0U ? 0U : (side == 1U || side == 3U
                                      ? 1560U : 760U)));
}
static void finishBlink(unsigned before, uint16_t world) {
    const unsigned schedule[6] = {105U, 65U, 105U, 65U, 105U, 55U};
    for (unsigned i = 0; i < 6U; ++i) {
        clockMs += schedule[i];
        poll();
        assert(actionState.viewportFlashLit == ((i % 2U) == 1U && i < 5U));
        assert(presents == before + (i < 5U ? i + 1U : 5U));
        if (i < 5U) {
            if (actionState.viewportFlashLit)
                expectBorder(directionCue.activeSide, world,
                             actionState.viewportFlashColor565);
            else expectClean(world);
        }
    }
    assert(!actionState.viewportFlashVisible &&
           !actionState.viewportFlashSnapshotValid &&
           !directionCue.snapshotValid);
    expectClean(world);
}
int main(void) {
    /* The SAME red border/tint turns ON+OFF three times using the existing
     * 500ms flash clock, with exactly one present for each visual state
     * change and NONE at the final already-OFF expiry. */
    for (unsigned side = 1U; side <= 4U; ++side) {
        seedUnpainted();
        armRed();
        assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
            10U + side, 157U, (EspNativeGameplayDamageSide)side));
        assert(presents == 0U);
        assert(__wrap_Esp32PlatformVideo_present());
        assert(presents == 1U && concurrentPresents == 1U);
        assert(actionState.viewportFlashLit &&
               actionState.viewportFlashVisible &&
               directionCue.activeSide == side);
        expectBorder(side, 0x1234U, FEEDBACK_DAMAGE_RED565);
        clockMs += 104U;
        poll();
        assert(presents == 1U);
        expectBorder(side, 0x1234U, FEEDBACK_DAMAGE_RED565);
        /* We are 104ms into the first ON; first OFF starts at 105ms. */
        clockMs -= 104U;
        finishBlink(1U, 0x1234U);
    }

    /* The nonlethal damage text and BOTH parts of the flash are committed
     * together after slow 818ms monster FX work, with ONE first present. */
    seedUnpainted();
    armRed();
    actionState.feedbackPending = 1U;
    actionState.feedbackKind = ACTION_FEEDBACK_DAMAGE;
    strcpy(actionState.feedbackText, "8 damage!");
    EspNativeGameplayActionEngine_beginMonsterVisualWork();
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashVisible &&
           actionState.feedbackPending && !damageGlyphs);
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        77U, 322U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    clockMs += 818U;
    EspNativeGameplayActionEngine_endMonsterVisualWork();
    assert(presents == 2U && concurrentPresents == 1U &&
           groupedTextPresents == 1U && damageGlyphs != 0U);
    assert(actionState.viewportFlashVisible &&
           actionState.viewportFlashShownAtMs == clockMs);
    assert(actionState.feedbackShownAtMs == clockMs);
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT, 0x1234U,
                 FEEDBACK_DAMAGE_RED565);
    finishBlink(2U, 0x1234U);

    /* Real white pickup compositor: three pulses, zero directional tint,
     * exact pixel restoration and no redundant final physical present. */
    seedUnpainted();
    actionState.viewportFlashPending = 1U;
    actionState.viewportFlashColor565 = 0xffffU;
    actionState.viewportFlashDurationMs = 500U;
    assert(__wrap_Esp32PlatformVideo_present());
    expectBorder(0U, 0x1234U, 0xffffU);
    finishBlink(1U, 0x1234U);

    /* A second hit refreshes the SAME lease and changes the tinted side
     * without leaving a ghost of the earlier red/tinted side. */
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
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT, 0x1234U,
                 FEEDBACK_DAMAGE_RED565);
    finishBlink(2U, 0x1234U);

    /* A failed first physical present retries the snapshot instead of
     * capturing already-tinted or already-red pixels as its world baseline. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        12U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK));
    failPresent = 1;
    assert(!__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashVisible &&
           actionState.viewportFlashSnapshotValid);
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 2U && actionState.viewportFlashLit);
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK, 0x1234U,
                 FEEDBACK_DAMAGE_RED565);
    finishBlink(2U, 0x1234U);

    /* Fail one shared OFF and then one shared ON; the pending state is
     * retried without exposing an incomplete border/side snapshot. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        20U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT));
    assert(__wrap_Esp32PlatformVideo_present());
    clockMs += 105U;
    failPresent = 1;
    assert(!serviceViewportFlashExpiry());
    assert(actionState.viewportFlashLit);
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT, 0x1234U,
                 FEEDBACK_DAMAGE_RED565);
    assert(serviceViewportFlashExpiry());
    assert(!actionState.viewportFlashLit);
    expectClean(0x1234U);
    clockMs += 65U;
    failPresent = 1;
    assert(!serviceViewportFlashExpiry());
    assert(!actionState.viewportFlashLit);
    expectClean(0x1234U);
    assert(serviceViewportFlashExpiry());
    assert(actionState.viewportFlashLit);
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT, 0x1234U,
                 FEEDBACK_DAMAGE_RED565);
    clockMs += 330U;
    poll();
    assert(!actionState.viewportFlashVisible);
    expectClean(0x1234U);

    /* Clean world redraw while ON adopts the NEW world pixels for BOTH
     * snapshots, preserving them through all following shared blinks. */
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
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT, 0x5678U,
                 FEEDBACK_DAMAGE_RED565);
    finishBlink(2U, 0x5678U);

    /* A redraw during an OFF interval is NEVER tinted early and the
     * next ON captures its new world baseline, not stale pre-move pixels. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        14U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    assert(__wrap_Esp32PlatformVideo_present());
    clockMs += 105U;
    poll();
    assert(!actionState.viewportFlashLit);
    for (unsigned y = 20U; y < 100U; ++y)
        for (unsigned x = 0U; x < 160U; ++x)
            pixels[1U + y * 160U + x] = 0x5678U;
    EspNativeGameplayActionEngine_markFreshFrame();
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 3U && !actionState.viewportFlashLit);
    expectClean(0x5678U);
    clockMs += 65U;
    poll();
    assert(actionState.viewportFlashLit);
    expectBorder(ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT, 0x5678U,
                 FEEDBACK_DAMAGE_RED565);
    clockMs += 330U;
    poll();
    assert(!actionState.viewportFlashVisible);
    expectClean(0x5678U);

    /* No framebuffer mutations while the touch lease or AUTOMAP own it. */
    seedUnpainted();
    armRed();
    assert(EspNativeGameplayActionEngine_queueDirectionalDamage(
        15U, 157U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    controls = 1;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashSnapshotValid);
    controls = 0;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(actionState.viewportFlashSnapshotValid);
    finishBlink(2U, 0x1234U);

    seedUnpainted();
    automapOwner = 1;
    armRed();
    for (unsigned i = 1U; i <= 160U * 120U; ++i) pixels[i] = 0x4567U;
    assert(__wrap_Esp32PlatformVideo_present());
    assert(presents == 1U && !actionState.viewportFlashPending &&
           !actionState.viewportFlashVisible);
    assert(!EspNativeGameplayActionEngine_queueDirectionalDamage(
        16U, 7U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
    clockMs += 2000U;
    poll();
    assert(presents == 1U && !directionCue.snapshotValid);
    for (unsigned i = 1U; i <= 160U * 120U; ++i)
        assert(pixels[i] == 0x4567U);

    /* Opaque menu/SAVESLOTS cannot inherit world-level blink or dirty bar. */
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

    puts("damage_cue_timing_host: PASS shared 3-pulse red+translucent direction, one 500ms clock, failed ON/OFF retry, exact new-world restore, white pickup 3-pulse exact restore, automap, menu isolation");
    return 0;
}
