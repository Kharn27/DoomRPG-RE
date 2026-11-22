#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>

#include "board_config.h"
#include "esp_asset_pack.h"
#include "esp_native_boot_legals.h"
#include "esp_audio_entrance_music.h"
#include "esp_native_sfx_probe.h"
#include "esp_legacy_asset_source.h"
#include "esp_legacy_config_mappings_startup.h"
#include "esp_native_config.h"
#include "engine_metrics.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_startup.h"
#include "esp_native_resident_gameplay.h"
#include "menu_bsp_probe.h"
#include "native_intro_clock.h"
#include "native_main_menu_model.h"
#include "native_main_menu_actions.h"
#include "native_main_menu_touch_layout.h"
#include "platform_input.h"
#include "platform_video.h"
#include "platform_video_config.h"
#if DOOMRPG_RES60_P6_MENU
#include "esp_res60_menu.h"
#include "platform_touch_events.h"
#endif
#if DOOMRPG_RES60_P7_NAV
#include "esp_res60_nav.h"
#endif
#if DOOMRPG_RES60_P8_TEXEL
#include "esp_res60_texture_probe.h"
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
#include "esp_res60_inventory_cache.h"
#endif
#include "esp_legacy_prerender_startup.h"
#include "esp_render_startup_bridge.h"
#include "soft_xpt2046.h"

extern struct DoomRPG_s* doomRpg;

#ifndef DOOMRPG_ESP32_SCREEN_DIAGNOSTICS
#define DOOMRPG_ESP32_SCREEN_DIAGNOSTICS 0
#endif

#ifndef DOOMRPG_ESP32_BRINGUP_PROBES
#define DOOMRPG_ESP32_BRINGUP_PROBES 0
#endif

namespace
{

    TFT_eSPI display;
    SoftXpt2046 touchscreen(cyd::kTouchMosi, cyd::kTouchMiso,
                            cyd::kTouchClock, cyd::kTouchCs, cyd::kTouchIrq);
    PlatformInput input(touchscreen);

    bool sdReady = false;
    bool assetPackReady = false;
    bool assetResourcesReady = false;
    bool videoReady = false;
    bool engineCoreReady = false;
    bool engineLayoutReady = false;
    bool enginePreRenderReady = false;
    bool engineRenderStartupReady = false;
    bool engineConfigMappingsReady = false;
    bool engineMainMenuReady = false;
    bool bootLegalsPending = false;
    bool bootLegalsTouchDown = false;
    DoomRpgCoreInitReport coreReport{};
    DoomRpgLayoutReport layoutReport{};
    uint32_t lastTouchUpdate = 0;
    uint32_t lastHeartbeat = 0;
#if DOOMRPG_RES60_P5_TITLE
    bool res60TitleVisible = false;
    bool res60TouchWasDown = false;
#endif
#if DOOMRPG_RES60_P7_NAV
    uint8_t res60QuarterTurns = 0U;
#endif
#if DOOMRPG_RES60_P6_MENU
    bool res60MenuVisible = false;
    int8_t res60MenuArmed = -1;
    bool res60TapPending = false;
    int16_t res60TapX = 0, res60TapY = 0;
    /* Called by the regular calibrated PlatformInput owner in loop context.
     * Merely capture an edge; never touch PAK, framebuffer or TFT here. */
    void res60MenuTapCallback(int16_t x, int16_t y,
                              uint16_t pressure, uint16_t rawX, uint16_t rawY) {
        (void)pressure; (void)rawX; (void)rawY;
        res60TapX = x; res60TapY = y; res60TapPending = true;
    }
#endif

    void drawLabel(int16_t y, const char *label, const char *value,
                   uint16_t color)
    {
#if DOOMRPG_ESP32_SCREEN_DIAGNOSTICS
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setCursor(8, y);
        display.print(label);
        display.setTextColor(color, TFT_BLACK);
        display.setCursor(106, y);
        display.print(value);
        display.print("                    ");
#else
        (void)y;
        (void)label;
        (void)value;
        (void)color;
#endif
    }

    void drawDiagnosticScreen()
    {
#if DOOMRPG_ESP32_SCREEN_DIAGNOSTICS
        display.fillScreen(TFT_BLACK);
        display.setTextSize(2);
        display.setTextColor(TFT_YELLOW, TFT_BLACK);
        display.setCursor(8, 7);
        display.print("DOOM RPG / CYD bring-up");

        display.fillRect(8, 31, 96, 22, TFT_RED);
        display.fillRect(112, 31, 96, 22, TFT_GREEN);
        display.fillRect(216, 31, 96, 22, TFT_BLUE);

        drawLabel(66, "Display:", "OK 320x240", TFT_GREEN);
        drawLabel(90, "SD card:", "testing...", TFT_YELLOW);
        drawLabel(114, "Touch:", "waiting", TFT_CYAN);
        drawLabel(138, "Game data:", "waiting", TFT_YELLOW);
        drawLabel(162, "Video:", "testing...", TFT_YELLOW);
        drawLabel(186, "Engine:", "waiting...", TFT_YELLOW);

        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, 214);
        display.print("Serial: 115200 / ttyUSB0");
#endif
    }

    bool assetContains(const char *name)
    {
        uint32_t bytes = 0U;
        return EspLegacyAssetSource_stat(name, &bytes) != 0;
    }

    bool validateLayoutResources()
    {
        static const char *const requiredHudFiles[] = {
            "bar_lg.bmp",
            "k.bmp",
            "n.bmp",
            "o.bmp",
            "l.bmp",
            "m.bmp",
        };

        bool allPresent = true;
        for (const char *name : requiredHudFiles)
        {
            if (!assetContains(name))
            {
                Serial.printf("[DATA] MISSING required HUD resource: %s\n", name);
                allPresent = false;
            }
        }

        if (!allPresent)
        {
            Serial.println("[DATA] HUD resource preflight FAILED");
            Serial.println("[DATA] DoomRPG-ESP32.pak is missing one or more required HUD assets");
        }
        else
        {
            Serial.println("[DATA] HUD resource preflight OK");
        }

        return allPresent;
    }

    void initializeSdCard()
    {
        pinMode(cyd::kSdCs, OUTPUT);
        digitalWrite(cyd::kSdCs, HIGH);
        SPI.begin(cyd::kSdClock, cyd::kSdMiso, cyd::kSdMosi, cyd::kSdCs);

        sdReady = SD.begin(cyd::kSdCs, SPI, cyd::kSdFrequency);
        if (!sdReady)
        {
            Serial.println("[SD] No card or mount failure");
            drawLabel(90, "SD card:", "not mounted", TFT_ORANGE);
            return;
        }

        const uint64_t sizeMiB = SD.cardSize() / (1024ULL * 1024ULL);
        char status[32];
        snprintf(status, sizeof(status), "OK (%llu MiB)", sizeMiB);
        drawLabel(90, "SD card:", status, TFT_GREEN);
        Serial.printf("[SD] Mounted, size=%llu MiB\n", sizeMiB);
    }

    void initializeGameAssets()
    {
        uint32_t entryCount = 0U;

        if (!sdReady)
        {
            drawLabel(138, "Game data:", "SD unavailable", TFT_ORANGE);
            return;
        }
        if (!SD.exists(ESP_ASSET_PACK_DEFAULT_PATH))
        {
            drawLabel(138, "Game data:", "native PAK missing", TFT_ORANGE);
            Serial.printf("[DATA] %s not found on SD card\n", ESP_ASSET_PACK_DEFAULT_PATH);
            return;
        }

        assetPackReady = EspLegacyAssetSource_validate(&entryCount) != 0;
        Serial.printf("[DATA] Native PAK indexed, entries=%u\n",
                      static_cast<unsigned int>(entryCount));

        if (!assetPackReady)
        {
            drawLabel(138, "Game data:", "PAK invalid", TFT_RED);
            return;
        }

        assetResourcesReady = validateLayoutResources();

        char status[32];
        if (assetResourcesReady)
        {
            snprintf(status, sizeof(status), "%u PAK entries",
                     static_cast<unsigned int>(entryCount));
            drawLabel(138, "Game data:", status, TFT_GREEN);
        }
        else
        {
            snprintf(status, sizeof(status), "PAK incomplete");
            drawLabel(138, "Game data:", status, TFT_ORANGE);
        }
    }

    void initializePlatformVideo()
    {
        videoReady = PlatformVideo_begin(&display);
        drawLabel(162, "Video:", videoReady ? "160x120 x2" : "alloc failed",
                  videoReady ? TFT_GREEN : TFT_RED);
    }

    void initializeEngineCore()
    {
        Serial.println();
        Serial.println("=== Doom RPG core object graph probe ===");
        engineCoreReady = DoomRPG_initEngineCore(&coreReport) != 0;

        char status[48];
        if (engineCoreReady)
        {
            snprintf(status, sizeof(status), "CORE %uB",
                     static_cast<unsigned int>(coreReport.bytesUsed));
            drawLabel(186, "Engine:", status, TFT_GREEN);
        }
        else
        {
            snprintf(status, sizeof(status), "FAIL %s",
                     DoomRPG_coreStageName(coreReport.failedStage));
            drawLabel(186, "Engine:", status, TFT_RED);
        }

        Serial.printf("[CORE] Summary ready=%s used=%u heap8=%u largest8=%u clip=%ux%u\n",
                      engineCoreReady ? "yes" : "no",
                      static_cast<unsigned int>(coreReport.bytesUsed),
                      static_cast<unsigned int>(coreReport.heapAfter),
                      static_cast<unsigned int>(coreReport.largestBlockAfter),
                      coreReport.clipWidth, coreReport.clipHeight);
    }

    void initializeEngineLayout()
    {
        Serial.println();
        Serial.println("=== Doom RPG 160x120 layout + HUD startup probe ===");

        if (!engineCoreReady || !assetResourcesReady || !videoReady)
        {
            Serial.println("[LAYOUT] Prerequisite unavailable; probe skipped safely");
            if (!assetResourcesReady)
            {
                Serial.println("[LAYOUT] native PAK does not contain the HUD resources required by the native HUD");
            }
            drawLabel(186, "Engine:", "LAYOUT skipped", TFT_ORANGE);
            return;
        }

        engineLayoutReady = DoomRPG_startEngineLayout(doomRpg, &layoutReport) != 0;

        char status[48];
        if (engineLayoutReady)
        {
            snprintf(status, sizeof(status), "OK %ux%u",
                     layoutReport.renderWidth, layoutReport.renderHeight);
            drawLabel(186, "Engine:", status, TFT_GREEN);
        }
        else
        {
            drawLabel(186, "Engine:", "LAYOUT FAIL", TFT_RED);
        }

        Serial.printf(
            "[LAYOUT] Summary ready=%s used=%u heap8=%u largest8=%u display=%ux%u render=%ux%u\n",
            engineLayoutReady ? "yes" : "no",
            static_cast<unsigned int>(layoutReport.bytesUsed),
            static_cast<unsigned int>(layoutReport.heap8After),
            static_cast<unsigned int>(layoutReport.largest8After),
            layoutReport.displayWidth, layoutReport.displayHeight,
            layoutReport.renderWidth, layoutReport.renderHeight);
    }

    void initializeRenderStartup()
    {
        enginePreRenderReady =
            EspLegacyPrerenderStartup_start(doomRpg, engineLayoutReady ? 1 : 0) != 0;
        engineRenderStartupReady =
            EspRenderStartupBridge_start(doomRpg, enginePreRenderReady ? 1 : 0) != 0;

        if (engineRenderStartupReady)
        {
            drawLabel(186, "Engine:", "RENDER startup OK", TFT_GREEN);
        }
        else if (enginePreRenderReady)
        {
            drawLabel(186, "Engine:", "RENDER startup FAIL", TFT_RED);
        }
    }

    void initializeConfigMappings()
    {
        engineConfigMappingsReady =
            EspLegacyConfigMappingsStartup_start(doomRpg, engineRenderStartupReady ? 1 : 0) != 0;

        if (engineConfigMappingsReady)
        {
            drawLabel(186, "Engine:", "MAPPINGS OK", TFT_GREEN);
        }
        else if (engineRenderStartupReady)
        {
            drawLabel(186, "Engine:", "MAPPINGS FAIL", TFT_RED);
        }
    }

    void initializeMainMenu()
    {
        if (!engineConfigMappingsReady)
        {
            engineMainMenuReady = false;
            return;
        }

#if DOOMRPG_ESP32_BRINGUP_PROBES
        /*
         * Report the retired ZIP menu probe fail-closed in bringup. It no
         * longer runs a structural/render suite. Production directly paints
         * the opaque native dashboard; do not resurrect a 3D menu prerequisite.
         */
        engineMainMenuReady =
            DoomRPG_probeMenuBspHeader(1) != 0;
        if (engineMainMenuReady)
        {
            drawLabel(186, "Engine:", "MENU BSP OK", TFT_GREEN);
        }
        else
        {
            drawLabel(186, "Engine:", "MENU BSP FAIL", TFT_RED);
        }
#else
        uint32_t frameFNV = 0;

        /* MENU-M1: title audio is already playing before the first MENU_MAIN
         * physical present. A missing WAV never blocks navigation. */
        const bool menuBuilt =
            doomRpg != nullptr &&
            DoomRPG_esp32MainMenuModelBuildMain(doomRpg) != 0;
        if (menuBuilt) (void)EspEntranceMusic_prestartMenuBeforeFirstFrame();
        engineMainMenuReady =
            menuBuilt &&
            DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV) != 0;
        if (!engineMainMenuReady && menuBuilt)
            EspEntranceMusic_stopMenuBeforeAction();

        if (engineMainMenuReady)
        {
            Serial.printf(
                "[MAINBOOT] READY owner=native-opaque menuBspRuntime=skipped "
                "legacyMapStructures=not-created frame=%08x\n",
                static_cast<unsigned int>(frameFNV));
            drawLabel(186, "Engine:", "MENU OK", TFT_GREEN);
        }
        else
        {
            Serial.println(
                "[MAINBOOT] FAILED native MENU_MAIN model/presentation");
            drawLabel(186, "Engine:", "MENU FAIL", TFT_RED);
        }
#endif
    }


    /* Original 1994-2005 legal logos, before MENU_MAIN.  Keep bringup in its
     * existing diagnostic path, and fail open to the title if g.bmp is absent
     * or malformed. The native PAK lease is closed for every visible frame. */
    bool initializeBootLegals()
    {
#if DOOMRPG_ESP32_BRINGUP_PROBES
        return false;
#else
        if (!engineConfigMappingsReady || !assetPackReady || !videoReady)
            return false;
        if (!EspNativeBootLegals_begin(millis()))
            return false;
        if (!EspNativeBootLegals_draw(PlatformVideo_framebuffer(),
                                     PlatformVideo_framebufferSizeBytes() /
                                         sizeof(uint16_t)) ||
            !PlatformVideo_present())
        {
            EspNativeBootLegals_cancel();
            return false;
        }
        bootLegalsTouchDown = input.touched();
        bootLegalsPending = true;
        Serial.printf("[BOOTLEGALS] DISPLAY panel=1/4 heap8=%u largest8=%u\n",
                      DoomRPG_getHeap8Free(), DoomRPG_getLargest8BitBlock());
        return true;
#endif
    }

    void serviceBootLegals()
    {
        const bool isDown = input.touched();
        const int next = EspNativeBootLegals_advance(
            millis(), isDown && !bootLegalsTouchDown ? 1 : 0);
        bootLegalsTouchDown = isDown;
        if (next == 0)
            return;
        if (next == 1 &&
            EspNativeBootLegals_draw(PlatformVideo_framebuffer(),
                                     PlatformVideo_framebufferSizeBytes() /
                                         sizeof(uint16_t)) &&
            PlatformVideo_present())
        {
            Serial.printf("[BOOTLEGALS] DISPLAY panel=%u/4 heap8=%u largest8=%u\n",
                          EspNativeBootLegals_currentPanel(),
                          DoomRPG_getHeap8Free(),
                          DoomRPG_getLargest8BitBlock());
            return;
        }
        if (next == 1)
            EspNativeBootLegals_cancel();
        bootLegalsPending = false;
        Serial.println("[BOOTLEGALS] HANDOFF main-menu pack=closed");
        initializeMainMenu();
    }

    void printSystemInfo()
    {
        Serial.println();
        Serial.println("=== Doom RPG CYD hardware bring-up ===");
#if defined(DOOMRPG_ESP32_TRACE_SPRITE)
        Serial.printf("[TRACEPROFILE] production-path sprite=%u optimization=release bringup=no timing=instrumented\n",
                      static_cast<unsigned int>(DOOMRPG_ESP32_TRACE_SPRITE));
#endif
        Serial.printf("Chip: %s, revision %u, cores %u\n", ESP.getChipModel(),
                      ESP.getChipRevision(), ESP.getChipCores());
        Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
        Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
        Serial.printf("Heap: %u free / %u total\n", ESP.getFreeHeap(),
                      ESP.getHeapSize());
        Serial.printf("Heap8: %u free, largest block %u\n",
                      DoomRPG_getHeap8Free(), DoomRPG_getLargest8BitBlock());
        Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
        Serial.println("[LEGACYINIT] STAGED runtime=ESP32-core/layout/startup legacy-DoomRPG_Init-anchor=retired");
        DoomRpgEngineMetrics metrics{};
        DoomRPG_getEngineMetrics(&metrics);
        Serial.printf("Engine structs: Render=%u Game=%u Canvas=%u Total=%u bytes\n",
                      metrics.render, metrics.game, metrics.doomCanvas,
                      metrics.totalInitialObjects);
        Serial.println("Upload target locked to /dev/ttyUSB0");
        Serial.printf("[TFT] Screen diagnostics=%s; game framebuffer owns visible output\n",
                      DOOMRPG_ESP32_SCREEN_DIAGNOSTICS ? "enabled" : "disabled");
    }

    void updateTouchDiagnostic()
    {
        if (!input.touched())
        {
            return;
        }

        const uint32_t now = millis();
        if (now - lastTouchUpdate < 80)
        {
            return;
        }
        lastTouchUpdate = now;

        PlatformTouchPoint point{};
        if (!input.readTouch(point))
        {
            return;
        }

        Serial.printf("[TOUCH] raw=%u,%u pressure=%u screen=%d,%d\n", point.rawX,
                      point.rawY, point.pressure, point.x, point.y);

#if DOOMRPG_ESP32_SCREEN_DIAGNOSTICS
        // SDL-B2: the old full-screen SDL test pattern is retired.
        // Retain the bounded direct-TFT touch coordinates overlay when
        // explicit screen diagnostics are enabled (normal profile = 0).
        char status[48];
        snprintf(status, sizeof(status), "%u,%u -> %d,%d", point.rawX, point.rawY,
                 point.x, point.y);
        drawLabel(114, "Touch:", status, TFT_CYAN);
        display.drawCircle(point.x, point.y, 5, TFT_CYAN);
        display.drawFastHLine(point.x - 8, point.y, 17, TFT_CYAN);
        display.drawFastVLine(point.x, point.y - 8, 17, TFT_CYAN);
#endif
    }

    void printHeartbeat()
    {
        const uint32_t now = millis();
        if (now - lastHeartbeat < 5000)
        {
            return;
        }
        lastHeartbeat = now;

        const char *pakState = assetResourcesReady
                                   ? "ready"
                                   : (assetPackReady ? "partial" : "unavailable");

        Serial.printf(
            "[ALIVE] uptime=%lu ms heap=%u heap8=%u largest8=%u SD=%s PAK=%s VIDEO=%s CORE=%s LAYOUT=%s PRERENDER=%s RENDER=%s MAPPINGS=%s MENU=%s touchIRQ=%s\n",
            now, ESP.getFreeHeap(), DoomRPG_getHeap8Free(),
            DoomRPG_getLargest8BitBlock(), sdReady ? "ready" : "unavailable",
            pakState, videoReady ? "ready" : "unavailable",
            engineCoreReady ? "ready" : "unavailable",
            engineLayoutReady ? "ready" : "unavailable",
            enginePreRenderReady ? "ready" : "unavailable",
            engineRenderStartupReady ? "ready" : "unavailable",
            engineConfigMappingsReady ? "ready" : "unavailable",
            engineMainMenuReady ? "ready" : "unavailable",
            input.touched() ? "active" : "idle");
    }

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(250);
    printSystemInfo();

    input.begin();

    display.begin();
    display.setRotation(cyd::kDisplayRotation);
    display.setSwapBytes(true);
    drawDiagnosticScreen();
    Serial.printf("[TFT] Ready, physical size=%dx%d diagnostics=%s\n",
                  display.width(), display.height(),
                  DOOMRPG_ESP32_SCREEN_DIAGNOSTICS ? "enabled" : "disabled");

    initializePlatformVideo();
#if DOOMRPG_RES60_P1_DIAGNOSTIC
    /* Fail closed before SD, PAK, audio or any 160x120 engine owner starts.
     * No world or HUD frame is ever written with an incompatible pitch. */
    if (!videoReady || PlatformVideo_framebuffer() == nullptr ||
        PlatformVideo_framebufferSizeBytes() != 55296U) {
        Serial.printf("[RES60P1] FAIL CLOSED ready=%u bytes=%u heap8=%u largest8=%u\n",
                      videoReady ? 1U : 0U,
                      (unsigned int)PlatformVideo_framebufferSizeBytes(),
                      (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                      (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return;
    }
    Serial.printf("[RES60P1] BOOT mode=VIDEO_ONLY logical=192x144 rgb565=55296 viewport3d=NOT_STARTED HUD=NOT_STARTED MENU=NOT_STARTED AUDIO=NOT_STARTED heap8=%u largest8=%u minHeap8=%u\n",
                  (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#if DOOMRPG_RES60_P3_WORLD
    initializeSdCard();
    if (!sdReady) {
        Serial.println("[RES60P3] FAIL CLOSED stage=SD_UNAVAILABLE no ZIP fallback");
        return;
    }
#if DOOMRPG_RES60_P5_TITLE
#if DOOMRPG_RES60_P6_MENU
    Serial.println("[RES60P6] BOOT mode=MAIN_MENU touch=TWO_TAP source=ORIGINAL_PAK start=BSP_PREVIEW_ONLY gameplay=NOT_STARTED audio=NOT_STARTED");
    PlatformInput_setTapCallback(res60MenuTapCallback);
    res60MenuArmed = -1;
    res60MenuVisible = PlatformVideo_showRes60MainMenu(res60MenuArmed);
    if (!res60MenuVisible) {
        Serial.println("[RES60P6] MENU-UNAVAILABLE fallback=MAP_INTRO-geometry");
        PlatformVideo_showRes60WorldPreview();
    }
#else
    Serial.println("[RES60P5] BOOT mode=ORIGINAL_TITLE touch=TOGGLE_BSP_PREVIEW gameplay=NOT_STARTED audio=NOT_STARTED");
    res60TitleVisible = PlatformVideo_showRes60TitlePreview();
    if (!res60TitleVisible) {
        Serial.println("[RES60P5] TITLE-UNAVAILABLE fallback=MAP_INTRO-geometry");
        PlatformVideo_showRes60WorldPreview();
    }
    res60TouchWasDown = input.touched();
#endif
#else
    Serial.println("[RES60P3] BOOT mode=INTRO_GEOMETRY_ONLY target=192x96@y24 source=/intro.bsp backing=PAK textures=NOT_STARTED gameplay=NOT_STARTED audio=NOT_STARTED touch=NOT_STARTED");
    PlatformVideo_showRes60WorldPreview();
#endif
#elif DOOMRPG_RES60_P2_LAYOUT
    Serial.println("[RES60P2] BOOT mode=LAYOUT_ONLY logical=192x144 hudTop=24 viewport=192x96@y24 hudBottom=24 engine=NOT_STARTED touch=NOT_STARTED audio=NOT_STARTED");
    PlatformVideo_showRes60LayoutPattern();
#else
    PlatformVideo_showTestPattern();
#endif
#if DOOMRPG_RES60_P6_MENU
    Serial.printf("[RES60P6] IDLE mode=MENU_OR_GEOMETRY heap8=%u largest8=%u minHeap8=%u gameplay=NOT_STARTED\n",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#elif DOOMRPG_RES60_P5_TITLE
    Serial.printf("[RES60P5] IDLE mode=TITLE_OR_GEOMETRY heap8=%u largest8=%u minHeap8=%u gameplay=NOT_STARTED\n",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#else
    Serial.printf("[RES60P1] IDLE mode=VIDEO_ONLY heap8=%u largest8=%u minHeap8=%u audio=NOT_STARTED map=NOT_LOADED\n",
                  (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#endif
    return;
#endif
    initializeSdCard();
    EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);
    /* SOUND=ON by default: drive DAC2 at digital silence before menus/intro.
     * No timer, SD read, ring or audio allocation here. */
    EspEntranceMusic_syncIdleOutput();
    initializeGameAssets();
    initializeEngineCore();
    initializeEngineLayout();
    initializeRenderStartup();
    initializeConfigMappings();
    if (!initializeBootLegals())
        initializeMainMenu();
    Serial.println("[READY] Bring-up alive; touch diagnostics are serial-only and TFT is reserved for the game framebuffer.");
}

void loop()
{
#if DOOMRPG_RES60_P1_DIAGNOSTIC
#if DOOMRPG_RES60_P5_TITLE
#if DOOMRPG_RES60_P6_MENU
    /* Input's existing 50ms released-edge debounce supplies one event per
     * tap, already calibrated to physical 320x240. No polling redraw. */
    (void)input.touched();
    if (res60TapPending) {
        const int16_t x = res60TapX, y = res60TapY;
        res60TapPending = false;
        if (res60MenuVisible) {
            const int8_t previous = res60MenuArmed;
            const EspRes60MenuTap action =
                EspRes60Menu_onTap(x, y, &res60MenuArmed);
            if (action == ESP_RES60_MENU_TAP_SELECTION) {
                if (!PlatformVideo_updateRes60MenuArmed(previous, res60MenuArmed)) {
                    res60MenuArmed = previous;
                    Serial.println("[RES60P6] FAIL CLOSED stage=selection-present");
                }
            } else if (action == ESP_RES60_MENU_TAP_START_PREVIEW) {
                Serial.println("[RES60P6] ACTION START target=MAP_INTRO_BSP_PREVIEW gameplay=NOT_STARTED");
#if DOOMRPG_RES60_P7_NAV
#if DOOMRPG_RES60_P8_TEXEL
                /* New preview session: forbid stale swatch across MENU/START. */
                EspRes60TextureProbe_reset();
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
                EspRes60InventoryCache_reset();
#endif
                if (PlatformVideo_showRes60WorldPreviewQuarterTurns(0U)) {
                    res60QuarterTurns = 0U;
                    res60MenuVisible = false;
                } else {
                    Serial.println("[RES60P7] FAIL CLOSED stage=initial-world-view");
                }
#else
                PlatformVideo_showRes60WorldPreview();
                res60MenuVisible = false;
#endif
            } else if (action == ESP_RES60_MENU_TAP_UNSUPPORTED) {
                Serial.printf("[RES60P6] ACTION BLOCKED item=%d reason=60pct-gameplay-route-not-ported\n",
                              (int)res60MenuArmed);
            } else if (action == ESP_RES60_MENU_TAP_INVALID) {
                Serial.printf("[RES60P6] TOUCH INVALID physical=%d,%d\n",
                              (int)x, (int)y);
            }
        } else {
#if DOOMRPG_RES60_P7_NAV
            const EspRes60NavTap tap = EspRes60Nav_hitPhysical(x,y);
            if (tap == ESP_RES60_NAV_LEFT || tap == ESP_RES60_NAV_RIGHT) {
                uint8_t next = res60QuarterTurns;
                if (!EspRes60Nav_applyTurn(res60QuarterTurns,tap,&next) ||
                    !PlatformVideo_showRes60WorldPreviewQuarterTurns(next)) {
                    Serial.printf("[RES60P7] TURN-REJECT direction=%s from=%u\n",
                         tap == ESP_RES60_NAV_LEFT ? "LEFT" : "RIGHT",
                         (unsigned)res60QuarterTurns);
                } else {
                    Serial.printf("[RES60P7] TURN direction=%s from=%u to=%u committed=yes\n",
                         tap == ESP_RES60_NAV_LEFT ? "LEFT" : "RIGHT",
                         (unsigned)res60QuarterTurns,(unsigned)next);
                    res60QuarterTurns = next;
                }
            } else if (tap == ESP_RES60_NAV_MENU) {
                res60MenuArmed = -1;
                Serial.println("[RES60P7] ACTION MENU from=BSP-preview");
                res60MenuVisible = PlatformVideo_showRes60MainMenu(-1);
                if (res60MenuVisible) {
                    res60QuarterTurns = 0U;
#if DOOMRPG_RES60_P8_TEXEL
                    EspRes60TextureProbe_reset();
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
                    EspRes60InventoryCache_reset();
#endif
                }
            } else {
                Serial.printf("[RES60P7] TOUCH IGNORED physical=%d,%d zone=world-or-border\n",
                              (int)x,(int)y);
            }
#else
            res60MenuArmed = -1;
            Serial.println("[RES60P6] TOUCH BSP-preview->MAIN_MENU");
            res60MenuVisible = PlatformVideo_showRes60MainMenu(-1);
#endif
        }
    }
    delay(20);
#else
    /* Edge-triggered toggle: not MENU_MAIN, not native gameplay. */
    const bool down = input.touched();
    if (down && !res60TouchWasDown) {
        if (res60TitleVisible) {
            Serial.println("[RES60P5] TOUCH title->BSP-preview (NOT gameplay)");
            PlatformVideo_showRes60WorldPreview();
            res60TitleVisible = false;
        } else {
            Serial.println("[RES60P5] TOUCH BSP-preview->original-title");
            res60TitleVisible = PlatformVideo_showRes60TitlePreview();
        }
    }
    res60TouchWasDown = down;
    delay(20);
#endif
#else
    delay(250);
#endif
    return;
#else
    if (bootLegalsPending)
    {
        serviceBootLegals();
        printHeartbeat();
        delay(5);
        return;
    }
    /* Release the one intro SD File/ring AFTER FINAL-CONTINUE has parked the
     * clock, but BEFORE the next clock service starts BSP/flash loading.
     * Preserve the native intro C files byte-exact (SDL-F8 regression). */
    EspEntranceMusic_stopIntroAtHandoff();
    Esp32IntroClock_service();
    DoomRPG_esp32MainMenuHelpService();
    if (EspNativeGameplaySession_canService())
    {
        EspNativeGameplaySessionCoordinator_service(doomRpg);
    }
    EspNativeResidentGameplay_serviceMenuExit(doomRpg);
    EspNativeStartup_serviceDeathRoute(doomRpg);
    /* SFX-P0: first validate the original WAV formats from verified PAK.
     * Exactly once per gameplay session, before music producer activity. */
    EspNativeSfxProbe_service(EspNativeGameplaySession_isActive());
    /* AUDIO-R2a: selected Entrance soundtrack, SD IMA-ADPCM 22050Hz.
     * Game-loop producer owns all SD reads/decoding; DAC timer ISR DRAM-only.
     * Automatically stop and close on session or map exit, fail closed. */
    EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);
    updateTouchDiagnostic();
    printHeartbeat();
    delay(5);
#endif
}
