#include <Arduino.h>
#include <SD.h>
#include <stdint.h>

#include "esp_native_config.h"
#include "esp_native_config_codec.h"
#include "esp_native_backlight.h"
#include "esp_native_orientation.h"

namespace {
constexpr char kConfigPath[] = "/DoomRPG-ESP32.cfg";
constexpr char kTempPath[] = "/DoomRPG-ESP32.cfg.tmp";
constexpr char kBackupPath[] = "/DoomRPG-ESP32.cfg.bak";
bool configSdReady = false;

struct ConfigSettings {
    uint8_t brightness;
    uint8_t orientation;
    uint8_t version;
};

bool readExact(const char* path, ConfigSettings* out) {
    if (out == nullptr || !SD.exists(path)) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    uint8_t bytes[ESP_NATIVE_CONFIG_BYTES];
    const bool exactLength = file.size() == ESP_NATIVE_CONFIG_BYTES;
    const bool readOk = exactLength &&
        file.read(bytes, sizeof(bytes)) == sizeof(bytes);
    file.close();
    ConfigSettings parsed{};
    if (!readOk ||
        !EspNativeConfigCodec_decode(bytes, sizeof(bytes),
                                     &parsed.brightness, &parsed.orientation))
        return false;
    parsed.version = bytes[4];
    *out = parsed;
    return true;
}

bool writeExact(const char* path, uint8_t percent, uint8_t orientation) {
    uint8_t bytes[ESP_NATIVE_CONFIG_BYTES];
    if (!EspNativeConfigCodec_encode(percent, orientation, bytes)) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    const bool wrote = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
    file.flush();
    const bool sizeOk = file.size() == sizeof(bytes);
    file.close();
    return wrote && sizeOk;
}

bool verify(const char* path, uint8_t percent, uint8_t orientation) {
    ConfigSettings actual{};
    return readExact(path, &actual) &&
           actual.version == ESP_NATIVE_CONFIG_VERSION &&
           actual.brightness == percent && actual.orientation == orientation;
}

void applyLoaded(const char* source, const ConfigSettings& loaded,
                 const char* primaryState) {
    const int rotationOk = Esp32Orientation_set(loaded.orientation);
    const int pwmOk = Esp32Backlight_setPercent(loaded.brightness);
    Serial.printf("[NATIVECONFIG] LOAD source=%s result=%s version=%u bytes=12 brightness=%u orientation=%s primary=%s write=no\n",
                  source, rotationOk && pwmOk ? "valid" : "hardware-rejected",
                  (unsigned)loaded.version, (unsigned)loaded.brightness,
                  loaded.orientation ? "INVERSE" : "NORMAL", primaryState);
}

/* All SD mutations are here. Never discard an intact backup when primary
 * is corrupt, and never touch the active physical orientation on failure. */
int saveSettings(uint8_t percent, uint8_t orientation) {
    bool oldMoved = false;
    if (!configSdReady || !Esp32Backlight_isReady() ||
        !Esp32Orientation_isReady() ||
        (percent != 25U && percent != 50U && percent != 75U && percent != 100U) ||
        !Esp32Orientation_valid(orientation)) {
        Serial.printf("[NATIVECONFIG] SAVE rejected brightness=%u orientation=%u sd=%d pwm=%d orientationReady=%d\n",
                      (unsigned)percent, (unsigned)orientation, configSdReady,
                      Esp32Backlight_isReady(), Esp32Orientation_isReady());
        return 0;
    }

    if (verify(kConfigPath, percent, orientation)) {
        Serial.printf("[NATIVECONFIG] SAVE unchanged brightness=%u orientation=%u version=2 write=no\n",
                      (unsigned)percent, (unsigned)orientation);
        return 1;
    }

    if (SD.exists(kTempPath) && !SD.remove(kTempPath)) {
        Serial.println("[NATIVECONFIG] SAVE failed stage=stale-temp-remove");
        return 0;
    }
    if (!writeExact(kTempPath, percent, orientation) ||
        !verify(kTempPath, percent, orientation)) {
        (void)SD.remove(kTempPath);
        Serial.println("[NATIVECONFIG] SAVE failed stage=temp-write-verify");
        return 0;
    }

    if (SD.exists(kConfigPath)) {
        ConfigSettings stored{};
        ConfigSettings recovered{};
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
        !verify(kConfigPath, percent, orientation)) {
        (void)SD.remove(kTempPath);
        (void)SD.remove(kConfigPath);
        if (oldMoved) (void)SD.rename(kBackupPath, kConfigPath);
        Serial.println("[NATIVECONFIG] SAVE failed stage=promote-or-readback rollback=attempted");
        return 0;
    }

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    Serial.printf("[NATIVECONFIG] SAVE result=committed version=2 brightness=%u orientation=%s bytes=12 path=%s backup=retired\n",
                  (unsigned)percent, orientation ? "INVERSE" : "NORMAL", kConfigPath);
    return 1;
}
} // namespace

extern "C" void EspNativeConfig_loadAtBoot(int sdMounted) {
    ConfigSettings loaded{};
    configSdReady = sdMounted != 0;
    if (!configSdReady) {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=sd-unavailable percent=100 orientation=NORMAL write=no");
        return;
    }
    if (readExact(kConfigPath, &loaded)) {
        applyLoaded("primary", loaded, "valid");
        return;
    }

    const bool primaryExists = SD.exists(kConfigPath);
    if (readExact(kBackupPath, &loaded)) {
        applyLoaded("backup", loaded, primaryExists ? "invalid" : "missing");
        return;
    }
    if (!primaryExists && !SD.exists(kBackupPath)) {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=first-run-missing percent=100 orientation=NORMAL write=no");
    } else {
        Serial.println("[NATIVECONFIG] LOAD source=default reason=invalid-or-incomplete percent=100 orientation=NORMAL write=no");
    }
}

extern "C" int EspNativeConfig_saveBrightness(uint8_t percent) {
    return saveSettings(percent, Esp32Orientation_get());
}

extern "C" int EspNativeConfig_saveOrientation(uint8_t orientation) {
    return saveSettings(Esp32Backlight_percent(), orientation);
}
