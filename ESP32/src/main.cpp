#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "engine_metrics.h"
#include "esp32_sdl_platform.h"
#include "platform_input.h"
#include "platform_video.h"
#include "soft_xpt2046.h"
#include "Z_Zip.h"

namespace
{

    TFT_eSPI display;
    SoftXpt2046 touchscreen(cyd::kTouchMosi, cyd::kTouchMiso,
                            cyd::kTouchClock, cyd::kTouchCs, cyd::kTouchIrq);
    PlatformInput input(touchscreen);

    bool sdReady = false;
    bool archiveReady = false;
    bool videoReady = false;
    bool videoTestShown = false;
    uint32_t lastTouchUpdate = 0;
    uint32_t lastHeartbeat = 0;

    void drawLabel(int16_t y, const char *label, const char *value,
                   uint16_t color)
    {
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setCursor(8, y);
        display.print(label);
        display.setTextColor(color, TFT_BLACK);
        display.setCursor(106, y);
        display.print(value);
        display.print("                    ");
    }

    void drawDiagnosticScreen()
    {
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

        display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        display.setCursor(8, 214);
        display.print("Serial: 115200 / ttyUSB0");
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
        char status[32];
        snprintf(status, sizeof(status), "%d ZIP entries", zipFile.entry_count);
        drawLabel(138, "Game data:", status, archiveReady ? TFT_GREEN : TFT_RED);
        Serial.printf("[DATA] DoomRPG.zip indexed, entries=%d\n", zipFile.entry_count);
    }

    void initializePlatformVideo()
    {
        videoReady = PlatformVideo_begin(&display);
        drawLabel(162, "Video:", videoReady ? "160x120 x2" : "alloc failed",
                  videoReady ? TFT_GREEN : TFT_RED);
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
        Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
        Serial.printf("Doom engine linked at: 0x%08x\n",
                      static_cast<unsigned int>(DoomRPG_engineLinkAnchor()));
        DoomRpgEngineMetrics metrics{};
        DoomRPG_getEngineMetrics(&metrics);
        Serial.printf("Engine structs: Render=%u Game=%u Canvas=%u Total=%u bytes\n",
                      metrics.render, metrics.game, metrics.doomCanvas,
                      metrics.totalInitialObjects);
        Serial.println("Upload target locked to /dev/ttyUSB0");
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

        if (!videoTestShown && videoReady)
        {
            videoTestShown = true;
            PlatformVideo_showTestPattern();
            Serial.println("[VIDEO] Touch-triggered 160x120 exact-2x test is on screen");
        }

        char status[48];
        snprintf(status, sizeof(status), "%u,%u -> %d,%d", point.rawX, point.rawY,
                 point.x, point.y);
        drawLabel(114, "Touch:", status, TFT_CYAN);
        display.drawCircle(point.x, point.y, 5, TFT_CYAN);
        display.drawFastHLine(point.x - 8, point.y, 17, TFT_CYAN);
        display.drawFastVLine(point.x, point.y - 8, 17, TFT_CYAN);
    }

    void printHeartbeat()
    {
        const uint32_t now = millis();
        if (now - lastHeartbeat < 5000)
        {
            return;
        }
        lastHeartbeat = now;
        Serial.printf("[ALIVE] uptime=%lu ms heap=%u SD=%s ZIP=%s VIDEO=%s touchIRQ=%s\n",
                      now, ESP.getFreeHeap(), sdReady ? "ready" : "unavailable",
                      archiveReady ? "ready" : "unavailable",
                      videoReady ? "ready" : "unavailable",
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
    Serial.printf("[TFT] Ready, logical size=%dx%d\n", display.width(),
                  display.height());

    initializePlatformVideo();
    initializeSdCard();
    initializeGameArchive();
    Serial.println("[READY] Touch the panel; raw samples will appear here.");
}

void loop()
{
    updateTouchDiagnostic();
    printHeartbeat();
    delay(5);
}
