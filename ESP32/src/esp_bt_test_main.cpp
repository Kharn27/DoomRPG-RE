#if defined(DOOMRPG_ESP32_BT_TEST_PROFILE) && DOOMRPG_ESP32_BT_TEST_PROFILE
/* Isolated BT-MIDI experiment. This TU is the sole Arduino entrypoint in
 * esp32-cyd-bt-test. Pull in the unchanged production bootstrap helpers, but
 * NEVER call its normal setup/loop or poll/dispatch touchscreen events.
 * This keeps the exact native MENU_MAIN image without starting Doom RPG. */
#define setup DoomRPG_ProductionSetup_Inert
#define loop DoomRPG_ProductionLoop_Inert
#include "main.cpp"
#undef setup
#undef loop
#include "esp_bt_test_midi.h"
#include "esp_bt_test_a2dp.h"
#include "esp_heap_caps.h"
#include "Render.h"
#include "esp_native_root_state.h"
#include "esp_render_startup_bridge.h"
extern bool PlatformVideo_btTestReleaseFrozenFramebuffer();


void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("[BTTEST] ISOLATED env=esp32-cyd-bt-test touchscreen=disabled map=none gameplay=disabled");
    printSystemInfo();
    /* NOT input.begin(): no touch IRQ or callback, no gameplay loop. */
    display.begin();
    display.setRotation(cyd::kDisplayRotation);
    display.setSwapBytes(true);
    initializePlatformVideo();
    initializeSdCard();
    EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);
    initializeGameAssets();
    initializeEngineCore();
    initializeEngineLayout();
    initializeRenderStartup();
    initializeConfigMappings();
    initializeMainMenu();
    Serial.printf("[BTTEST] MENU_STATIC ready=%u heap8=%lu largest8=%lu dma=%lu largestDMA=%lu\n",
                  (unsigned)engineMainMenuReady,
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    if (!engineMainMenuReady || !assetPackReady) {
        Serial.println("[BTTEST] FAIL-CLOSED menu-or-PAK-not-ready");
        return;
    }
    /* One last exact native redraw. LCD GRAM retains the image forever;
     * disabling both input and further renderer calls permits a one-way
     * release of the RAM framebuffer and now-dead media mappings.
     * These are valid ONLY in the isolated BT-1 diagnostic firmware. */
    if (!PlatformVideo_present()) {
        Serial.println("[BTTEST] FAIL-CLOSED final-menu-present");
        return;
    }
    Render_t *render = EspNativeRootOwners_live()->render;
    if (render == nullptr ||
        render->framebuffer != (uint8_t*)PlatformVideo_framebuffer()) {
        Serial.println("[BTTEST] FAIL-CLOSED render-framebuffer-alias-mismatch");
        return;
    }
    const uint32_t beforeFreeze = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    /* Mapping compatibility release touches NO immutable maps or framebuffer.
     * No render/game/menu code is ever serviced after this point. */
    EspNativeRenderRuntime_releaseCompatibility(render);
    render->framebuffer = nullptr;
    if (!PlatformVideo_btTestReleaseFrozenFramebuffer()) {
        Serial.println("[BTTEST] FAIL-CLOSED framebuffer-release");
        return;
    }
    const uint32_t afterFreeze = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[BTTEST] FROZEN_RELEASE before=%lu after=%lu gained=%lu largest8=%lu dma=%lu display=latched gameplay=off\\n",
                  (unsigned long)beforeFreeze, (unsigned long)afterFreeze,
                  (unsigned long)(afterFreeze - beforeFreeze),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_DMA));
    (void)EspBtTestMidi_begin(); /* exact PAK MIDI or explicit music failure */
    EspBtTestA2dp_begin();
}

void loop() {
    /* Zero calls to native gameplay, main menu actions, touch or intro.
     * Only SD MIDI lookahead + BT connection/media task are serviced. */
    EspBtTestMidi_pump();
    EspBtTestA2dp_service();
    delay(2);
}

#endif /* DOOMRPG_ESP32_BT_TEST_PROFILE */
