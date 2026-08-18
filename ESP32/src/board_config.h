#pragma once

#include <Arduino.h>

namespace cyd {

constexpr uint8_t kDisplayRotation = 1;
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// XPT2046 touchscreen. It is deliberately driven in software because the two
// hardware SPI controllers are reserved for the TFT and SD card.
constexpr uint8_t kTouchMosi = 32;
constexpr uint8_t kTouchMiso = 39;
constexpr uint8_t kTouchClock = 25;
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchIrq = 36;

// Measured on this CYD in landscape rotation 1. Values are clamped so the
// bezel remains usable even when an individual press lands a little outside.
constexpr uint16_t kTouchRawMinX = 372;
constexpr uint16_t kTouchRawMaxX = 3800;
constexpr uint16_t kTouchRawMinY = 331;
constexpr uint16_t kTouchRawMaxY = 3802;

// microSD card on the native VSPI wiring.
constexpr uint8_t kSdMosi = 23;
constexpr uint8_t kSdMiso = 19;
constexpr uint8_t kSdClock = 18;
constexpr uint8_t kSdCs = 5;
constexpr uint32_t kSdFrequency = 20000000UL;

}  // namespace cyd
