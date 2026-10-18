#!/usr/bin/env python3
"""Exercise native config record codec and explicit desktop Config retirement.

Physical SD rename and PWM are checked on the classic CYD; host checks wire
format, strict decode validation, and startup/owner invariants.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
INC = ROOT / "ESP32/include"
start = (SRC / "main.cpp").read_text()
owner = (SRC / "esp_native_config.cpp").read_text()
bridge = (SRC / "esp_legacy_game_bridge.c").read_text()
startup = (SRC / "esp_legacy_config_mappings_startup.c").read_text()
menu = (SRC / "native_main_menu_options_back.c").read_text()
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
old_tu = SRC / "esp_doomrpg_config_read.c"

assert not old_tu.exists()
assert "Game_loadConfig(" not in bridge
assert "Game_loadConfig(" not in startup
assert 'SDL_RWFromFile("Config"' not in startup
assert "retired_config_readers" in builder
assert "File_readByte(SDL_RWops* rw)" in builder
assert "File_readInt(SDL_RWops* rw)" in builder
assert 'EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);' in start
assert start.index("initializePlatformVideo();") < start.index("initializeSdCard();")
assert start.index("initializeSdCard();") < start.index("EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);")
assert start.index("EspNativeConfig_loadAtBoot(sdReady ? 1 : 0);") < start.index("initializeGameAssets();")
assert 'constexpr char kConfigPath[] = "/DoomRPG-ESP32.cfg";' in owner
assert 'constexpr char kTempPath[] = "/DoomRPG-ESP32.cfg.tmp";' in owner
assert 'constexpr char kBackupPath[] = "/DoomRPG-ESP32.cfg.bak";' in owner
assert "reason=first-run-missing percent=100 write=no" in owner
assert "readExact(kConfigPath, &loaded)" in owner
assert "readExact(kBackupPath, &loaded)" in owner
assert "verify(kTempPath, percent)" in owner
assert "verify(kConfigPath, percent)" in owner
assert "SD.rename(kConfigPath, kBackupPath)" in owner
assert "SD.rename(kTempPath, kConfigPath)" in owner
assert "SD.rename(kBackupPath, kConfigPath)" in owner
assert "if (!EspNativeConfig_saveBrightness(after))" in menu
assert menu.index("EspNativeConfig_saveBrightness(after)") < menu.index("Esp32Backlight_setPercent(after)")
assert "SDL_RenderPresent(NULL)" in menu

with tempfile.TemporaryDirectory(prefix="native-cyd-config-") as d:
    exe = Path(d) / "codec"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-I", str(INC),
                    str(SRC / "esp_native_config_codec.c"),
                    str(ROOT / "ESP32/test/native_config_codec_host.c"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


# Compile and execute the *real* SD owner against tiny deterministic Arduino/SD
# doubles. This covers boot with no config, corrupt files, backup rescue,
# rollback on failed write/promote and persistence across simulated restarts.
arduino_stub = r"""
#pragma once
struct SerialStub {
    void println(const char*) {}
    template <typename... Args> void printf(const char*, Args...) {}
};
extern SerialStub Serial;
"""
sd_stub = r"""
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#define FILE_READ 0
#define FILE_WRITE 1
class File {
public:
    std::string path;
    size_t cursor = 0;
    bool valid = false;
    bool writable = false;
    explicit operator bool() const { return valid; }
    size_t size() const;
    size_t read(uint8_t*, size_t);
    size_t write(const uint8_t*, size_t);
    void flush() {}
    void close() { valid = false; }
};
class SDClass {
public:
    std::map<std::string, std::vector<uint8_t>> bytes;
    bool failWrites = false;
    bool failPromote = false;
    bool exists(const char*) const;
    File open(const char*, int);
    bool remove(const char*);
    bool rename(const char*, const char*);
};
extern SDClass SD;
"""
storage_host = r"""
#include <Arduino.h>
#include <SD.h>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include "esp_native_config.h"
#include "esp_native_config_codec.h"
#include "esp_native_backlight.h"

SerialStub Serial;
SDClass SD;
static uint8_t pwm = 100U;
extern "C" int Esp32Backlight_isReady(void) { return 1; }
extern "C" uint8_t Esp32Backlight_percent(void) { return pwm; }
extern "C" int Esp32Backlight_setPercent(uint8_t percent) {
    if (percent != 25U && percent != 50U &&
        percent != 75U && percent != 100U) return 0;
    pwm = percent;
    return 1;
}
bool SDClass::exists(const char* path) const {
    return bytes.find(path) != bytes.end();
}
File SDClass::open(const char* path, int mode) {
    File f;
    if (mode == FILE_READ && !exists(path)) return f;
    if (mode == FILE_WRITE) bytes[path].clear();
    f.path = path;
    f.valid = true;
    f.writable = mode == FILE_WRITE;
    return f;
}
bool SDClass::remove(const char* path) {
    return bytes.erase(path) == 1;
}
bool SDClass::rename(const char* oldPath, const char* newPath) {
    if (failPromote &&
        std::strcmp(oldPath, "/DoomRPG-ESP32.cfg.tmp") == 0) return false;
    auto it = bytes.find(oldPath);
    if (it == bytes.end() || exists(newPath)) return false;
    bytes[newPath] = it->second;
    bytes.erase(it);
    return true;
}
size_t File::size() const { return SD.bytes.at(path).size(); }
size_t File::read(uint8_t* out, size_t count) {
    if (!valid || writable) return 0;
    const auto& data = SD.bytes.at(path);
    const size_t remaining = data.size() - cursor;
    if (count > remaining) count = remaining;
    std::memcpy(out, data.data()+cursor, count);
    cursor += count;
    return count;
}
size_t File::write(const uint8_t* data, size_t count) {
    if (!valid || !writable || SD.failWrites) return 0;
    auto& target = SD.bytes[path];
    target.insert(target.end(), data, data+count);
    return count;
}
static uint8_t config(const char* path) {
    const auto it = SD.bytes.find(path);
    assert(it != SD.bytes.end());
    uint8_t percent = 0U;
    assert(EspNativeConfigCodec_decodeBrightness(
        it->second.data(), (uint32_t)it->second.size(), &percent));
    return percent;
}
int main() {
    constexpr const char* mainCfg = "/DoomRPG-ESP32.cfg";
    constexpr const char* tmpCfg = "/DoomRPG-ESP32.cfg.tmp";
    constexpr const char* bakCfg = "/DoomRPG-ESP32.cfg.bak";

    /* First boot is NOT an error and does not create a default file. */
    assert(SD.bytes.empty());
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 100 && SD.bytes.empty());

    assert(EspNativeConfig_saveBrightness(75));
    assert(pwm == 100); /* persistence never changes hardware itself */
    assert(SD.bytes.size() == 1 && config(mainCfg) == 75);
    assert(Esp32Backlight_setPercent(75));
    pwm = 100; /* power cycle */
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 75);

    /* Writing identical setting must not trigger a needless SD transaction. */
    assert(EspNativeConfig_saveBrightness(75));
    assert(SD.bytes.size() == 1);

    SD.failWrites = true;
    assert(!EspNativeConfig_saveBrightness(50));
    SD.failWrites = false;
    assert(config(mainCfg) == 75 && !SD.exists(tmpCfg));

    SD.failPromote = true;
    assert(!EspNativeConfig_saveBrightness(50));
    SD.failPromote = false;
    assert(config(mainCfg) == 75 && !SD.exists(bakCfg));

    assert(EspNativeConfig_saveBrightness(50));
    assert(config(mainCfg) == 50 && SD.bytes.size() == 1);

    /* Invalid primary plus valid backup: recover, and never destroy the
     * last good copy when the next promotion fails. */
    SD.bytes[bakCfg] = SD.bytes[mainCfg];
    SD.bytes[mainCfg][5] ^= 1U;
    pwm = 100;
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 50);
    SD.failPromote = true;
    assert(!EspNativeConfig_saveBrightness(25));
    SD.failPromote = false;
    assert(!SD.exists(mainCfg) && config(bakCfg) == 50);
    pwm = 100;
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 50);
    assert(EspNativeConfig_saveBrightness(25));
    assert(config(mainCfg) == 25 && !SD.exists(bakCfg));

    /* A torn or incompatible primary has an honest default, never a
     * half-parsed 8-bit brightness; stale .tmp must not be trusted. */
    SD.bytes[mainCfg].resize(7);
    pwm = 100;
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 100);
    SD.bytes.clear();
    uint8_t blob[ESP_NATIVE_CONFIG_BYTES];
    assert(EspNativeConfigCodec_encodeBrightness(75, blob));
    SD.bytes[tmpCfg].assign(blob, blob+sizeof(blob));
    EspNativeConfig_loadAtBoot(1);
    assert(pwm == 100 && !SD.exists(mainCfg));

    SD.bytes.clear();
    EspNativeConfig_loadAtBoot(0);
    assert(pwm == 100 && SD.bytes.empty());
    assert(!EspNativeConfig_saveBrightness(75));

    std::puts("PASS native CYD SD config simulated first boot/restore/backup/rollback/corrupt/no SD");
}
"""
with tempfile.TemporaryDirectory(prefix="native-cyd-config-sd-") as d:
    folder = Path(d)
    (folder / "Arduino.h").write_text(arduino_stub)
    (folder / "SD.h").write_text(sd_stub)
    (folder / "storage_host.cpp").write_text(storage_host)
    exe = folder / "storage"
    subprocess.run(["c++", "-std=c++17", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-I", str(folder), "-I", str(INC),
                    str(SRC / "esp_native_config_codec.c"),
                    str(SRC / "esp_native_config.cpp"),
                    str(folder / "storage_host.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print("PASS native CYD config: desktop reader disconnected, first-run defaults, SD commit before PWM")
