#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "esp_asset_pack.h"
#include "esp_legacy_asset_source.h"
#include "esp_legacy_config_mappings_startup.h"
#include "engine_metrics.h"
#include "esp_native_gameplay_session.h"
#include "esp32_sdl_platform.h"
#include "menu_bsp_probe.h"
#include "native_intro_clock.h"
#include "native_main_menu_model.h"
#include "native_main_menu_touch_layout.h"
#include "platform_input.h"
#include "platform_video.h"
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
    DoomRpgCoreInitReport coreReport{};
    DoomRpgLayoutReport layoutReport{};
    uint32_t lastTouchUpdate = 0;
    uint32_t lastHeartbeat = 0;

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
                Serial.println("[LAYOUT] native PAK does not contain the HUD resources required by Hud_startup()");
            }
            drawLabel(186, "Engine:", "LAYOUT skipped", TFT_ORANGE);
            return;
        }

        engineLayoutReady = DoomRPG_startEngineLayout(&layoutReport) != 0;

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
            EspLegacyPrerenderStartup_start(engineLayoutReady ? 1 : 0) != 0;
        engineRenderStartupReady =
            EspRenderStartupBridge_start(enginePreRenderReady ? 1 : 0) != 0;

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
            EspLegacyConfigMappingsStartup_start(engineRenderStartupReady ? 1 : 0) != 0;

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
         * Keep the historical menu.bsp structural/render suite available only
         * in the explicit bring-up profile. Production no longer needs a 3D
         * menu map before painting the opaque native dashboard.
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

        engineMainMenuReady =
            doomRpg != nullptr &&
            DoomRPG_esp32MainMenuModelBuildMain(doomRpg) != 0 &&
            DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV) != 0;

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

    void printSystemInfo()
    {
        Serial.println();
        Serial.println("=== Doom RPG CYD hardware bring-up ===");
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
        Esp32Sdl_showTestPattern();
        Serial.println("[SDL] Diagnostic touch test pattern is on screen");

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
    Esp32Sdl_attachDisplay(&display);
    drawDiagnosticScreen();
    Serial.printf("[TFT] Ready, physical size=%dx%d diagnostics=%s\n",
                  display.width(), display.height(),
                  DOOMRPG_ESP32_SCREEN_DIAGNOSTICS ? "enabled" : "disabled");

    initializePlatformVideo();
    initializeSdCard();
    initializeGameAssets();
    initializeEngineCore();
    initializeEngineLayout();
    initializeRenderStartup();
    initializeConfigMappings();
    initializeMainMenu();
    Serial.println("[READY] Bring-up alive; touch diagnostics are serial-only and TFT is reserved for the game framebuffer.");
}

void loop()
{
    Esp32IntroClock_service();
    if (EspNativeGameplaySession_canService())
    {
        EspNativeGameplaySession_service(doomRpg);
    }
    updateTouchDiagnostic();
    printHeartbeat();
    delay(5);
}
