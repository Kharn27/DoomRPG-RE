#!/usr/bin/env python3
"""SDL-F1: exercise real Arduino-millis and fatal-Serial native process owner.

The host replaces only Arduino's external clock and Serial hardware, not the
EspNativeProcess_* methods under test. Clock epoch/wrap parity is separately
checked against desktop in doomrpg_process_clock_host.py.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_native_process_services.cpp"
HEADER = (ROOT / "ESP32/include/esp_native_process_services.h").read_text()
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
CLOCK = (ROOT / "ESP32/src/esp_doomrpg_process_clock.c").read_text()

assert 'EspNativeProcess_fatalMessage("DoomRPG Error", errMsg);' in CORE
assert "char errMsg[256];" in CORE and "vsnprintf(errMsg, sizeof(errMsg), fmt, ap);" in CORE
assert "DoomRPG_FreeAppData(doomRpg);" in CORE
assert "exit(0);" in CORE
assert "SDL_ShowMessageBox(&messageboxdata, NULL);" not in CORE
assert CLOCK.count("EspNativeProcess_ticksMs()") == 2
assert "SDL_GetTicks()" not in CLOCK
assert "uint32_t EspNativeProcess_ticksMs(void);" in HEADER
assert "void EspNativeProcess_fatalMessage(" in HEADER

ARDUINO = r"""
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
extern uint32_t mockTicks;
extern std::string serialCapture;
uint32_t millis();
struct HostSerial {
    template<typename... Args> void printf(const char* format, Args... args) const {
        char buf[512];
        const int n = std::snprintf(buf, sizeof(buf), format, args...);
        if (n > 0) serialCapture.append(buf);
    }
};
extern HostSerial Serial;
"""

HARNESS = r"""
#include <Arduino.h>
#include "esp_native_process_services.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <initializer_list>
uint32_t mockTicks = 0;
std::string serialCapture;
HostSerial Serial;
uint32_t millis() { return mockTicks; }
int main() {
    for (uint32_t now : {0U, 1U, 123456U, 0xfffffffeU, 0xffffffffU, 0U, 7U}) {
        mockTicks = now;
        assert(EspNativeProcess_ticksMs() == now);
    }
    EspNativeProcess_fatalMessage("DoomRPG Error", "file: no PAK");
    assert(serialCapture == "[DOOM ERROR] DoomRPG Error: file: no PAK\n");
    serialCapture.clear();
    EspNativeProcess_fatalMessage("DoomRPG Error", "");
    assert(serialCapture == "[DOOM ERROR] DoomRPG Error: \n");
    serialCapture.clear();
    EspNativeProcess_fatalMessage(nullptr, nullptr);
    assert(serialCapture == "[DOOM ERROR] : \n");
    std::puts("SDL-F1 PASS: real native millis and fatal Serial owner, no allocation");
}
"""

def main():
    with tempfile.TemporaryDirectory(prefix="sdl-f1-process-") as d:
        tmp = Path(d)
        (tmp / "Arduino.h").write_text(ARDUINO)
        (tmp / "harness.cpp").write_text(HARNESS)
        exe = tmp / "native-process-test"
        subprocess.run(
            shlex.split(os.environ.get("CXX", "g++")) +
            ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
             "-I", str(tmp), "-I", str(ROOT / "ESP32/include"),
             str(tmp / "harness.cpp"), str(SOURCE), "-o", str(exe)],
            check=True, cwd=ROOT,
        )
        subprocess.run([str(exe)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
