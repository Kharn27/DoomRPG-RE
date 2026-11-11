#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include "esp_native_audio_volume.h"
#include "esp_native_audio_volume_codec.h"
#include "esp_native_audio_midi.h"

namespace {
constexpr char kPrimary[] = "/DoomRPG-ESP32.audio.cfg";
constexpr char kTmp[] = "/DoomRPG-ESP32.audio.cfg.tmp";
constexpr char kBackup[] = "/DoomRPG-ESP32.audio.cfg.bak";
bool sdReady = false;

bool readExact(const char* path, uint8_t* percent) {
    if (!SD.exists(path) || percent == nullptr) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    uint8_t record[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES] = {};
    const bool ok = file.size() == sizeof(record) &&
                    file.read(record, sizeof(record)) == sizeof(record);
    file.close();
    return ok && EspNativeAudioVolumeCodec_decode(record, sizeof(record),
                                                   percent);
}
bool writeExact(const char* path, uint8_t percent) {
    uint8_t record[ESP_NATIVE_AUDIO_VOLUME_RECORD_BYTES];
    if (!EspNativeAudioVolumeCodec_encode(percent, record)) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    const bool ok = file.write(record, sizeof(record)) == sizeof(record);
    file.flush();
    const bool sized = file.size() == sizeof(record);
    file.close();
    return ok && sized;
}
bool verify(const char* path, uint8_t percent) {
    uint8_t saved = 0;
    return readExact(path, &saved) && saved == percent;
}
}

extern "C" void EspNativeAudioVolume_loadAtBoot(int mounted) {
    uint8_t loaded = 100U;
    sdReady = mounted != 0;
    (void)EspNativeAudioMidi_setVolume(100U);
    if (sdReady && readExact(kPrimary, &loaded)) {
        (void)EspNativeAudioMidi_setVolume(loaded);
        Serial.printf("[AUDIOVOLUME] LOAD source=primary percent=%u version=1 write=no\n",
                      (unsigned)loaded);
    } else if (sdReady && readExact(kBackup, &loaded)) {
        (void)EspNativeAudioMidi_setVolume(loaded);
        Serial.printf("[AUDIOVOLUME] LOAD source=backup percent=%u version=1 write=no\n",
                      (unsigned)loaded);
    } else {
        Serial.printf("[AUDIOVOLUME] LOAD source=default percent=100 sd=%d write=no\n",
                      (int)sdReady);
    }
}

extern "C" int EspNativeAudioVolume_savePercent(uint8_t percent) {
    uint8_t old = EspNativeAudioMidi_getVolume();
    if (!sdReady || !EspNativeAudioVolumeCodec_valid(percent)) {
        Serial.printf("[AUDIOVOLUME] SAVE rejected requested=%u sd=%d\n",
                      (unsigned)percent, (int)sdReady);
        return 0;
    }
    if (verify(kPrimary, percent)) {
        (void)EspNativeAudioMidi_setVolume(percent);
        Serial.printf("[AUDIOVOLUME] SAVE unchanged percent=%u write=no\n",
                      (unsigned)percent);
        return 1;
    }
    if (SD.exists(kTmp) && !SD.remove(kTmp)) return 0;
    if (!writeExact(kTmp, percent) || !verify(kTmp, percent)) {
        (void)SD.remove(kTmp);
        Serial.println("[AUDIOVOLUME] SAVE failed stage=temp");
        return 0;
    }

    bool moved = false;
    if (SD.exists(kPrimary)) {
        uint8_t stored, recovered;
        if (!readExact(kPrimary, &stored) &&
            readExact(kBackup, &recovered)) {
            if (!SD.remove(kPrimary)) {
                (void)SD.remove(kTmp);
                return 0;
            }
        } else {
            if (SD.exists(kBackup) && !SD.remove(kBackup)) {
                (void)SD.remove(kTmp);
                return 0;
            }
            if (!SD.rename(kPrimary, kBackup)) {
                (void)SD.remove(kTmp);
                return 0;
            }
            moved = true;
        }
    }
    if (!SD.rename(kTmp, kPrimary) || !verify(kPrimary, percent)) {
        (void)SD.remove(kTmp);
        (void)SD.remove(kPrimary);
        if (moved) (void)SD.rename(kBackup, kPrimary);
        Serial.println("[AUDIOVOLUME] SAVE failed stage=promote rollback=attempted");
        return 0;
    }
    if (SD.exists(kBackup)) (void)SD.remove(kBackup);
    (void)EspNativeAudioMidi_setVolume(percent);
    Serial.printf("[AUDIOVOLUME] SAVE result=committed old=%u percent=%u "
                  "version=1 bytes=10 path=%s\n",
                  (unsigned)old, (unsigned)percent, kPrimary);
    return 1;
}
