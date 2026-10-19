/* Compile production death presentation + shared HUB font unchanged. Only
 * clock, gameplay and video boundaries are mocked; no board/SD is needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "SDL.h"
#include "DoomRPG.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_player_view_state.h"

static uint16_t pixels[160 * 120 + 2];
static EspPlayerViewState view;
static EspNativeGameplayHudState hud;
static EspNativeGameplayPlayerState player;
static uint32_t clockMs;
static int available, loads, presents, presentOk = 1, validBuffer = 1;
static int selectorActive, selectorReady, selectorBegins, selectorFinishes;
static int selectorBeginOk = 1, selectorTapResult, selectorTaps;
static int loadOk = 1;
static unsigned int deathRngCalls;
static int automapOwnsScreen, liveHudRepaints, failLiveHud;

void* Esp32PlatformVideo_framebuffer(void) { return pixels + 1; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return validBuffer ? 160 * 120 * sizeof(uint16_t) : 0;
}
int Esp32PlatformVideo_present(void) { ++presents; return presentOk; }
unsigned int DoomRPG_GetUpTimeMS(void) { return clockMs; }
byte DoomRPG_randNextByte(Random_t* rng) {
    (void)rng;
    ++deathRngCalls;
    return 1;
}
const EspPlayerViewState* EspPlayerView_view(void) { return &view; }
const EspNativeGameplayHudState* EspNativeGameplayHud_view(void) { return &hud; }
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) { return &player; }
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) { *out = player; return 1; }
int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState* in) { player = *in; return 1; }
uint32_t EspNativeGameplayPlayerState_fingerprint(void) { return 123; }
int EspNativeGameplayPlayerState_enterDeath(uint16_t* weapons, uint8_t* weapon) {
    *weapons = player.weapons; *weapon = player.weapon;
    player.weapons = player.weapon = 0; return 1;
}
int EspNativeResidentGameplay_isAutomapActive(void) {
    return automapOwnsScreen;
}
EspNativeGameplayHudStatus EspNativeGameplayHud_repaintLive(
    const EspNativeGameplayHudState* retained,
    const EspNativeGameplayPlayerState* livePlayer,
    uint8_t angle,
    EspNativeGameplayHudStats* stats) {
    assert(!automapOwnsScreen); /* Never paint HUD into a full-screen map. */
    assert(retained == &hud && retained->model.weapon == 1U);
    assert(livePlayer == &player);
    assert(livePlayer->active && livePlayer->weapon == 0U &&
           livePlayer->weapons == 0U && (livePlayer->param1 & 0xffU) == 0U);
    assert(angle == view.viewAngle);
    ++liveHudRepaints;
    memset(stats, 0, sizeof(*stats));
    if (failLiveHud) return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;
    stats->pixelsWritten = 160U * 20U;
    /* Pixel witness: the CURRENT death state is painted, not stale retained
     * ammo/health/weapon from map entry. No extra framebuffer allocation. */
    for (unsigned x = 0U; x < 160U; ++x)
        pixels[1U + 110U * 160U + x] = 0x003fU;
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}
int EspPlayerView_commitDeathViewZ(int32_t before, int32_t after) {
    assert(view.viewZ == before); view.viewZ = after; return 1;
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render, uint8_t angle,
                                     EspNativeGameplayFrameStats* stats) {
    (void)render; (void)angle; memset(stats, 0, sizeof(*stats)); return 1;
}
int EspNativeGameplaySave_hasReadableCheckpoint(void) { return available; }
int EspNativeGameplaySave_mainSelectorActive(void) { return selectorActive; }
int EspNativeGameplaySave_mainSelectorReady(void) { return selectorReady; }
int EspNativeGameplaySave_mainSelectorBegin(void) {
    ++selectorBegins;
    selectorActive = selectorBeginOk;
    selectorReady = 0;
    return selectorBeginOk;
}
int EspNativeGameplaySave_mainSelectorTap(int x, int y) {
    (void)x; (void)y;
    assert(selectorActive);
    ++selectorTaps;
    if (selectorTapResult == -1) selectorActive = selectorReady = 0;
    return selectorTapResult;
}
void EspNativeGameplaySave_mainSelectorFinish(void) {
    ++selectorFinishes;
    selectorActive = selectorReady = 0;
}
int EspNativeGameplaySave_loadCheckpoint(void) {
    assert(selectorActive && selectorReady);
    ++loads;
    if (!loadOk) return 0;
    EspNativeGameplayPlayerDeath_reset(); return 1;
}

static uint16_t pixel(int x, int y) { return pixels[1 + y * 160 + x]; }
static int colorCount(int top, int bottom, uint16_t color) {
    int count = 0;
    for (int y = top; y <= bottom; ++y)
        for (int x = 0; x < 160; ++x) count += pixel(x, y) == color;
    return count;
}
static void start(DoomRPG_t* doom, int hasSave) {
    EspNativeGameplayPlayerDeath_reset();
    memset(&view, 0, sizeof(view)); memset(&hud, 0, sizeof(hud));
    memset(&player, 0, sizeof(player));
    view.active = 1; view.viewZ = 36; view.viewAngle = view.destAngle = 64;
    hud.active = hud.painted = 1;
    hud.model.weapon = 1; /* Intentionally obsolete immutable map-entry HUD. */
    player.active = 1;
    player.weapons = (uint16_t)(1U << 3);
    player.weapon = 3; /* Chosen with the native weapon grid before death. */
    player.ammo[2] = 17;
    player.param1 = (34U << 8) | (12U << 16) | (23U << 24);
    automapOwnsScreen = failLiveHud = liveHudRepaints = 0;
    available = hasSave; clockMs = 0; presents = 0;
    selectorActive = selectorReady = selectorBegins = selectorFinishes = 0;
    selectorTaps = selectorTapResult = 0;
    selectorBeginOk = loadOk = 1;
    for (int i = 0; i < 160 * 120 + 2; ++i) pixels[i] = 0xdead;
    assert(EspNativeGameplayPlayerDeath_arm(doom, 1, 0));
    assert(liveHudRepaints == 1);
    assert(player.weapon == 0U && player.weapons == 0U);
    assert(pixel(30, 110) == 0x003fU);
    assert(!EspNativeGameplayPlayerDeath_handleTap(80, 40));
    clockMs = 500;
    assert(EspNativeGameplayPlayerDeath_service(doom));
    assert(view.viewZ < 36 && !EspNativeGameplayPlayerDeath_isMenuReady());
    assert(!EspNativeGameplayPlayerDeath_handleTap(80, 40));
    clockMs = 2999;
    assert(EspNativeGameplayPlayerDeath_service(doom));
    assert(!EspNativeGameplayPlayerDeath_isMenuReady());
    clockMs = 3000;
    assert(EspNativeGameplayPlayerDeath_service(doom));
    assert(EspNativeGameplayPlayerDeath_isMenuReady());
    assert(pixels[0] == 0xdead && pixels[160 * 120 + 1] == 0xdead);
}
static void preview(const char* path) {
    FILE* f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n320 240\n255\n");
    for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
        const uint16_t c = pixel(x / 2, y / 2);
        const unsigned char rgb[3] = { (unsigned char)(((c >> 11) & 31) * 255 / 31),
            (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) };
        assert(fwrite(rgb, 1, 3, f) == 3);
    }
    assert(fclose(f) == 0);
}
int main(int argc, char** argv) {
    DoomRPG_t doom = {0}; doom.render = (void*)1;
    start(&doom, 1);
    assert(pixel(3, 3) == ESP_HUB_COLOR_STEEL_DARK);
    assert(pixel(8, 7) == ESP_HUB_COLOR_RED);
    assert(pixel(10, 33) == ESP_HUB_COLOR_AMBER);
    assert(colorCount(10, 16, ESP_HUB_COLOR_RED) > 40);
    assert(colorCount(36, 42, ESP_HUB_COLOR_IVORY) > 100);
    if (argc > 1) preview(argv[1]);
    /* Header, margin, all gaps, footer and out-of-screen taps stay inert. */
    const int gaps[] = {-1, 0, 28, 32, 52, 53, 73, 74, 94, 95, 115, 119, 120};
    for (size_t i = 0; i < sizeof(gaps) / sizeof(gaps[0]); ++i)
        assert(!EspNativeGameplayPlayerDeath_handleTap(80, gaps[i]));
    assert(!EspNativeGameplayPlayerDeath_handleTap(9, 40));
    assert(!EspNativeGameplayPlayerDeath_handleTap(150, 40));
    assert(EspNativeGameplayPlayerDeath_handleTap(10, 33));
    assert(loads == 0); /* Touch only queues: LOAD runs in service, unchanged. */
    assert(!EspNativeGameplayPlayerDeath_handleTap(149, 51));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(loads == 0 && selectorBegins == 1 && selectorActive);
    assert(EspNativeGameplayPlayerDeath_isMenuReady());
    /* Pagination/arming is consumed without dispatching a checkpoint load. */
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 60));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(selectorTaps == 1 && loads == 0);
    selectorTapResult = -1; /* BACK repaints the death menu. */
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 8));
    assert(!selectorActive && loads == 0);
    assert(pixel(10, 33) == ESP_HUB_COLOR_AMBER);
    assert(EspNativeGameplayPlayerDeath_handleTap(10, 33));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(selectorBegins == 2 && loads == 0);
    selectorTapResult = 1; selectorReady = 1; /* Explicit slot confirmation. */
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 60));
    assert(loads == 0);
    assert(!EspNativeGameplayPlayerDeath_handleTap(80, 60)); /* Already queued. */
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(loads == 1 && !EspNativeGameplayPlayerDeath_isActive());
    assert(selectorFinishes == 1 && !selectorActive);

    /* Selector open failure, unready choice and restore failure stay closed. */
    start(&doom, 1);
    selectorBeginOk = 0;
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 40));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(!selectorActive && loads == 1);
    selectorBeginOk = 1;
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 40));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    selectorTapResult = 1; /* A callback alone does not prove Ready. */
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 60));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(loads == 1 && selectorFinishes == 0);
    selectorReady = 1; loadOk = 0;
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 60));
    assert(!EspNativeGameplayPlayerDeath_service(&doom));
    assert(loads == 2 && selectorActive && selectorFinishes == 0);
    assert(EspNativeGameplayPlayerDeath_isActive());
    loadOk = 1;
    assert(EspNativeGameplayPlayerDeath_handleTap(80, 60));
    assert(EspNativeGameplayPlayerDeath_service(&doom));
    assert(loads == 3 && selectorFinishes == 1 && !selectorActive);

    start(&doom, 0);
    assert(pixel(10, 33) == ESP_HUB_COLOR_STEEL_DARK);
    assert(colorCount(36, 42, ESP_HUB_COLOR_IVORY) == 0);
    assert(colorCount(45, 49, ESP_HUB_COLOR_RED) > 10);
    if (argc > 2) preview(argv[2]);
    for (int row = 0; row < 4; ++row) {
        const int top = 33 + row * 21;
        assert(EspNativeGameplayPlayerDeath_handleTap(149, top + 18));
        assert(EspNativeGameplayPlayerDeath_service(&doom));
        assert(loads == 3 && EspNativeGameplayPlayerDeath_isMenuReady());
        assert(EspNativeGameplayPlayerDeath_handleTap(10, top));
        assert(EspNativeGameplayPlayerDeath_service(&doom));
    }
    assert(loads == 3 && selectorBegins == 0 && selectorFinishes == 0);
    /* A malformed buffer or failed present must not publish a usable menu. */
    EspNativeGameplayPlayerDeath_reset();
    view.viewZ = 36; clockMs = 0;
    assert(EspNativeGameplayPlayerDeath_arm(&doom, 2, 0));
    clockMs = 3000; validBuffer = 0;
    assert(!EspNativeGameplayPlayerDeath_service(&doom));
    assert(!EspNativeGameplayPlayerDeath_handleTap(80, 40));
    validBuffer = 1;
    EspNativeGameplayPlayerDeath_reset(); view.viewZ = 36; clockMs = 0;
    assert(EspNativeGameplayPlayerDeath_arm(&doom, 3, 0));
    clockMs = 3000; presentOk = 0;
    assert(!EspNativeGameplayPlayerDeath_service(&doom));
    assert(!EspNativeGameplayPlayerDeath_handleTap(80, 40));
    assert(pixels[0] == 0xdead && pixels[160 * 120 + 1] == 0xdead);
    /* The single-blast barrel consumes death RNG before monster splash, then
     * commits into this same terminal owner WITHOUT drawing another byte. */
    EspNativeGameplayPlayerDeath_reset();
    view.viewZ = 36;
    clockMs = 0;
    presentOk = 1;
    unsigned int beforeDeathRng = deathRngCalls;
    assert(EspNativeGameplayPlayerDeath_armWithReservedRand(
        &doom, 487U, 486U, 0xa5U));
    assert(EspNativeGameplayPlayerDeath_isActive());
    assert(deathRngCalls == beforeDeathRng);
    assert(player.weapon == 0U && player.weapons == 0U);
    EspNativeGameplayPlayerDeath_reset();
    view.viewZ = 36;
    assert(EspNativeGameplayPlayerDeath_arm(&doom, 488U, 486U));
    assert(deathRngCalls == beforeDeathRng + 1U);
    /* Fail-closed world HUD repaint: the post-death PlayerState rolls back,
     * no death RNG is consumed and there is no armed death owner. */
    {
        const unsigned int rngBefore = deathRngCalls;
        EspNativeGameplayPlayerState before;
        EspNativeGameplayPlayerDeath_reset();
        view.viewZ = 36;
        player.active = 1;
        player.weapon = 3U;
        player.weapons = (uint16_t)(1U << 3);
        player.param1 = 34U << 8;
        before = player;
        failLiveHud = 1;
        liveHudRepaints = 0;
        assert(!EspNativeGameplayPlayerDeath_arm(&doom, 501U, 181U));
        assert(liveHudRepaints == 1 && !EspNativeGameplayPlayerDeath_isActive());
        assert(memcmp(&player, &before, sizeof(player)) == 0);
        assert(deathRngCalls == rngBefore);
        failLiveHud = 0;
    }
    /* Lethal hit while AUTOMAP owns the full screen: arming death may mutate
     * PlayerState but MUST NOT paint even the LIVE 3D HUD on map pixels.
     * Monster retaliation's raw present must still see the original map. */
    {
        const unsigned int rngBefore = deathRngCalls;
        EspNativeGameplayPlayerDeath_reset();
        view.viewZ = 36;
        player.weapon = 3U;
        player.weapons = (uint16_t)(1U << 3);
        player.param1 = 34U << 8;
        automapOwnsScreen = 1;
        liveHudRepaints = 0;
        for (unsigned i = 1U; i < 160U * 120U + 1U; ++i)
            pixels[i] = 0x49a2U;
        pixels[0] = pixels[160U * 120U + 1U] = 0xdeadU;
        assert(EspNativeGameplayPlayerDeath_arm(&doom, 502U, 181U));
        assert(player.weapon == 0U && player.weapons == 0U);
        assert(liveHudRepaints == 0 && deathRngCalls == rngBefore + 1U);
        assert(Esp32PlatformVideo_present());
        for (unsigned i = 1U; i < 160U * 120U + 1U; ++i)
            assert(pixels[i] == 0x49a2U);
        assert(pixels[0] == 0xdeadU && pixels[160U * 120U + 1U] == 0xdeadU);
        automapOwnsScreen = 0;
        EspNativeGameplayPlayerDeath_reset();
    }
    puts("Death menu: PASS live post-death HUD, stale retained HUD forbidden, automap lethal raw-present stays map-exact, rollback/RNG, menu/timing/slots");
    return 0;
}
