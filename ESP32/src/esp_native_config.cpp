#include <Arduino.h>
#include <SD.h>
#include <stdint.h>

#include "esp_native_config.h"
#include "esp_native_config_codec.h"
#include "esp_native_backlight.h"

namespace {
constexpr char kConfigPath[] = "/DoomRPG-ESP32.cfg";
constexpr char kTempPath[] = "/DoomRPG-ESP32.cfg.tmp";
constexpr char kBackupPath[] = "/DoomRPG-ESP32.cfg.bak";
bool configSdReady = false;

bool readExact(const char* path, uint8_t* outPercent) {
    if (!SD.exists(path)) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    uint8_t bytes[ESP_NATIVE_CONFIG_BYTES];
    const bool exactLength = file.size() == ESP_NATIVE_CONFIG_BYTES;
    const bool readOk = exactLength &&
        file.read(bytes, sizeof(bytes)) == sizeof(bytes);
    file.close();
    return readOk &&
           EspNativeConfigCodec_decodeBrightness(
               bytes, sizeof(bytes), outPercent) != 0;
}

bool writeExact(const char* path, uint8_t percent) {
    uint8_t bytes[ESP_NATIVE_CONFIG_BYTES];
    if (!EspNativeConfigCodec_encodeBrightness(percent, bytes)) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    const bool wrote = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
    file.flush();
    const bool sizeOk = file.size() == sizeof(bytes);
    file.close();
    return wrote && sizeOk;
}

bool verify(const char* path, uint8_t expected) {
    uint8_t actual = 0U;
    return readExact(path, &actual) && actual == expected;
}
}

extern "C" void EspNativeConfig_loadAtBoot(int sdMounted) {
    uint8_t loaded = 100U;
    configSdReady = sdMounted != 0;

    if (!configSdReady) {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=sd-unavailable percent=100 write=no");
        return;
    }
    if (readExact(kConfigPath, &loaded)) {
        if (Esp32Backlight_setPercent(loaded)) {
            Serial.printf("[NATIVECONFIG] LOAD source=primary result=valid version=1 bytes=12 brightness=%u write=no\n",
                          (unsigned int)loaded);
        } else {
            Serial.println("[NATIVECONFIG] LOAD source=primary result=pwm-unavailable default=100 write=no");
        }
        return;
    }

    const bool primaryExists = SD.exists(kConfigPath);
    if (readExact(kBackupPath, &loaded)) {
        if (Esp32Backlight_setPercent(loaded)) {
            Serial.printf("[NATIVECONFIG] LOAD source=backup result=recovered brightness=%u primary=%s write=no\n",
                          (unsigned int)loaded, primaryExists ? "invalid" : "missing");
        } else {
            Serial.println("[NATIVECONFIG] LOAD source=backup result=pwm-unavailable default=100 write=no");
        }
        return;
    }
    if (!primaryExists && !SD.exists(kBackupPath)) {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=first-run-missing percent=100 write=no");
    } else {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=invalid-or-incomplete percent=100 write=no");
    }
}

extern "C" int EspNativeConfig_saveBrightness(uint8_t percent) {
    bool oldMoved = false;

    if (!configSdReady || !Esp32Backlight_isReady() ||
        (percent != 25U && percent != 50U &&
         percent != 75U && percent != 100U)) {
        Serial.printf("[NATIVECONFIG] SAVE rejected percent=%u sd=%d pwm=%d\n",
                      (unsigned int)percent, configSdReady ? 1 : 0,
                      Esp32Backlight_isReady());
        return 0;
    }

    if (verify(kConfigPath, percent)) {
        Serial.printf("[NATIVECONFIG] SAVE unchanged percent=%u bytes=12 write=no\n",
                      (unsigned int)percent);
        return 1;
    }

    if (SD.exists(kTempPath) && !SD.remove(kTempPath)) {
        Serial.println("[NATIVECONFIG] SAVE failed stage=stale-temp-remove");
        return 0;
    }
    if (!writeExact(kTempPath, percent) || !verify(kTempPath, percent)) {
        (void)SD.remove(kTempPath);
        Serial.println("[NATIVECONFIG] SAVE failed stage=temp-write-verify");
        return 0;
    }

    if (SD.exists(kConfigPath)) {
        uint8_t stored = 0U;
        uint8_t recovered = 0U;
        /* If an earlier interrupted save left an intact backup and an
         * invalid primary, do not destroy the last recoverable settings.
         * A failed promotion must leave the valid backup available at boot. */
        if (!readExact(kConfigPath, &stored) &&
            readExact(kBackupPath, &recovered)) {
            if (!SD.remove(kConfigPath)) {
                (void)SD.remove(kTempPath);
                Serial.println("[NATIVECONFIG] SAVE failed stage=invalid-primary-remove backup=kept");
                return 0;
            }
        } else {
            if (SD.exists(kBackupPath) && !SD.remove(kBackupPath)) {
                (void)SD.remove(kTempPath);
                Serial.println("[NATIVECONFIG] SAVE failed stage=stale-backup-remove");
                return 0;
            }
            if (!SD.rename(kConfigPath, kBackupPath)) {
                (void)SD.remove(kTempPath);
                Serial.println("[NATIVECONFIG] SAVE failed stage=backup-rename");
                return 0;
            }
            oldMoved = true;
        }
    }

    if (!SD.rename(kTempPath, kConfigPath) ||
        !verify(kConfigPath, percent)) {
        (void)SD.remove(kTempPath);
        (void)SD.remove(kConfigPath);
        if (oldMoved) (void)SD.rename(kBackupPath, kConfigPath);
        Serial.println("[NATIVECONFIG] SAVE failed stage=promote-or-readback rollback=attempted");
        return 0;
    }

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    Serial.printf("[NATIVECONFIG] SAVE result=committed version=1 percent=%u bytes=12 path=%s backup=retired\n",
                  (unsigned int)percent, kConfigPath);
    return 1;
}
