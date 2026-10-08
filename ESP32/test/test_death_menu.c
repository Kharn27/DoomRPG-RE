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
#include "esp_player_view_state.h"

static uint16_t pixels[160 * 120 + 2];
static EspPlayerViewState view;
static EspNativeGameplayHudState hud;
static EspNativeGameplayPlayerState player;
static uint32_t clockMs;
static int available, loads, presents, presentOk = 1, validBuffer = 1;

void* Esp32PlatformVideo_framebuffer(void) { return pixels + 1; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return validBuffer ? 160 * 120 * sizeof(uint16_t) : 0;
}
int Esp32PlatformVideo_present(void) { ++presents; return presentOk; }
unsigned int DoomRPG_GetUpTimeMS(void) { return clockMs; }
byte DoomRPG_randNextByte(Random_t* rng) { (void)rng; return 1; }
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
EspNativeGameplayHudStatus EspNativeGameplayHud_repaint(
    const EspNativeGameplayHudState* state, EspNativeGameplayHudStats* stats) {
    (void)state; memset(stats, 0, sizeof(*stats)); return ESP_NATIVE_GAMEPLAY_HUD_OK;
}
int EspPlayerView_commitDeathViewZ(int32_t before, int32_t after) {
    assert(view.viewZ == before); view.viewZ = after; return 1;
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render, uint8_t angle,
                                     EspNativeGameplayFrameStats* stats) {
    (void)render; (void)angle; memset(stats, 0, sizeof(*stats)); return 1;
}
int EspNativeGameplaySave_hasReadableCheckpoint(void) { return available; }
int EspNativeGameplaySave_loadCheckpoint(void) {
    ++loads; EspNativeGameplayPlayerDeath_reset(); return 1;
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
    view.active = 1; view.viewZ = 36; hud.active = hud.painted = 1;
    available = hasSave; clockMs = 0; presents = 0;
    for (int i = 0; i < 160 * 120 + 2; ++i) pixels[i] = 0xdead;
    assert(EspNativeGameplayPlayerDeath_arm(doom, 1, 0));
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
    assert(loads == 1 && !EspNativeGameplayPlayerDeath_isActive());

    start(&doom, 0);
    assert(pixel(10, 33) == ESP_HUB_COLOR_STEEL_DARK);
    assert(colorCount(36, 42, ESP_HUB_COLOR_IVORY) == 0);
    assert(colorCount(45, 49, ESP_HUB_COLOR_RED) > 10);
    if (argc > 2) preview(argv[2]);
    for (int row = 0; row < 4; ++row) {
        const int top = 33 + row * 21;
        assert(EspNativeGameplayPlayerDeath_handleTap(149, top + 18));
        assert(EspNativeGameplayPlayerDeath_service(&doom));
        assert(loads == 1 && EspNativeGameplayPlayerDeath_isMenuReady());
        assert(EspNativeGameplayPlayerDeath_handleTap(10, top));
        assert(EspNativeGameplayPlayerDeath_service(&doom));
    }
    assert(loads == 1);
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
    puts("Death menu: PASS (theme/fonts, timing, hitboxes, LOAD/no-save/deferred routes, guards)");
    return 0;
}
