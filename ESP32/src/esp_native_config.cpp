#include <Arduino.h>
#include <SD.h>
#include <stdint.h>

#include "esp_native_config.h"
#include "esp_native_config_codec.h"
#include "esp_native_backlight.h"
#include "esp_native_orientation.h"
#include "esp_main_menu_skin.h"

namespace {
constexpr char kConfigPath[] = "/DoomRPG-ESP32.cfg";
constexpr char kTempPath[] = "/DoomRPG-ESP32.cfg.tmp";
constexpr char kBackupPath[] = "/DoomRPG-ESP32.cfg.bak";
bool configSdReady = false;

struct ConfigSettings {
    uint8_t brightness;
    uint8_t orientation;
    uint8_t theme;
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
        !EspNativeConfigCodec_decodeWithTheme(bytes, sizeof(bytes),
                                     &parsed.brightness, &parsed.orientation,
                                     &parsed.theme))
        return false;
    parsed.version = bytes[4];
    *out = parsed;
    return true;
}

bool writeExact(const char* path, uint8_t percent, uint8_t orientation,
                uint8_t theme) {
    uint8_t bytes[ESP_NATIVE_CONFIG_BYTES];
    if (!EspNativeConfigCodec_encodeWithTheme(percent, orientation, theme, bytes)) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    const bool wrote = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
    file.flush();
    const bool sizeOk = file.size() == sizeof(bytes);
    file.close();
    return wrote && sizeOk;
}

bool verify(const char* path, uint8_t percent, uint8_t orientation,
            uint8_t theme) {
    ConfigSettings actual{};
    return readExact(path, &actual) &&
           actual.version == ESP_NATIVE_CONFIG_VERSION &&
           actual.brightness == percent && actual.orientation == orientation &&
           actual.theme == theme;
}

void applyLoaded(const char* source, const ConfigSettings& loaded,
                 const char* primaryState) {
    const int rotationOk = Esp32Orientation_set(loaded.orientation);
    const int pwmOk = Esp32Backlight_setPercent(loaded.brightness);
    const int themeOk = EspMainMenuSkin_setTheme(loaded.theme);
    Serial.printf("[NATIVECONFIG] LOAD source=%s result=%s version=%u bytes=12 brightness=%u orientation=%s theme=%u primary=%s write=no\n",
                  source, rotationOk && pwmOk && themeOk ? "valid" : "hardware-rejected",
                  (unsigned)loaded.version, (unsigned)loaded.brightness,
                  loaded.orientation ? "INVERSE" : "NORMAL",
                  (unsigned)loaded.theme, primaryState);
}

/* All SD mutations are here. Never discard an intact backup when primary
 * is corrupt, and never touch the active physical orientation on failure. */
int saveSettings(uint8_t percent, uint8_t orientation, uint8_t theme) {
    bool oldMoved = false;
    if (!configSdReady || !Esp32Backlight_isReady() ||
        !Esp32Orientation_isReady() ||
        (percent != 25U && percent != 50U && percent != 75U && percent != 100U) ||
        !Esp32Orientation_valid(orientation) ||
        theme > ESP_MAIN_MENU_THEME_PHOSPHORE) {
        Serial.printf("[NATIVECONFIG] SAVE rejected brightness=%u orientation=%u sd=%d pwm=%d orientationReady=%d\n",
                      (unsigned)percent, (unsigned)orientation, configSdReady,
                      Esp32Backlight_isReady(), Esp32Orientation_isReady());
        return 0;
    }

    if (verify(kConfigPath, percent, orientation, theme)) {
        Serial.printf("[NATIVECONFIG] SAVE unchanged brightness=%u orientation=%u version=3 theme=%u write=no\n",
                      (unsigned)percent, (unsigned)orientation, (unsigned)theme);
        return 1;
    }

    if (SD.exists(kTempPath) && !SD.remove(kTempPath)) {
        Serial.println("[NATIVECONFIG] SAVE failed stage=stale-temp-remove");
        return 0;
    }
    if (!writeExact(kTempPath, percent, orientation, theme) ||
        !verify(kTempPath, percent, orientation, theme)) {
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
        !verify(kConfigPath, percent, orientation, theme)) {
        (void)SD.remove(kTempPath);
        (void)SD.remove(kConfigPath);
        if (oldMoved) (void)SD.rename(kBackupPath, kConfigPath);
        Serial.println("[NATIVECONFIG] SAVE failed stage=promote-or-readback rollback=attempted");
        return 0;
    }

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    Serial.printf("[NATIVECONFIG] SAVE result=committed version=3 brightness=%u orientation=%s theme=%u bytes=12 path=%s backup=retired\n",
                  (unsigned)percent, orientation ? "INVERSE" : "NORMAL",
                  (unsigned)theme, kConfigPath);
    return 1;
}
} // namespace

extern "C" void EspNativeConfig_loadAtBoot(int sdMounted) {
    ConfigSettings loaded{};
    configSdReady = sdMounted != 0;
    /* Defaults are applied before any source is opened, even without SD. */
    (void)EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC);
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
    return saveSettings(percent, Esp32Orientation_get(),
                        EspMainMenuSkin_getTheme());
}

extern "C" int EspNativeConfig_saveOrientation(uint8_t orientation) {
    return saveSettings(Esp32Backlight_percent(), orientation,
                        EspMainMenuSkin_getTheme());
}

extern "C" int EspNativeConfig_saveTheme(uint8_t theme) {
    return saveSettings(Esp32Backlight_percent(), Esp32Orientation_get(), theme);
}
