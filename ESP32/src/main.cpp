#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "config_mappings_probe.h"
#include "engine_metrics.h"
#include "esp32_sdl_platform.h"
#include "menu_bsp_probe.h"
#include "native_intro_clock.h"
#include "platform_input.h"
#include "platform_video.h"
#include "esp_legacy_prerender_startup.h"
#include "esp_render_startup_bridge.h"
#include "soft_xpt2046.h"
#include "Z_Zip.h"

#ifndef DOOMRPG_ESP32_SCREEN_DIAGNOSTICS
#define DOOMRPG_ESP32_SCREEN_DIAGNOSTICS 0
#endif

namespace
{

    TFT_eSPI display;
    SoftXpt2046 touchscreen(cyd::kTouchMosi, cyd::kTouchMiso,
                            cyd::kTouchClock, cyd::kTouchCs, cyd::kTouchIrq);
    PlatformInput input(touchscreen);

    bool sdReady = false;
    bool archiveReady = false;
    bool archiveResourcesReady = false;
    bool videoReady = false;
    bool engineCoreReady = false;
    bool engineLayoutReady = false;
    bool enginePreRenderReady = false;
    bool engineRenderStartupReady = false;
    bool engineConfigMappingsReady = false;
    bool engineMenuBspReady = false;
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

    bool archiveContains(const char *name)
    {
        if (!archiveReady || zipFile.entry == nullptr)
        {
            return false;
        }

        for (int i = 0; i < zipFile.entry_count; ++i)
        {
            const zip_entry_t &entry = zipFile.entry[i];
            if (entry.name != nullptr && SDL_strcasecmp(name, entry.name) == 0)
            {
                return true;
            }
        }
        return false;
    }

    void printArchiveEntries()
    {
        Serial.printf("[DATA] ZIP directory (%d entries):\n", zipFile.entry_count);
        for (int i = 0; i < zipFile.entry_count; ++i)
        {
            const zip_entry_t &entry = zipFile.entry[i];
            Serial.printf("[DATA]   [%d] %s csize=%d usize=%d\n", i,
                          entry.name != nullptr ? entry.name : "<null>",
                          entry.csize, entry.usize);
        }
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
            if (!archiveContains(name))
            {
                Serial.printf("[DATA] MISSING required HUD resource: %s\n", name);
                allPresent = false;
            }
        }

        if (!allPresent)
        {
            printArchiveEntries();
            Serial.println("[DATA] HUD resource preflight FAILED");
            Serial.println("[DATA] Expected a DoomRPG.zip generated from the original doomrpg.bar with BarToZip");
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

    void initializeGameArchive()
    {
        if (!sdReady)
        {
            drawLabel(138, "Game data:", "SD unavailable", TFT_ORANGE);
            return;
        }
        if (!SD.exists("/DoomRPG.zip"))
        {
            drawLabel(138, "Game data:", "DoomRPG.zip missing", TFT_ORANGE);
            Serial.println("[DATA] /DoomRPG.zip not found on SD card");
            return;
        }

        openZipFile("/sd/DoomRPG.zip", &zipFile);
        archiveReady = zipFile.entry_count > 0;
        Serial.printf("[DATA] DoomRPG.zip indexed, entries=%d\n", zipFile.entry_count);

        if (!archiveReady)
        {
            drawLabel(138, "Game data:", "ZIP empty", TFT_RED);
            return;
        }

        archiveResourcesReady = validateLayoutResources();

        char status[32];
        if (archiveResourcesReady)
        {
            snprintf(status, sizeof(status), "%d ZIP entries", zipFile.entry_count);
            drawLabel(138, "Game data:", status, TFT_GREEN);
        }
        else
        {
            snprintf(status, sizeof(status), "ZIP incomplete (%d)", zipFile.entry_count);
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

        if (!engineCoreReady || !archiveResourcesReady || !videoReady)
        {
            Serial.println("[LAYOUT] Prerequisite unavailable; probe skipped safely");
            if (!archiveResourcesReady)
            {
                Serial.println("[LAYOUT] DoomRPG.zip does not contain the HUD resources required by Hud_startup()");
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
            DoomRPG_probeConfigAndMappings(engineRenderStartupReady ? 1 : 0) != 0;

        if (engineConfigMappingsReady)
        {
            drawLabel(186, "Engine:", "MAPPINGS OK", TFT_GREEN);
        }
        else if (engineRenderStartupReady)
        {
            drawLabel(186, "Engine:", "MAPPINGS FAIL", TFT_RED);
        }
    }

    void initializeMenuBspProbe()
    {
        engineMenuBspReady =
            DoomRPG_probeMenuBspHeader(engineConfigMappingsReady ? 1 : 0) != 0;

        if (engineMenuBspReady)
        {
            drawLabel(186, "Engine:", "MENU BSP OK", TFT_GREEN);
        }
        else if (engineConfigMappingsReady)
        {
            drawLabel(186, "Engine:", "MENU BSP FAIL", TFT_RED);
        }
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
        Serial.printf("Doom engine linked at: 0x%08x\n",
                      static_cast<unsigned int>(DoomRPG_engineLinkAnchor()));
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

        const char *zipState = archiveResourcesReady
                                   ? "ready"
                                   : (archiveReady ? "partial" : "unavailable");

        Serial.printf(
            "[ALIVE] uptime=%lu ms heap=%u heap8=%u largest8=%u SD=%s ZIP=%s VIDEO=%s CORE=%s LAYOUT=%s PRERENDER=%s RENDER=%s MAPPINGS=%s MENUBSP=%s touchIRQ=%s\n",
            now, ESP.getFreeHeap(), DoomRPG_getHeap8Free(),
            DoomRPG_getLargest8BitBlock(), sdReady ? "ready" : "unavailable",
            zipState, videoReady ? "ready" : "unavailable",
            engineCoreReady ? "ready" : "unavailable",
            engineLayoutReady ? "ready" : "unavailable",
            enginePreRenderReady ? "ready" : "unavailable",
            engineRenderStartupReady ? "ready" : "unavailable",
            engineConfigMappingsReady ? "ready" : "unavailable",
            engineMenuBspReady ? "ready" : "unavailable",
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
    initializeGameArchive();
    initializeEngineCore();
    initializeEngineLayout();
    initializeRenderStartup();
    initializeConfigMappings();
    initializeMenuBspProbe();
    Serial.println("[READY] Bring-up alive; touch diagnostics are serial-only and TFT is reserved for the game framebuffer.");
}

void loop()
{
    Esp32IntroClock_service();
    updateTouchDiagnostic();
    printHeartbeat();
    delay(5);
}
