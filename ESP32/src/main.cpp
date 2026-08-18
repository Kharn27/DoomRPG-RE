#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "engine_metrics.h"
#include "esp32_sdl_platform.h"
#include "soft_xpt2046.h"
#include "Z_Zip.h"

namespace {

TFT_eSPI display;
SoftXpt2046 touchscreen(cyd::kTouchMosi, cyd::kTouchMiso,
                        cyd::kTouchClock, cyd::kTouchCs, cyd::kTouchIrq);

bool sdReady = false;
bool archiveReady = false;
bool rendererTestShown = false;
uint32_t lastTouchUpdate = 0;
uint32_t lastHeartbeat = 0;

int16_t mapTouchAxis(uint16_t raw, uint16_t rawMinimum, uint16_t rawMaximum,
                     int16_t screenMaximum) {
    const uint16_t clamped = constrain(raw, rawMinimum, rawMaximum);
    return static_cast<int16_t>(
        (static_cast<uint32_t>(clamped - rawMinimum) * screenMaximum) /
        (rawMaximum - rawMinimum));
}

void drawLabel(int16_t y, const char* label, const char* value,
               uint16_t color) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(8, y);
    display.print(label);
    display.setTextColor(color, TFT_BLACK);
    display.setCursor(106, y);
    display.print(value);
    display.print("                    ");
}

void drawDiagnosticScreen() {
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

    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(8, 214);
    display.print("Serial: 115200 / ttyUSB0");
}

void initializeSdCard() {
    pinMode(cyd::kSdCs, OUTPUT);
    digitalWrite(cyd::kSdCs, HIGH);
    SPI.begin(cyd::kSdClock, cyd::kSdMiso, cyd::kSdMosi, cyd::kSdCs);

    sdReady = SD.begin(cyd::kSdCs, SPI, cyd::kSdFrequency);
    if (!sdReady) {
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

void initializeGameArchive() {
    if (!sdReady) {
        drawLabel(138, "Game data:", "SD unavailable", TFT_ORANGE);
        return;
    }
    if (!SD.exists("/DoomRPG.zip")) {
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

void printSystemInfo() {
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

void updateTouchDiagnostic() {
    if (!touchscreen.touched()) {
        return;
    }

    const uint32_t now = millis();
    if (now - lastTouchUpdate < 80) {
        return;
    }
    lastTouchUpdate = now;

    TouchSample sample{};
    if (!touchscreen.read(sample)) {
        return;
    }

    const int16_t screenX = mapTouchAxis(sample.x, cyd::kTouchRawMinX,
                                         cyd::kTouchRawMaxX,
                                         cyd::kScreenWidth - 1);
    const int16_t screenY = mapTouchAxis(sample.y, cyd::kTouchRawMinY,
                                         cyd::kTouchRawMaxY,
                                         cyd::kScreenHeight - 1);
    Serial.printf("[TOUCH] raw=%u,%u pressure=%u screen=%d,%d\n", sample.x,
                  sample.y, sample.pressure, screenX, screenY);

    if (!rendererTestShown) {
        rendererTestShown = true;
        Esp32Sdl_showTestPattern();
        // Pure TFT_eSPI bars below the SDL bars provide a direct colour and
        // byte-order comparison on the same panel.
        display.fillRect(8, 180, 96, 18, TFT_RED);
        display.fillRect(112, 180, 96, 18, TFT_GREEN);
        display.fillRect(216, 180, 96, 18, TFT_BLUE);
        display.setTextSize(1);
        display.setTextColor(TFT_CYAN, TFT_BLACK);
        display.setCursor(88, 220);
        display.print("SDL top / TFT bottom");
        Serial.println("[SDL] Touch-triggered renderer test is on screen");
    }

    char status[48];
    snprintf(status, sizeof(status), "%u,%u -> %d,%d", sample.x, sample.y,
             screenX, screenY);
    drawLabel(114, "Touch:", status, TFT_CYAN);
    display.drawCircle(screenX, screenY, 5, TFT_CYAN);
    display.drawFastHLine(screenX - 8, screenY, 17, TFT_CYAN);
    display.drawFastVLine(screenX, screenY - 8, 17, TFT_CYAN);
}

void printHeartbeat() {
    const uint32_t now = millis();
    if (now - lastHeartbeat < 5000) {
        return;
    }
    lastHeartbeat = now;
    Serial.printf("[ALIVE] uptime=%lu ms heap=%u SD=%s ZIP=%s touchIRQ=%s\n", now,
                  ESP.getFreeHeap(), sdReady ? "ready" : "unavailable",
                  archiveReady ? "ready" : "unavailable",
                  touchscreen.touched() ? "active" : "idle");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    printSystemInfo();

    touchscreen.begin();

    display.begin();
    display.setRotation(cyd::kDisplayRotation);
    display.setSwapBytes(true);
    Esp32Sdl_attachDisplay(&display);
    drawDiagnosticScreen();
    Serial.printf("[TFT] Ready, logical size=%dx%d\n", display.width(),
                  display.height());

    initializeSdCard();
    initializeGameArchive();
    Serial.println("[READY] Touch the panel; raw samples will appear here.");
}

void loop() {
    updateTouchDiagnostic();
    printHeartbeat();
    delay(5);
}
